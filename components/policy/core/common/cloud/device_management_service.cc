// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kPostContentType[] = "application/protobuf";

const char kAuthHeader[] = "Authorization";
const char kServiceTokenAuthHeaderPrefix[] = "GoogleLogin auth=";
const char kDMTokenAuthHeaderPrefix[] = "GoogleDMToken token=";
const char kEnrollmentTokenAuthHeaderPrefix[] = "GoogleEnrollmentToken token=";

// Number of times to retry on ERR_NETWORK_CHANGED errors.
const int kMaxRetries = 3;

// HTTP Error Codes of the DM Server with their concrete meanings in the context
// of the DM Server communication.
const int kSuccess = 200;
const int kInvalidArgument = 400;
const int kInvalidAuthCookieOrDMToken = 401;
const int kMissingLicenses = 402;
const int kDeviceManagementNotAllowed = 403;
const int kInvalidURL = 404;  // This error is not coming from the GFE.
const int kInvalidSerialNumber = 405;
const int kDomainMismatch = 406;
const int kDeviceIdConflict = 409;
const int kDeviceNotFound = 410;
const int kPendingApproval = 412;
const int kConsumerAccountWithPackagedLicense = 417;
const int kInternalServerError = 500;
const int kServiceUnavailable = 503;
const int kPolicyNotFound = 902;
const int kDeprovisioned = 903;
const int kArcDisabled = 904;

// Delay after first unsuccessful upload attempt. After each additional failure,
// the delay increases exponentially. Can be changed for testing to prevent
// timeouts.
long g_retry_delay_ms = 10000;

bool IsProxyError(int net_error) {
  switch (net_error) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_HTTPS_PROXY_TUNNEL_RESPONSE:
    case net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED:
    case net::ERR_PROXY_CERTIFICATE_INVALID:
    case net::ERR_SOCKS_CONNECTION_FAILED:
    case net::ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      return true;
  }
  return false;
}

bool IsConnectionError(int net_error) {
  switch (net_error) {
    case net::ERR_NETWORK_CHANGED:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_CONNECTION_TIMED_OUT:
    case net::ERR_NAME_RESOLUTION_FAILED:
      return true;
  }
  return false;
}

bool IsProtobufMimeType(const std::string& mime_type) {
  return mime_type == "application/x-protobuffer";
}

bool FailedWithProxy(const std::string& mime_type,
                     int response_code,
                     int net_error,
                     bool was_fetched_via_proxy) {
  if (IsProxyError(net_error)) {
    LOG(WARNING) << "Proxy failed while contacting dmserver.";
    return true;
  }

  if (net_error == net::OK && response_code == kSuccess &&
      was_fetched_via_proxy && !IsProtobufMimeType(mime_type)) {
    // The proxy server can be misconfigured but pointing to an existing
    // server that replies to requests. Try to recover if a successful
    // request that went through a proxy returns an unexpected mime type.
    LOG(WARNING) << "Got bad mime-type in response from dmserver that was "
                 << "fetched via a proxy.";
    return true;
  }

  return false;
}

const char* JobTypeToRequestType(DeviceManagementRequestJob::JobType type) {
  switch (type) {
    case DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT:
      return dm_protocol::kValueRequestAutoEnrollment;
    case DeviceManagementRequestJob::TYPE_REGISTRATION:
      return dm_protocol::kValueRequestRegister;
    case DeviceManagementRequestJob::TYPE_POLICY_FETCH:
      return dm_protocol::kValueRequestPolicy;
    case DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH:
      return dm_protocol::kValueRequestApiAuthorization;
    case DeviceManagementRequestJob::TYPE_UNREGISTRATION:
      return dm_protocol::kValueRequestUnregister;
    case DeviceManagementRequestJob::TYPE_UPLOAD_CERTIFICATE:
      return dm_protocol::kValueRequestUploadCertificate;
    case DeviceManagementRequestJob::TYPE_DEVICE_STATE_RETRIEVAL:
      return dm_protocol::kValueRequestDeviceStateRetrieval;
    case DeviceManagementRequestJob::TYPE_UPLOAD_STATUS:
      return dm_protocol::kValueRequestUploadStatus;
    case DeviceManagementRequestJob::TYPE_REMOTE_COMMANDS:
      return dm_protocol::kValueRequestRemoteCommands;
    case DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE_PERMISSION:
      return dm_protocol::kValueRequestDeviceAttributeUpdatePermission;
    case DeviceManagementRequestJob::TYPE_ATTRIBUTE_UPDATE:
      return dm_protocol::kValueRequestDeviceAttributeUpdate;
    case DeviceManagementRequestJob::TYPE_GCM_ID_UPDATE:
      return dm_protocol::kValueRequestGcmIdUpdate;
    case DeviceManagementRequestJob::TYPE_ANDROID_MANAGEMENT_CHECK:
      return dm_protocol::kValueRequestCheckAndroidManagement;
    case DeviceManagementRequestJob::TYPE_CERT_BASED_REGISTRATION:
      return dm_protocol::kValueRequestCertBasedRegister;
    case DeviceManagementRequestJob::TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER:
      return dm_protocol::kValueRequestActiveDirectoryEnrollPlayUser;
    case DeviceManagementRequestJob::TYPE_ACTIVE_DIRECTORY_PLAY_ACTIVITY:
      return dm_protocol::kValueRequestActiveDirectoryPlayActivity;
    case DeviceManagementRequestJob::TYPE_REQUEST_LICENSE_TYPES:
      return dm_protocol::kValueRequestCheckDeviceLicense;
    case DeviceManagementRequestJob::TYPE_UPLOAD_APP_INSTALL_REPORT:
      return dm_protocol::kValueRequestAppInstallReport;
    case DeviceManagementRequestJob::TYPE_TOKEN_ENROLLMENT:
      return dm_protocol::kValueRequestTokenEnrollment;
    case DeviceManagementRequestJob::TYPE_CHROME_DESKTOP_REPORT:
      return dm_protocol::kValueRequestChromeDesktopReport;
    case DeviceManagementRequestJob::TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL:
      return dm_protocol::kValueRequestInitialEnrollmentStateRetrieval;
    case DeviceManagementRequestJob::TYPE_UPLOAD_POLICY_VALIDATION_REPORT:
      return dm_protocol::kValueRequestUploadPolicyValidationReport;
  }
  NOTREACHED() << "Invalid job type " << type;
  return "";
}

}  // namespace

// Request job implementation used with DeviceManagementService.
class DeviceManagementRequestJobImpl : public DeviceManagementRequestJob {
 public:
  DeviceManagementRequestJobImpl(
      JobType type,
      const std::string& agent_parameter,
      const std::string& platform_parameter,
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~DeviceManagementRequestJobImpl() override;

  // Handles the URL request response.
  void HandleResponse(int net_error,
                      int response_code,
                      const std::string& data);

  // Gets the URL to contact.
  GURL GetURL(const std::string& server_url);

  // Configures the headers and flags.
  void ConfigureRequest(network::ResourceRequest* resource_request);

  // Attaches the payload.
  void AddPayload(network::SimpleURLLoader* loader);

  enum RetryMethod {
    // No retry required for this request.
    NO_RETRY,
    // Should retry immediately (no delay).
    RETRY_IMMEDIATELY,
    // Should retry after a delay.
    RETRY_WITH_DELAY
  };

  // Returns if and how this job should be retried.
  RetryMethod ShouldRetry(const std::string& mime_type,
                          int response_code,
                          int net_error,
                          bool was_fetched_via_proxy);

  // Returns the delay before the next retry with the specified RetryMethod.
  int GetRetryDelay(RetryMethod method);

  // Invoked right before retrying this job.
  void PrepareRetry();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return url_loader_factory_;
  }

  // Get weak pointer
  base::WeakPtr<DeviceManagementRequestJobImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // DeviceManagementRequestJob:
  void Run() override;

 private:
  // Invokes the callback with the given error code.
  void ReportError(DeviceManagementStatus code);

  // Pointer to the service this job is associated with.
  DeviceManagementService* service_;

  // Whether the BYPASS_PROXY flag should be set by ConfigureRequest().
  bool bypass_proxy_;

  // Number of times that this job has been retried due to connection errors.
  int retries_count_;

  // The last error why we had to retry.
  int last_error_ = 0;

  // The URLLoaderFactory to use for this job.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to get notified if the job has been canceled while waiting for retry.
  base::WeakPtrFactory<DeviceManagementRequestJobImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRequestJobImpl);
};

// Used in the Enterprise.DMServerRequestSuccess histogram, shows how many
// retries we had to do to execute the DeviceManagementRequestJob.
enum DMServerRequestSuccess {
  // No retries happened, the request succeeded for the first try.
  REQUEST_NO_RETRY = 0,

  // 1..kMaxRetries: number of retries

  // The request failed (too many retries or non-retriable error).
  REQUEST_FAILED = 10,
  // The server responded with an error.
  REQUEST_ERROR,

  REQUEST_MAX
};

DeviceManagementRequestJobImpl::DeviceManagementRequestJobImpl(
    JobType type,
    const std::string& agent_parameter,
    const std::string& platform_parameter,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : DeviceManagementRequestJob(type, agent_parameter, platform_parameter),
      service_(service),
      bypass_proxy_(false),
      retries_count_(0),
      url_loader_factory_(url_loader_factory),
      weak_ptr_factory_(this) {}

DeviceManagementRequestJobImpl::~DeviceManagementRequestJobImpl() {
  service_->RemoveJob(this);
}

void DeviceManagementRequestJobImpl::Run() {
  service_->AddJob(this);
}

void DeviceManagementRequestJobImpl::HandleResponse(int net_error,
                                                    int response_code,
                                                    const std::string& data) {
  if (net_error != net::OK) {
    UMA_HISTOGRAM_ENUMERATION("Enterprise.DMServerRequestSuccess",
                              DMServerRequestSuccess::REQUEST_FAILED,
                              DMServerRequestSuccess::REQUEST_MAX);
    LOG(WARNING) << "DMServer request failed, error: " << net_error;
    em::DeviceManagementResponse dummy_response;
    callback_.Run(DM_STATUS_REQUEST_FAILED, net_error, dummy_response);
    return;
  }

  if (response_code != kSuccess) {
    UMA_HISTOGRAM_ENUMERATION("Enterprise.DMServerRequestSuccess",
                              DMServerRequestSuccess::REQUEST_ERROR,
                              DMServerRequestSuccess::REQUEST_MAX);
    LOG(WARNING) << "DMServer sent an error response: " << response_code;
  } else {
    // Success with retries_count_ retries.
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Enterprise.DMServerRequestSuccess", retries_count_,
        static_cast<int>(DMServerRequestSuccess::REQUEST_MAX));
  }

  switch (response_code) {
    case kSuccess: {
      em::DeviceManagementResponse response;
      if (!response.ParseFromString(data)) {
        ReportError(DM_STATUS_RESPONSE_DECODING_ERROR);
        return;
      }
      callback_.Run(DM_STATUS_SUCCESS, net::OK, response);
      return;
    }
    case kInvalidArgument:
      ReportError(DM_STATUS_REQUEST_INVALID);
      return;
    case kInvalidAuthCookieOrDMToken:
      ReportError(DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID);
      return;
    case kMissingLicenses:
      ReportError(DM_STATUS_SERVICE_MISSING_LICENSES);
      return;
    case kDeviceManagementNotAllowed:
      ReportError(DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED);
      return;
    case kPendingApproval:
      ReportError(DM_STATUS_SERVICE_ACTIVATION_PENDING);
      return;
    case kConsumerAccountWithPackagedLicense:
      ReportError(DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE);
      return;
    case kInvalidURL:
    case kInternalServerError:
    case kServiceUnavailable:
      ReportError(DM_STATUS_TEMPORARY_UNAVAILABLE);
      return;
    case kDeviceNotFound:
      ReportError(DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
      return;
    case kPolicyNotFound:
      ReportError(DM_STATUS_SERVICE_POLICY_NOT_FOUND);
      return;
    case kInvalidSerialNumber:
      ReportError(DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER);
      return;
    case kDomainMismatch:
      ReportError(DM_STATUS_SERVICE_DOMAIN_MISMATCH);
      return;
    case kDeprovisioned:
      ReportError(DM_STATUS_SERVICE_DEPROVISIONED);
      return;
    case kDeviceIdConflict:
      ReportError(DM_STATUS_SERVICE_DEVICE_ID_CONFLICT);
      return;
    case kArcDisabled:
      ReportError(DM_STATUS_SERVICE_ARC_DISABLED);
      return;
    default:
      // Handle all unknown 5xx HTTP error codes as temporary and any other
      // unknown error as one that needs more time to recover.
      if (response_code >= 500 && response_code <= 599)
        ReportError(DM_STATUS_TEMPORARY_UNAVAILABLE);
      else
        ReportError(DM_STATUS_HTTP_STATUS_ERROR);
      return;
  }
}

GURL DeviceManagementRequestJobImpl::GetURL(
    const std::string& server_url) {
  std::string result(server_url);
  result += '?';
  ParameterMap current_query_params(query_params_);
  if (last_error_ == 0) {
    // Not a retry.
    current_query_params.push_back(
        std::make_pair(dm_protocol::kParamRetry, "false"));
  } else {
    current_query_params.push_back(
        std::make_pair(dm_protocol::kParamRetry, "true"));
    current_query_params.push_back(std::make_pair(dm_protocol::kParamLastError,
                                                  std::to_string(last_error_)));
  }
  for (ParameterMap::const_iterator entry(current_query_params.begin());
       entry != current_query_params.end(); ++entry) {
    if (entry != current_query_params.begin())
      result += '&';
    result += net::EscapeQueryParamValue(entry->first, true);
    result += '=';
    result += net::EscapeQueryParamValue(entry->second, true);
  }
  return GURL(result);
}

void DeviceManagementRequestJobImpl::ConfigureRequest(
    network::ResourceRequest* resource_request) {
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DISABLE_CACHE | (bypass_proxy_ ? net::LOAD_BYPASS_PROXY : 0);
  CHECK(auth_data_);
  if (!auth_data_->gaia_token().empty()) {
    resource_request->headers.SetHeader(
        kAuthHeader,
        std::string(kServiceTokenAuthHeaderPrefix) + auth_data_->gaia_token());
  }
  if (!auth_data_->dm_token().empty()) {
    resource_request->headers.SetHeader(
        kAuthHeader,
        std::string(kDMTokenAuthHeaderPrefix) + auth_data_->dm_token());
  }
  if (!auth_data_->enrollment_token().empty()) {
    resource_request->headers.SetHeader(
        kAuthHeader, std::string(kEnrollmentTokenAuthHeaderPrefix) +
                         auth_data_->enrollment_token());
  }
}

void DeviceManagementRequestJobImpl::AddPayload(
    network::SimpleURLLoader* loader) {
  std::string payload;
  CHECK(request_.SerializeToString(&payload));
  loader->AttachStringForUpload(payload, kPostContentType);
}

DeviceManagementRequestJobImpl::RetryMethod
DeviceManagementRequestJobImpl::ShouldRetry(const std::string& mime_type,
                                            int response_code,
                                            int net_error,
                                            bool was_fetched_via_proxy) {
  last_error_ = net_error;
  if (!bypass_proxy_ && FailedWithProxy(mime_type, response_code, net_error,
                                        was_fetched_via_proxy)) {
    // Retry the job immediately if it failed due to a broken proxy, by
    // bypassing the proxy on the next try.
    bypass_proxy_ = true;
    return RETRY_IMMEDIATELY;
  }

  // Early device policy fetches on ChromeOS and Auto-Enrollment checks are
  // often interrupted during ChromeOS startup when network is not yet ready.
  // Allowing the fetcher to retry once after that is enough to recover; allow
  // it to retry up to 3 times just in case.
  if (IsConnectionError(net_error) && retries_count_ < kMaxRetries) {
    ++retries_count_;
    if (type_ == DeviceManagementRequestJob::TYPE_POLICY_FETCH) {
      // We must not delay when retrying policy fetch, because it is a blocking
      // call when logging in.
      return RETRY_IMMEDIATELY;
    } else {
      return RETRY_WITH_DELAY;
    }
  }

  // The request didn't fail, or the limit of retry attempts has been reached;
  // forward the result to the job owner.
  return NO_RETRY;
}

int DeviceManagementRequestJobImpl::GetRetryDelay(RetryMethod method) {
  switch (method) {
    case RETRY_WITH_DELAY:
      return g_retry_delay_ms << (retries_count_ - 1);
    case RETRY_IMMEDIATELY:
      return 0;
    default:
      NOTREACHED();
      return 0;
  }
}

void DeviceManagementRequestJobImpl::PrepareRetry() {
  if (!retry_callback_.is_null())
    retry_callback_.Run(this);
}

void DeviceManagementRequestJobImpl::ReportError(DeviceManagementStatus code) {
  em::DeviceManagementResponse dummy_response;
  callback_.Run(code, net::OK, dummy_response);
}

DeviceManagementRequestJob::~DeviceManagementRequestJob() {}

void DeviceManagementRequestJob::SetAuthData(std::unique_ptr<DMAuth> auth) {
  auth_data_ = std::move(auth);
  if (auth_data_->has_oauth_token())
    AddParameter(dm_protocol::kParamOAuthToken, auth_data_->oauth_token());
}

void DeviceManagementRequestJob::SetClientID(const std::string& client_id) {
  AddParameter(dm_protocol::kParamDeviceID, client_id);
}

void DeviceManagementRequestJob::SetCritical(bool critical) {
  if (critical)
    AddParameter(dm_protocol::kParamCritical, "true");
}

em::DeviceManagementRequest* DeviceManagementRequestJob::GetRequest() {
  return &request_;
}

DeviceManagementRequestJob::DeviceManagementRequestJob(
    JobType type,
    const std::string& agent_parameter,
    const std::string& platform_parameter)
    : type_(type) {
  AddParameter(dm_protocol::kParamRequest, JobTypeToRequestType(type));
  AddParameter(dm_protocol::kParamDeviceType, dm_protocol::kValueDeviceType);
  AddParameter(dm_protocol::kParamAppType, dm_protocol::kValueAppType);
  AddParameter(dm_protocol::kParamAgent, agent_parameter);
  AddParameter(dm_protocol::kParamPlatform, platform_parameter);
}

void DeviceManagementRequestJob::SetRetryCallback(
    const RetryCallback& retry_callback) {
  retry_callback_ = retry_callback;
}

void DeviceManagementRequestJob::Start(const Callback& callback) {
  callback_ = callback;
  Run();
}

void DeviceManagementRequestJob::AddParameter(const std::string& name,
                                              const std::string& value) {
  query_params_.push_back(std::make_pair(name, value));
}

DeviceManagementService::~DeviceManagementService() {
  // All running jobs should have been cancelled by now.
  DCHECK(pending_jobs_.empty());
  DCHECK(queued_jobs_.empty());
}

DeviceManagementRequestJob* DeviceManagementService::CreateJob(
    DeviceManagementRequestJob::JobType type,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return new DeviceManagementRequestJobImpl(
      type, configuration_->GetAgentParameter(),
      configuration_->GetPlatformParameter(), this, url_loader_factory);
}

void DeviceManagementService::ScheduleInitialization(
    int64_t delay_milliseconds) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (initialized_)
    return;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceManagementService::Initialize,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void DeviceManagementService::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (initialized_)
    return;
  initialized_ = true;

  while (!queued_jobs_.empty()) {
    StartJob(queued_jobs_.front());
    queued_jobs_.pop_front();
  }
}

void DeviceManagementService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  weak_ptr_factory_.InvalidateWeakPtrs();
  for (auto job(pending_jobs_.begin()); job != pending_jobs_.end(); ++job) {
    delete job->first;
    queued_jobs_.push_back(job->second);
  }
  pending_jobs_.clear();
}

DeviceManagementService::DeviceManagementService(
    std::unique_ptr<Configuration> configuration)
    : configuration_(std::move(configuration)),
      initialized_(false),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DCHECK(configuration_);
}

void DeviceManagementService::StartJob(DeviceManagementRequestJobImpl* job) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GURL url = job->GetURL(GetServerUrl());
  DCHECK(url.is_valid()) << "Maybe invalid --device-management-url was passed?";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_management_service", R"(
        semantics {
          sender: "Cloud Policy"
          description:
            "Communication with the Cloud Policy backend, used to check for "
            "the existence of cloud policy for the signed-in account, and to "
            "load/update cloud policy if it exists."
          trigger:
            "Sign in to Chrome, also periodic refreshes."
          data:
            "During initial signin or device enrollment, auth data is sent up "
            "as part of registration. After initial signin/enrollment, if the "
            "session or device is managed, a unique device or profile ID is "
            "sent with every future request. On Chrome OS, other diagnostic "
            "information can be sent up for managed sessions, including which "
            "users have used the device, device hardware status, connected "
            "networks, CPU usage, etc."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be controlled by Chrome settings, but users "
            "can sign out of Chrome to disable it."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  resource_request->method = "POST";
  job->ConfigureRequest(resource_request.get());
  network::SimpleURLLoader* fetcher =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation)
          .release();
  job->AddPayload(fetcher);
  fetcher->SetAllowHttpErrorResults(true);
  fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      job->url_loader_factory().get(),
      base::BindOnce(&DeviceManagementService::OnURLLoaderComplete,
                     base::Unretained(this), fetcher));

  pending_jobs_[fetcher] = job;
}

void DeviceManagementService::StartJobAfterDelay(
    base::WeakPtr<DeviceManagementRequestJobImpl> job) {
  // Check if the job still exists (it is possible that it had been canceled
  // while we were waiting for the retry).
  if (job) {
    StartJob(job.get());
  }
}

std::string DeviceManagementService::GetServerUrl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return configuration_->GetServerUrl();
}

// static
void DeviceManagementService::SetRetryDelayForTesting(long retry_delay_ms) {
  CHECK_GE(retry_delay_ms, 0);
  g_retry_delay_ms = retry_delay_ms;
}

void DeviceManagementService::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  bool was_fetched_via_proxy = false;
  std::string mime_type;
  if (url_loader->ResponseInfo()) {
    was_fetched_via_proxy = url_loader->ResponseInfo()->was_fetched_via_proxy;
    mime_type = url_loader->ResponseInfo()->mime_type;
    if (url_loader->ResponseInfo()->headers)
      response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  std::string response_body_str;
  if (response_body.get())
    response_body_str = std::move(*response_body.get());

  OnURLLoaderCompleteInternal(url_loader, response_body_str, mime_type,
                              url_loader->NetError(), response_code,
                              was_fetched_via_proxy);
}

void DeviceManagementService::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const std::string& response_body,
    const std::string& mime_type,
    int net_error,
    int response_code,
    bool was_fetched_via_proxy) {
  auto entry(pending_jobs_.find(url_loader));
  if (entry == pending_jobs_.end()) {
    NOTREACHED() << "Callback from foreign URL loader";
    return;
  }

  DeviceManagementRequestJobImpl* job = entry->second;
  pending_jobs_.erase(entry);

  DeviceManagementRequestJobImpl::RetryMethod retry_method = job->ShouldRetry(
      mime_type, response_code, net_error, was_fetched_via_proxy);
  if (retry_method != DeviceManagementRequestJobImpl::RetryMethod::NO_RETRY) {
    job->PrepareRetry();
    int delay = job->GetRetryDelay(retry_method);
    LOG(WARNING) << "Dmserver request failed, retrying in " << delay / 1000
                 << "s.";
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeviceManagementService::StartJobAfterDelay,
                       weak_ptr_factory_.GetWeakPtr(), job->GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay));
  } else {
    job->HandleResponse(net_error, response_code, response_body);
  }
  delete url_loader;
}

network::SimpleURLLoader*
DeviceManagementService::GetSimpleURLLoaderForTesting() {
  DCHECK_EQ(1u, pending_jobs_.size());
  return const_cast<network::SimpleURLLoader*>(pending_jobs_.begin()->first);
}

void DeviceManagementService::AddJob(DeviceManagementRequestJobImpl* job) {
  if (initialized_)
    StartJob(job);
  else
    queued_jobs_.push_back(job);
}

void DeviceManagementService::RemoveJob(DeviceManagementRequestJobImpl* job) {
  for (auto entry(pending_jobs_.begin()); entry != pending_jobs_.end();
       ++entry) {
    if (entry->second == job) {
      delete entry->first;
      pending_jobs_.erase(entry);
      return;
    }
  }

  const JobQueue::iterator elem =
      std::find(queued_jobs_.begin(), queued_jobs_.end(), job);
  if (elem != queued_jobs_.end())
    queued_jobs_.erase(elem);
}

}  // namespace policy
