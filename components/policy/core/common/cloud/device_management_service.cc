// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char kPostContentType[] = "application/protobuf";

// Number of times to retry on ERR_NETWORK_CHANGED errors.
static_assert(DeviceManagementService::kMaxRetries <
                  static_cast<int>(DMServerRequestSuccess::kRequestFailed),
              "Maximum retries must be less than 10 which is uma sample of "
              "request failed.");

// Delay after first unsuccessful upload attempt. After each additional failure,
// the delay increases exponentially. Can be changed for testing to prevent
// timeouts.
long g_retry_delay_ms = 10000;

bool IsProxyError(int net_error) {
  switch (net_error) {
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_PROXY_AUTH_UNSUPPORTED:
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

  if (net_error == net::OK &&
      response_code == DeviceManagementService::kSuccess &&
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

}  // namespace

// While these are declared as constexpr in the header file, they also need to
// be defined here so that references can be retrieved when needed.  For
// example, setting one of these constants as an argument to base::Bind()
// requires such a reference.
const int DeviceManagementService::kSuccess;
const int DeviceManagementService::kInvalidArgument;
const int DeviceManagementService::kInvalidAuthCookieOrDMToken;
const int DeviceManagementService::kMissingLicenses;
const int DeviceManagementService::kDeviceManagementNotAllowed;
const int DeviceManagementService::kInvalidURL;
const int DeviceManagementService::kInvalidSerialNumber;
const int DeviceManagementService::kDomainMismatch;
const int DeviceManagementService::kDeviceIdConflict;
const int DeviceManagementService::kDeviceNotFound;
const int DeviceManagementService::kPendingApproval;
const int DeviceManagementService::kRequestTooLarge;
const int DeviceManagementService::kConsumerAccountWithPackagedLicense;
const int DeviceManagementService::kInternalServerError;
const int DeviceManagementService::kServiceUnavailable;
const int DeviceManagementService::kPolicyNotFound;
const int DeviceManagementService::kDeprovisioned;
const int DeviceManagementService::kArcDisabled;

// static
std::string DeviceManagementService::JobConfiguration::GetJobTypeAsString(
    JobType type) {
  switch (type) {
    case DeviceManagementService::JobConfiguration::TYPE_INVALID:
      return "Invalid";
    case DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT:
      return "AutoEnrollment";
    case DeviceManagementService::JobConfiguration::TYPE_REGISTRATION:
      return "Registration";
    case DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH:
      return "ApiAuthCodeFetch";
    case DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH:
      return "PolicyFetch";
    case DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION:
      return "Unregistration";
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE:
      return "UploadCertificate";
    case DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL:
      return "DeviceStateRetrieval";
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS:
      return "UploadStatus";
    case DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS:
      return "RemoteCommands";
    case DeviceManagementService::JobConfiguration::
        TYPE_ATTRIBUTE_UPDATE_PERMISSION:
      return "AttributeUpdatePermission";
    case DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE:
      return "AttributeUpdate";
    case DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE:
      return "GcmIdUpdate";
    case DeviceManagementService::JobConfiguration::
        TYPE_ANDROID_MANAGEMENT_CHECK:
      return "AndroidManagementCheck";
    case DeviceManagementService::JobConfiguration::
        TYPE_CERT_BASED_REGISTRATION:
      return "CertBasedRegistration";
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER:
      return "ActiveDirectoryEnrollPlayUser";
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_PLAY_ACTIVITY:
      return "ActiveDirectoryPlayActivity";
    case DeviceManagementService::JobConfiguration::TYPE_REQUEST_LICENSE_TYPES:
      return "RequestLicenseTypes";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_APP_INSTALL_REPORT:
      return "UploadAppInstallReport";
    case DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT:
      return "TokenEnrollment";
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT:
      return "ChromeDesktopReport";
    case DeviceManagementService::JobConfiguration::
        TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL:
      return "InitialEnrollmentStateRetrieval";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_POLICY_VALIDATION_REPORT:
      return "UploadPolicyValidationReport";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_REAL_TIME_REPORT:
      return "UploadrealtimeReport";
    case DeviceManagementService::JobConfiguration::TYPE_REQUEST_SAML_URL:
      return "PublicSamlUserRequest";
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT:
      return "ChromeOsUserReport";
  }
  NOTREACHED() << "Invalid job type " << type;
  return "";
}

JobConfigurationBase::JobConfigurationBase(
    JobType type,
    std::unique_ptr<DMAuth> auth_data,
    base::Optional<std::string> oauth_token,
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : type_(type),
      factory_(factory),
      auth_data_(std::move(auth_data)),
      oauth_token_(std::move(oauth_token)) {
  CHECK(auth_data_ || oauth_token_);
  CHECK(!auth_data_->has_oauth_token()) << "Use |oauth_token| instead";

  if (oauth_token_)
    AddParameter(dm_protocol::kParamOAuthToken, *oauth_token_);
}

JobConfigurationBase::~JobConfigurationBase() {}

JobConfigurationBase::JobType JobConfigurationBase::GetType() {
  return type_;
}

const JobConfigurationBase::ParameterMap&
JobConfigurationBase::GetQueryParams() {
  return query_params_;
}

void JobConfigurationBase::AddParameter(const std::string& name,
                                        const std::string& value) {
  query_params_[name] = value;
}

scoped_refptr<network::SharedURLLoaderFactory>
JobConfigurationBase::GetUrlLoaderFactory() {
  return factory_;
}

net::NetworkTrafficAnnotationTag
JobConfigurationBase::GetTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("device_management_service", R"(
    semantics {
      sender: "Cloud Policy"
      description:
        "Communication with the Cloud Policy backend, used to check for "
        "the existence of cloud policy for the signed-in account, and to "
        "load/update cloud policy if it exists.  Also used to send reports, "
        "both desktop batch reports and real-time reports."
      trigger:
        "Sign in to Chrome, enroll for Chrome Browser Cloud Management, "
        "periodic refreshes."
      data:
        "During initial signin or device enrollment, auth data is sent up "
        "as part of registration. After initial signin/enrollment, if the "
        "session or device is managed, a unique device or profile ID is "
        "sent with every future request. Other diagnostic information can be "
        "sent up for managed sessions, including which users have used the "
        "device, device hardware status, connected networks, CPU usage, etc."
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
}

std::unique_ptr<network::ResourceRequest>
JobConfigurationBase::GetResourceRequest(bool bypass_proxy, int last_error) {
  auto rr = std::make_unique<network::ResourceRequest>();

  // Build the URL for the request, including parameters.
  GURL url = GetURL(last_error);
  for (ParameterMap::const_iterator entry(query_params_.begin());
       entry != query_params_.end(); ++entry) {
    url = net::AppendQueryParameter(url, entry->first, entry->second);
  }

  rr->url = url;
  rr->method = "POST";
  rr->load_flags =
      net::LOAD_DISABLE_CACHE | (bypass_proxy ? net::LOAD_BYPASS_PROXY : 0);
  rr->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // If auth data is specified, use it to build the request.
  if (auth_data_) {
    if (!auth_data_->gaia_token().empty()) {
      rr->headers.SetHeader(
          dm_protocol::kAuthHeader,
          std::string(dm_protocol::kServiceTokenAuthHeaderPrefix) +
              auth_data_->gaia_token());
    }
    if (!auth_data_->dm_token().empty()) {
      rr->headers.SetHeader(dm_protocol::kAuthHeader,
                            std::string(dm_protocol::kDMTokenAuthHeaderPrefix) +
                                auth_data_->dm_token());
    }
    if (!auth_data_->enrollment_token().empty()) {
      rr->headers.SetHeader(
          dm_protocol::kAuthHeader,
          std::string(dm_protocol::kEnrollmentTokenAuthHeaderPrefix) +
              auth_data_->enrollment_token());
    }
  }

  return rr;
}

// A device management service job implementation.
class DeviceManagementService::JobImpl : public Job, public JobControl {
 public:
  JobImpl(DeviceManagementService* service,
          std::unique_ptr<JobConfiguration> config)
      : service_(service), config_(std::move(config)) {}
  ~JobImpl() override { service_->RemoveJob(this); }

 private:
  // JobControl interface.
  JobConfiguration* GetConfiguration() override { return config_.get(); }
  base::WeakPtr<JobControl> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  std::unique_ptr<network::SimpleURLLoader> CreateFetcher() override;

  RetryMethod OnURLLoadComplete(const std::string& response_body,
                                const std::string& mime_type,
                                int net_error,
                                int response_code,
                                bool was_fetched_via_proxy,
                                int* retry_delay) override;
  RetryMethod ShouldRetry(const std::string& mime_type,
                          int response_code,
                          int net_error,
                          bool was_fetched_via_proxy);
  int GetRetryDelay(RetryMethod method);

  DeviceManagementService* service_;
  std::unique_ptr<JobConfiguration> config_;
  bool bypass_proxy_ = false;

  // Number of times that this job has been retried due to connection errors.
  int retries_count_ = 0;

  // Network error code passed of last call to OnURLLoadComplete().
  int last_error_ = 0;

  base::WeakPtrFactory<JobControl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JobImpl);
};

std::unique_ptr<network::SimpleURLLoader>
DeviceManagementService::JobImpl::CreateFetcher() {
  auto rr = config_->GetResourceRequest(bypass_proxy_, last_error_);
  auto annotation = config_->GetTrafficAnnotationTag();
  auto fetcher = network::SimpleURLLoader::Create(std::move(rr), annotation);
  fetcher->AttachStringForUpload(config_->GetPayload(), kPostContentType);
  fetcher->SetAllowHttpErrorResults(true);
  return fetcher;
}

DeviceManagementService::JobImpl::RetryMethod
DeviceManagementService::JobImpl::OnURLLoadComplete(
    const std::string& response_body,
    const std::string& mime_type,
    int net_error,
    int response_code,
    bool was_fetched_via_proxy,
    int* retry_delay) {
  RetryMethod retry_method =
      ShouldRetry(mime_type, response_code, net_error, was_fetched_via_proxy);
  if (retry_method != RetryMethod::NO_RETRY) {
    config_->OnBeforeRetry();
    *retry_delay = GetRetryDelay(retry_method);
    return retry_method;
  }

  *retry_delay = 0;

  std::string uma_name = config_->GetUmaName();
  if (net_error != net::OK) {
    // Using histogram functions which allows runtime histogram name.
    base::UmaHistogramEnumeration(uma_name,
                                  DMServerRequestSuccess::kRequestFailed);
    LOG(WARNING) << "Request failed, error: " << net_error << " Type: "
                 << JobConfiguration::GetJobTypeAsString(config_->GetType());
    config_->OnURLLoadComplete(this, net_error, response_code, std::string());
    return RetryMethod::NO_RETRY;
  }

  if (response_code != kSuccess) {
    base::UmaHistogramEnumeration(uma_name,
                                  DMServerRequestSuccess::kRequestError);
  } else {
    // Success with retries_count_ retries.
    base::UmaHistogramExactLinear(
        uma_name, retries_count_,
        static_cast<int>(DMServerRequestSuccess::kMaxValue) + 1);
  }

  config_->OnURLLoadComplete(this, net_error, response_code, response_body);
  return NO_RETRY;
}

DeviceManagementService::JobImpl::RetryMethod
DeviceManagementService::JobImpl::ShouldRetry(const std::string& mime_type,
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
    if (config_->GetType() ==
        DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH) {
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

int DeviceManagementService::JobImpl::GetRetryDelay(RetryMethod method) {
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

DeviceManagementService::~DeviceManagementService() {
  // All running jobs should have been cancelled by now.
  DCHECK(pending_jobs_.empty());
  DCHECK(queued_jobs_.empty());
}

std::unique_ptr<DeviceManagementService::Job>
DeviceManagementService::CreateJob(std::unique_ptr<JobConfiguration> config) {
  std::unique_ptr<JobImpl> job =
      std::make_unique<JobImpl>(this, std::move(config));
  AddJob(job.get());
  std::unique_ptr<DeviceManagementService::Job> ret(job.release());
  return ret;
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

  StartQueuedJobs();
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
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(configuration_);
}

void DeviceManagementService::StartJob(JobControl* job) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<network::SimpleURLLoader> fetcher = job->CreateFetcher();
  fetcher->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      job->GetConfiguration()->GetUrlLoaderFactory().get(),
      base::BindOnce(&DeviceManagementService::OnURLLoaderComplete,
                     base::Unretained(this), fetcher.get()));

  pending_jobs_[fetcher.release()] = job;
}

void DeviceManagementService::StartJobAfterDelay(
    base::WeakPtr<JobControl> job) {
  // Check if the job still exists (it is possible that it had been canceled
  // while we were waiting for the retry).
  if (job) {
    StartJob(job.get());
  }
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
    was_fetched_via_proxy =
        url_loader->ResponseInfo()->proxy_server.is_valid() &&
        !url_loader->ResponseInfo()->proxy_server.is_direct();
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

  JobControl* job = entry->second;
  pending_jobs_.erase(entry);

  int delay;
  JobControl::RetryMethod retry_method =
      job->OnURLLoadComplete(response_body, mime_type, net_error, response_code,
                             was_fetched_via_proxy, &delay);
  if (retry_method != JobControl::NO_RETRY) {
    LOG(WARNING) << "Dmserver request failed, retrying in " << delay / 1000
                 << "s.";
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeviceManagementService::StartJobAfterDelay,
                       weak_ptr_factory_.GetWeakPtr(), job->GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay));
  }

  delete url_loader;
}

network::SimpleURLLoader*
DeviceManagementService::GetSimpleURLLoaderForTesting() {
  DCHECK_EQ(1u, pending_jobs_.size());
  return const_cast<network::SimpleURLLoader*>(pending_jobs_.begin()->first);
}

void DeviceManagementService::AddJob(JobControl* job) {
  if (initialized_)
    StartJob(job);
  else
    queued_jobs_.push_back(job);
}

void DeviceManagementService::RemoveJob(JobControl* job) {
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

base::WeakPtr<DeviceManagementService> DeviceManagementService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeviceManagementService::StartQueuedJobs() {
  DCHECK(initialized_);
  while (!queued_jobs_.empty()) {
    StartJob(queued_jobs_.front());
    queued_jobs_.pop_front();
  }
}

void DeviceManagementService::RequeueJobForTesting(JobControl* job) {
  DCHECK(initialized_);
  queued_jobs_.push_back(job);
}

}  // namespace policy
