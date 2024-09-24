// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/device_management_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Proxy failed while contacting dmserver.";
    return true;
  }

  if (net_error == net::OK &&
      response_code == DeviceManagementService::kSuccess &&
      was_fetched_via_proxy && !IsProtobufMimeType(mime_type)) {
    // The proxy server can be misconfigured but pointing to an existing
    // server that replies to requests. Try to recover if a successful
    // request that went through a proxy returns an unexpected mime type.
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Got bad mime-type in response from dmserver that was "
        << "fetched via a proxy.";
    return true;
  }

  return false;
}

std::string ResponseCodeToString(int response_code) {
  switch (response_code) {
    case DeviceManagementService::kSuccess:
      return "Success";
    case DeviceManagementService::kInvalidArgument:
      return "InvalidArgument";
    case DeviceManagementService::kInvalidAuthCookieOrDMToken:
      return "InvalidAuthCookieOrDMToken";
    case DeviceManagementService::kMissingLicenses:
      return "MissingLicenses";
    case DeviceManagementService::kDeviceManagementNotAllowed:
      return "DeviceManagementNotAllowed";
    case DeviceManagementService::kInvalidURL:
      return "InvalidURL";
    case DeviceManagementService::kInvalidSerialNumber:
      return "InvalidSerialNumber";
    case DeviceManagementService::kDomainMismatch:
      return "DomainMismatch";
    case DeviceManagementService::kDeviceIdConflict:
      return "DeviceIdConflict";
    case DeviceManagementService::kDeviceNotFound:
      return "DeviceNotFound";
    case DeviceManagementService::kPendingApproval:
      return "PendingApproval";
    case DeviceManagementService::kRequestTooLarge:
      return "RequestTooLarge";
    case DeviceManagementService::kConsumerAccountWithPackagedLicense:
      return "ConsumerAccountWithPackagedLicense";
    case DeviceManagementService::kTooManyRequests:
      return "TooManyRequests";
    case DeviceManagementService::kInternalServerError:
      return "InternalServerError";
    case DeviceManagementService::kServiceUnavailable:
      return "ServiceUnavailable";
    case DeviceManagementService::kPolicyNotFound:
      return "PolicyNotFound";
    case DeviceManagementService::kDeprovisioned:
      return "Deprovisioned";
    case DeviceManagementService::kArcDisabled:
      return "ArcDisabled";
    case DeviceManagementService::kInvalidDomainlessCustomer:
      return "InvalidDomainlessCustomer";
    case DeviceManagementService::kTosHasNotBeenAccepted:
      return "TosHasNotBeenAccepted";
    case DeviceManagementService::kIllegalAccountForPackagedEDULicense:
      return "IllegalAccountForPackagedEDULicense";
    case DeviceManagementService::kInvalidPackagedDeviceForKiosk:
      return "InvalidPackagedDeviceForKiosk";
  }

  return base::NumberToString(response_code);
}

}  // namespace

// While these are declared as constexpr in the header file, they also need to
// be defined here so that references can be retrieved when needed.  For
// example, setting one of these constants as an argument to base::BindOnce()
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
const int DeviceManagementService::kTooManyRequests;
const int DeviceManagementService::kInternalServerError;
const int DeviceManagementService::kServiceUnavailable;
const int DeviceManagementService::kPolicyNotFound;
const int DeviceManagementService::kDeprovisioned;
const int DeviceManagementService::kArcDisabled;
const int DeviceManagementService::kInvalidDomainlessCustomer;
const int DeviceManagementService::kTosHasNotBeenAccepted;
const int DeviceManagementService::kIllegalAccountForPackagedEDULicense;
const int DeviceManagementService::kInvalidPackagedDeviceForKiosk;

// static
std::string DeviceManagementService::JobConfiguration::GetJobTypeAsString(
    JobType type) {
  // Please also update EnterpriseDMServerRequest in
  // tools/metrics/histograms/metadata/enterprise/histograms.xml when updating
  // this.
  //
  // Please keep sorted by returned string (case-insensitive).
  switch (type) {
    case DeviceManagementService::JobConfiguration::
        TYPE_ANDROID_MANAGEMENT_CHECK:
      return "AndroidManagementCheck";
    case DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH:
      return "ApiAuthCodeFetch";
    case DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE:
      return "AttributeUpdate";
    case DeviceManagementService::JobConfiguration::
        TYPE_ATTRIBUTE_UPDATE_PERMISSION:
      return "AttributeUpdatePermission";
    case DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT:
      return "AutoEnrollment";
    case DeviceManagementService::JobConfiguration::
        TYPE_BROWSER_UPLOAD_PUBLIC_KEY:
      return "BrowserUploadPublicKey";
    case DeviceManagementService::JobConfiguration::
        TYPE_CERT_BASED_REGISTRATION:
      return "CertBasedRegistration";
    case DeviceManagementService::JobConfiguration::
        TYPE_CERT_PROVISIONING_REQUEST:
      return "CertProvisioningRequest";
    case DeviceManagementService::JobConfiguration::TYPE_CHECK_USER_ACCOUNT:
      return "CheckUserAccount";
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT:
      return "ChromeDesktopReport";
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT:
      return "ChromeOsUserReport";
    case DeviceManagementService::JobConfiguration::TYPE_CHROME_PROFILE_REPORT:
      return "ChromeProfileReport";
    case DeviceManagementService::JobConfiguration::TYPE_DEVICE_STATE_RETRIEVAL:
      return "DeviceStateRetrieval";
    case DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE:
      return "GcmIdUpdate";
    case DeviceManagementService::JobConfiguration::
        TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL:
      return "InitialEnrollmentStateRetrieval";
    case DeviceManagementService::JobConfiguration::TYPE_INVALID:
      return "Invalid";
    case DeviceManagementService::JobConfiguration::TYPE_OIDC_REGISTRATION:
      return "OidcRegistration";
    case DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH:
      return "PolicyFetch";
    case DeviceManagementService::JobConfiguration::
        TYPE_PSM_HAS_DEVICE_STATE_REQUEST:
      return "PSMDeviceStateRequest";
    case DeviceManagementService::JobConfiguration::TYPE_REQUEST_SAML_URL:
      return "PublicSamlUserRequest";
    case DeviceManagementService::JobConfiguration::TYPE_REGISTRATION:
      return "Registration";
    case DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS:
      return "RemoteCommands";
    // Type TOKEN_ENROLLMENT was renamed to BROWSER_REGISTRATION when device
    // token-based enrollment was added, but unfortunately we have to keep the
    // stringified job type as "TokenEnrollment" because this string defines
    // an UMA metric.
    case DeviceManagementService::JobConfiguration::TYPE_BROWSER_REGISTRATION:
      return "TokenEnrollment";
    case DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION:
      return "Unregistration";
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE:
      return "UploadCertificate";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_ENCRYPTED_REPORT:
      return "UploadEncryptedReport";
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_EUICC_INFO:
      return "UploadEuiccInfo";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_POLICY_VALIDATION_REPORT:
      return "UploadPolicyValidationReport";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_REAL_TIME_REPORT:
      return "UploadrealtimeReport";
    case DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS:
      return "UploadStatus";
    case DeviceManagementService::JobConfiguration::
        TYPE_TOKEN_BASED_DEVICE_REGISTRATION:
      return "TokenBasedDeviceRegistration";
    case DeviceManagementService::JobConfiguration::
        TYPE_UPLOAD_FM_REGISTRATION_TOKEN:
      return "UploadFmRegistrationToken";
    case DeviceManagementService::JobConfiguration::
        TYPE_POLICY_AGENT_REGISTRATION:
      return "PolicyAgentRegistration";
    // TODO(b/263367348): Remove the Active Directory types below, after they're
    // removed from the corresponding enum.
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER:
    case DeviceManagementService::JobConfiguration::
        TYPE_ACTIVE_DIRECTORY_PLAY_ACTIVITY:
      NOTREACHED_IN_MIGRATION() << "Invalid job type: " << type;
      return "";
  }
}

JobConfigurationBase::JobConfigurationBase(
    JobType type,
    DMAuth auth_data,
    std::optional<std::string> oauth_token,
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : type_(type),
      factory_(factory),
      auth_data_(std::move(auth_data)),
      oauth_token_(std::move(oauth_token)) {
  CHECK(!auth_data_.has_oauth_token()) << "Use |oauth_token| instead";

#if !BUILDFLAG(IS_IOS)
  if (oauth_token_ && auth_data.token_type() != DMAuthTokenType::kOidc) {
    // Put the oauth token in the query parameters for platforms that are not
    // iOS. On iOS we are trying the oauth token in the request headers
    // (crbug.com/1312158). We might want to use the iOS approach on all
    // platforms at some point.
    AddParameter(dm_protocol::kParamOAuthToken, *oauth_token_);
  }
#endif
}

JobConfigurationBase::~JobConfigurationBase() = default;

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

const DMAuth& JobConfigurationBase::GetAuth() const {
  return auth_data_;
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

  rr->url = std::move(url);
  rr->method = "POST";
  rr->load_flags =
      net::LOAD_DISABLE_CACHE | (bypass_proxy ? net::LOAD_BYPASS_PROXY : 0);
  rr->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Disable secure DNS for requests related to device management to allow for
  // recovery in the event of a misconfigured secure DNS policy.
  rr->trusted_params = network::ResourceRequest::TrustedParams();
  rr->trusted_params->disable_secure_dns = true;

#if BUILDFLAG(IS_IOS)
  // Put the oauth token in the request headers on iOS. We might want
  // to use this approach on the other platforms at some point. This approach
  // will be tried first on iOS (crbug.com/1312158). Technically, the
  // DMServer should already be able to handle the oauth token in the
  // request headers, but we prefer to try the approach on iOS first to
  // avoid breaking the other platforms with unexpected issues.
  if (oauth_token_ && !oauth_token_->empty()) {
    rr->headers.SetHeader(dm_protocol::kAuthHeader,
                          base::StrCat({dm_protocol::kOAuthTokenHeaderPrefix,
                                        " ", *oauth_token_}));
  }
#endif

  // If auth data is specified, use it to build the request.
  switch (auth_data_.token_type()) {
    case DMAuthTokenType::kNoAuth:
      break;
    case DMAuthTokenType::kDm:
      rr->headers.SetHeader(dm_protocol::kAuthHeader,
                            std::string(dm_protocol::kDMTokenAuthHeaderPrefix) +
                                auth_data_.dm_token());
      break;
    case DMAuthTokenType::kEnrollment:
      rr->headers.SetHeader(
          dm_protocol::kAuthHeader,
          std::string(dm_protocol::kEnrollmentTokenAuthHeaderPrefix) +
              auth_data_.enrollment_token());
      break;
    case DMAuthTokenType::kOauth:
      // OAuth token is transferred as a HTTP query parameter.
      break;
    case DMAuthTokenType::kOidc:
      // Send OIDC Auth token and ID token in auth header, send profile ID in
      // URL parameter
      rr->headers.SetHeader(
          dm_protocol::kAuthHeader,
          base::StrCat({dm_protocol::kOidcAuthHeaderPrefix,
                        dm_protocol::kOidcAuthTokenHeaderPrefix, *oauth_token_,
                        ",", dm_protocol::kOidcIdTokenHeaderPrefix,
                        auth_data_.oidc_id_token()}));
      break;
  }

  return rr;
}

bool JobConfigurationBase::ShouldRecordUma() const {
  return true;
}

DeviceManagementService::Job::RetryMethod JobConfigurationBase::ShouldRetry(
    int response_code,
    const std::string& response_body) {
  // By default, no need to retry based on the contents of the response.
  return DeviceManagementService::Job::NO_RETRY;
}

std::optional<base::TimeDelta> JobConfigurationBase::GetTimeoutDuration() {
  return timeout_;
}

// A device management service job implementation.
class DeviceManagementService::JobImpl : public Job {
 public:
  JobImpl(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          std::unique_ptr<JobConfiguration> config)
      : config_(std::move(config)), task_runner_(task_runner) {}
  JobImpl(const JobImpl&) = delete;
  JobImpl& operator=(const JobImpl&) = delete;
  ~JobImpl() override = default;

  void Start();
  base::WeakPtr<JobImpl> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  friend class JobForTesting;

  void CreateUrlLoader();

  // Callback for `SimpleURLLoader`. Extracts data from |response_body| and
  // |url_loader_| and passes it on to |OnURLLoaderCompleteInternal|.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Interprets URL loading data and either schedules a retry or hands the data
  // off to |HandleResponseData|.
  RetryMethod OnURLLoaderCompleteInternal(const std::string& response_body,
                                          const std::string& mime_type,
                                          int net_error,
                                          int response_code,
                                          bool was_fetched_via_proxy,
                                          bool is_test = false);
  // Logs failed jobs an jobs that succeeded after retry.
  // Then hands the response data off to |config_|.
  RetryMethod HandleResponseData(const std::string& response_body,
                                 const std::string& mime_type,
                                 int net_error,
                                 int response_code,
                                 bool was_fetched_via_proxy);
  RetryMethod ShouldRetry(const std::string& response_body,
                          const std::string& mime_type,
                          int response_code,
                          int net_error,
                          bool was_fetched_via_proxy);
  int GetRetryDelay(RetryMethod method);

  std::unique_ptr<JobConfiguration> config_;
  bool bypass_proxy_ = false;

  // Number of times that this job has been retried due to connection errors.
  int retries_count_ = 0;

  // Network error code passed of last call to HandleResponseData().
  int last_error_ = 0;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<JobImpl> weak_ptr_factory_{this};
};

void DeviceManagementService::JobImpl::CreateUrlLoader() {
  auto rr = config_->GetResourceRequest(bypass_proxy_, last_error_);
  auto annotation = config_->GetTrafficAnnotationTag();
  url_loader_ = network::SimpleURLLoader::Create(std::move(rr), annotation);
  url_loader_->AttachStringForUpload(config_->GetPayload(), kPostContentType);
  url_loader_->SetAllowHttpErrorResults(true);
  if (config_->GetTimeoutDuration()) {
    url_loader_->SetTimeoutDuration(config_->GetTimeoutDuration().value());
  }
}

void DeviceManagementService::JobImpl::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  bool was_fetched_via_proxy = false;
  std::string mime_type;
  if (url_loader_->ResponseInfo()) {
    was_fetched_via_proxy =
        url_loader_->ResponseInfo()->proxy_chain.IsValid() &&
        !url_loader_->ResponseInfo()->proxy_chain.is_direct();
    mime_type = url_loader_->ResponseInfo()->mime_type;
    if (url_loader_->ResponseInfo()->headers) {
      response_code = url_loader_->ResponseInfo()->headers->response_code();
    }
  }

  std::string response_body_str;
  if (response_body.get()) {
    response_body_str = std::move(*response_body.get());
  }

  OnURLLoaderCompleteInternal(response_body_str, mime_type,
                              url_loader_->NetError(), response_code,
                              was_fetched_via_proxy);
}

DeviceManagementService::Job::RetryMethod
DeviceManagementService::JobImpl::OnURLLoaderCompleteInternal(
    const std::string& response_body,
    const std::string& mime_type,
    int net_error,
    int response_code,
    bool was_fetched_via_proxy,
    bool is_test) {
  RetryMethod retry_method =
      ShouldRetry(response_body, mime_type, response_code, net_error,
                  was_fetched_via_proxy);

  if (retry_method == Job::NO_RETRY) {
    HandleResponseData(response_body, mime_type, net_error, response_code,
                       was_fetched_via_proxy);
    return retry_method;
  }

  config_->OnBeforeRetry(response_code, response_body);
  int retry_delay = GetRetryDelay(retry_method);
  LOG_POLICY(WARNING, CBCM_ENROLLMENT)
      << "Request of type "
      << JobConfiguration::GetJobTypeAsString(config_->GetType())
      << " failed (net_error = " << net_error
      << ", response_code = " << response_code << "), retrying in "
      << retry_delay << "ms.";
  if (!is_test) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeviceManagementService::JobImpl::Start, GetWeakPtr()),
        base::Milliseconds(retry_delay));
  }
  return retry_method;
}

DeviceManagementService::Job::RetryMethod
DeviceManagementService::JobImpl::HandleResponseData(
    const std::string& response_body,
    const std::string& mime_type,
    int net_error,
    int response_code,
    bool was_fetched_via_proxy) {
  if (net_error != net::OK) {
    if (config_->ShouldRecordUma()) {
      // Using histogram functions which allows runtime histogram name.
      base::UmaHistogramEnumeration(config_->GetUmaName(),
                                    DMServerRequestSuccess::kRequestFailed);
    }
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Request of type "
        << JobConfiguration::GetJobTypeAsString(config_->GetType())
        << " failed (net_error = " << net::ErrorToString(net_error) << " ("
        << net_error << ")).";
    config_->OnURLLoadComplete(this, net_error, response_code, std::string());
    return RetryMethod::NO_RETRY;
  }

  if (response_code != kSuccess) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Request of type "
        << JobConfiguration::GetJobTypeAsString(config_->GetType())
        << " failed (response_code = " << ResponseCodeToString(response_code)
        << " (" << response_code << ")).";
    if (config_->ShouldRecordUma()) {
      base::UmaHistogramEnumeration(config_->GetUmaName(),
                                    DMServerRequestSuccess::kRequestError);
    }
  } else {
    // Success with retries_count_ retries.
    if (retries_count_) {
      LOG_POLICY(WARNING, CBCM_ENROLLMENT)
          << "Request of type "
          << JobConfiguration::GetJobTypeAsString(config_->GetType())
          << " succeeded after " << retries_count_ << " retries.";
    }
    if (config_->ShouldRecordUma()) {
      base::UmaHistogramExactLinear(
          config_->GetUmaName(), retries_count_,
          static_cast<int>(DMServerRequestSuccess::kMaxValue) + 1);
    }
  }

  config_->OnURLLoadComplete(this, net_error, response_code, response_body);
  return NO_RETRY;
}

DeviceManagementService::Job::RetryMethod
DeviceManagementService::JobImpl::ShouldRetry(const std::string& response_body,
                                              const std::string& mime_type,
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

  // The request didn't fail, or the limit of retry attempts has been reached.
  // Ask the config if this is a valid response.
  return config_->ShouldRetry(response_code, response_body);
}

int DeviceManagementService::JobImpl::GetRetryDelay(RetryMethod method) {
  switch (method) {
    case RETRY_WITH_DELAY:
      return g_retry_delay_ms << (retries_count_ - 1);
    case RETRY_IMMEDIATELY:
      return 0;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

DeviceManagementService::~DeviceManagementService() = default;

DeviceManagementService::JobForTesting::JobForTesting() = default;
DeviceManagementService::JobForTesting::JobForTesting(JobImpl* job_impl)
    : job_impl_(job_impl ? job_impl->GetWeakPtr() : base::WeakPtr<JobImpl>{}) {}
DeviceManagementService::JobForTesting::JobForTesting(const JobForTesting&) =
    default;
DeviceManagementService::JobForTesting::JobForTesting(
    JobForTesting&&) noexcept = default;
DeviceManagementService::JobForTesting&
DeviceManagementService::JobForTesting::operator=(const JobForTesting&) =
    default;
DeviceManagementService::JobForTesting&
DeviceManagementService::JobForTesting::operator=(JobForTesting&&) noexcept =
    default;
DeviceManagementService::JobForTesting::~JobForTesting() = default;

bool DeviceManagementService::JobForTesting::IsActive() const {
  return job_impl_.get();
}

void DeviceManagementService::JobForTesting::Deactivate() {
  return job_impl_.reset();
}

DeviceManagementService::JobConfiguration*
DeviceManagementService::JobForTesting::GetConfigurationForTesting() const {
  CHECK(IsActive());
  return job_impl_.get()->config_.get();
}

DeviceManagementService::Job::RetryMethod
DeviceManagementService::JobForTesting::SetResponseForTesting(
    int net_error,
    int response_code,
    const std::string& response_body,
    const std::string& mime_type,
    bool was_fetched_via_proxy) {
  CHECK(IsActive());
  return job_impl_.get()->OnURLLoaderCompleteInternal(
      response_body, mime_type, net_error, response_code, was_fetched_via_proxy,
      /*is_test=*/true);
}

std::pair<std::unique_ptr<DeviceManagementService::Job>,
          DeviceManagementService::JobForTesting>
DeviceManagementService::CreateJobForTesting(
    std::unique_ptr<JobConfiguration> config) {
  CHECK(config);
  auto job = std::make_unique<JobImpl>(task_runner_, std::move(config));
  JobForTesting job_for_testing(job.get());  // IN-TEST
  return std::make_pair(std::move(job), std::move(job_for_testing));
}

std::unique_ptr<DeviceManagementService::Job>
DeviceManagementService::CreateJob(std::unique_ptr<JobConfiguration> config) {
  CHECK(config);
  std::unique_ptr<JobImpl> job =
      std::make_unique<JobImpl>(task_runner_, std::move(config));
  AddJob(job.get());
  return job;
}

void DeviceManagementService::ScheduleInitialization(
    int64_t delay_milliseconds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialized_) {
    return;
  }
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceManagementService::Initialize,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(delay_milliseconds));
}

void DeviceManagementService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialized_) {
    return;
  }
  initialized_ = true;

  StartQueuedJobs();
}

void DeviceManagementService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

DeviceManagementService::DeviceManagementService(
    std::unique_ptr<Configuration> configuration)
    : configuration_(std::move(configuration)),
      initialized_(false),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(configuration_);
}

void DeviceManagementService::JobImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CreateUrlLoader();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      config_->GetUrlLoaderFactory().get(),
      base::BindOnce(&DeviceManagementService::JobImpl::OnURLLoaderComplete,
                     GetWeakPtr()));
}

// static
void DeviceManagementService::SetRetryDelayForTesting(long retry_delay_ms) {
  CHECK_GE(retry_delay_ms, 0);
  g_retry_delay_ms = retry_delay_ms;
}

void DeviceManagementService::AddJob(JobImpl* job) {
  if (initialized_) {
    job->Start();
  } else {
    queued_jobs_.push_back(job->GetWeakPtr());
  }
}

base::WeakPtr<DeviceManagementService> DeviceManagementService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeviceManagementService::StartQueuedJobs() {
  DCHECK(initialized_);
  for (auto& job : queued_jobs_) {
    if (job.get()) {
      job.get()->Start();
    }
  }
  queued_jobs_.clear();
}

}  // namespace policy
