// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/user_cloud_signin_restriction_policy_fetcher.h"

#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {

namespace {

constexpr char kSecondaryGoogleAccountUsageLatencyHistogramName[] =
    "Enterprise.SecondaryGoogleAccountUsage.PolicyFetch.ResponseLatency";
constexpr char kSecondaryAccountAllowedInArcLatencyHistogramName[] =
    "Arc.Policy.SecondaryAccountAllowedInArc.TimeDelta";
constexpr char kSecondaryAccountAllowedInArcStatusHistogramName[] =
    "Arc.Policy.SecondaryAccountAllowedInArc.Status";
constexpr char kSecondaryAccountAllowedInArcValueHistogramName[] =
    "Arc.Policy.SecondaryAccountAllowedInArc.Value";
const char kAuthorizationHeaderFormat[] = "Bearer %s";
const char kSecureConnectApiGetSecondaryGoogleAccountUsageUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryGoogleAccountUsage";
const char kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryAccountAllowedInArcPolicy";
const char kJsonContentType[] = "application/json";
// Presence of this key in the user info response indicates whether the user is
// on a hosted domain.
const char kHostedDomainKey[] = "hd";
constexpr net::NetworkTrafficAnnotationTag kAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "managed_acccount_signin_restrictions_secure_connect_chromeos",
        R"(
    semantics {
      sender: "Chrome OS sign-in restrictions"
      description:
        "A request to the SecureConnect API to retrieve the value of "
        "SecondaryAccountAllowedInArcPolicy and SecondaryGoogleAccountUsage "
        "policies for the signed in user."
      trigger:
        "After a user signs into a managed account as a secondary account in "
        "Chrome OS."
      data:
        "Gaia access token."
      destination: GOOGLE_OWNED_SERVICE
      internal {
        contacts {
          email: "chromeos-commercial-identity@google.com"
        }
      }
      user_data {
        type: ACCESS_TOKEN
      }
      last_reviewed: "2024-06-04"
    }
    policy {
      cookies_allowed: NO
      setting: "This feature cannot be disabled by settings."
      policy_exception_justification:
          "No policy. The operation is only triggered by a user signing "
          "into a managed account, and there is no policy to opt out."
    })");

std::unique_ptr<network::SimpleURLLoader> CreateUrlLoader(
    const GURL& url,
    const std::string& access_token,
    const net::NetworkTrafficAnnotationTag& annotation) {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kJsonContentType);

  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          annotation);
}

}  // namespace

// static
const char UserCloudSigninRestrictionPolicyFetcher::
    kSecondaryGoogleAccountUsagePolicyValueAll[] = "all";
// static
const char UserCloudSigninRestrictionPolicyFetcher::
    kSecondaryGoogleAccountUsagePolicyValuePrimaryAccountSignin[] =
        "primary_account_signin";

UserCloudSigninRestrictionPolicyFetcher::
    UserCloudSigninRestrictionPolicyFetcher(
        const std::string& email,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : email_(email), url_loader_factory_(url_loader_factory) {
  DCHECK(url_loader_factory_);
  user_info_fetcher_ =
      std::make_unique<policy::UserInfoFetcher>(this, url_loader_factory_);
}

UserCloudSigninRestrictionPolicyFetcher::
    ~UserCloudSigninRestrictionPolicyFetcher() = default;

void UserCloudSigninRestrictionPolicyFetcher::GetSecondaryGoogleAccountUsage(
    std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher,
    PolicyInfoCallback callback) {
  CHECK(access_token_fetcher);
  CHECK(callback);
  CHECK(!callback_) << "A request is already in progress";
  callback_ = std::move(callback);
  if (!signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          email_)) {
    // Non Enterprise accounts do not have restrictions.
    FinalizeResult(Status::kUnsupportedAccountTypeError,
                   /*policy=*/std::nullopt);
    return;
  }
  access_token_fetcher_ = std::move(access_token_fetcher);
  FetchAccessToken();
}

void UserCloudSigninRestrictionPolicyFetcher::
    GetSecondaryAccountAllowedInArcPolicy(
        std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher,
        PolicyInfoCallbackForArc callback) {
  CHECK(access_token_fetcher);
  CHECK(callback);
  CHECK(!callback_for_arc_) << "A request is already in progress";
  callback_for_arc_ = std::move(callback);
  access_token_fetcher_ = std::move(access_token_fetcher);
  FetchAccessToken();
}

void UserCloudSigninRestrictionPolicyFetcher::FetchAccessToken() {
  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      {GaiaConstants::kGoogleUserInfoEmail,
       GaiaConstants::kGoogleUserInfoProfile,
       GaiaConstants::kSecureConnectOAuth2Scope});
}

void UserCloudSigninRestrictionPolicyFetcher::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  access_token_ = token_response.access_token;
  FetchUserInfo();
}

void UserCloudSigninRestrictionPolicyFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(b/223628330): Implement retry strategy.
  LOG(ERROR) << "Failed to fetch access token for consumer: "
             << GetConsumerName() << " with error: " << error.ToString();
  FinalizeResult(Status::kGetTokenError, /*policy=*/std::nullopt);
}

bool UserCloudSigninRestrictionPolicyFetcher::IsFetchingArcPolicy() const {
  return !callback_for_arc_.is_null();
}

std::string UserCloudSigninRestrictionPolicyFetcher::GetConsumerName() const {
  return "signin_restriction_policy_fetcher";
}

void UserCloudSigninRestrictionPolicyFetcher::FetchUserInfo() {
  user_info_fetcher_->Start(access_token_);
}

void UserCloudSigninRestrictionPolicyFetcher::OnGetUserInfoSuccess(
    const base::Value::Dict& user_info) {
  // No need to check for hosted domain when fetching value of
  // SecondaryAccountAllowedInArcPolicy since call to fetch its value is only
  // made after establishing that SecondaryGoogleAccountUsage is called on a
  // hosted domain
  if (!IsFetchingArcPolicy()) {
    const std::string* hosted_domain = user_info.FindString(kHostedDomainKey);
    if (!hosted_domain) {
      // Non Enterprise accounts do not have restrictions.
      DVLOG(1) << "User account is not an Enterprise account";
      FinalizeResult(Status::kUnsupportedAccountTypeError,
                     /*policy=*/std::nullopt);
      return;
    } else {
      hosted_domain_ = *hosted_domain;
    }
  }
  GetSecondaryGoogleAccountUsageInternal();
}

void UserCloudSigninRestrictionPolicyFetcher::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed to fetch user info: " << error.ToString();
  FinalizeResult(Status::kGetUserInfoError, /*policy=*/std::nullopt);
}

void UserCloudSigninRestrictionPolicyFetcher::
    GetSecondaryGoogleAccountUsageInternal() {
  policy_fetch_start_time_ = base::TimeTicks::Now();
  // Each url loader can only be used for one request.
  DCHECK(!url_loader_);
  url_loader_ =
      CreateUrlLoader(GURL(GetSecureConnectApiGetAccountSigninRestrictionUrl()),
                      access_token_, kAnnotation);
  // base::Unretained is safe here because `url_loader_` is owned by `this`.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UserCloudSigninRestrictionPolicyFetcher::
                         OnSecondaryGoogleAccountUsageResult,
                     base::Unretained(this)),
      1024 * 1024 /* 1 MiB */);
}

void UserCloudSigninRestrictionPolicyFetcher::FinalizeResult(
    Status status,
    std::optional<std::string> policy) {
  if (callback_for_arc_.is_null() && callback_.is_null()) {
    LOG(ERROR) << "Callback is missing. Result dropped.";
    return;
  }

  if (IsFetchingArcPolicy()) {
    std::optional<bool> policy_value = std::nullopt;
    if (policy) {
      policy_value = policy.value() == "true";
      base::UmaHistogramBoolean(kSecondaryAccountAllowedInArcValueHistogramName,
                                policy_value.value());
    }
    base::UmaHistogramEnumeration(
        kSecondaryAccountAllowedInArcStatusHistogramName, status);
    std::move(callback_for_arc_).Run(status, policy_value);
  } else {
    std::move(callback_).Run(status, policy, hosted_domain_);
  }
}

void UserCloudSigninRestrictionPolicyFetcher::
    OnSecondaryGoogleAccountUsageResult(
        std::unique_ptr<std::string> response_body) {
  const char* histogram_name =
      IsFetchingArcPolicy() ? kSecondaryAccountAllowedInArcLatencyHistogramName
                            : kSecondaryGoogleAccountUsageLatencyHistogramName;
  base::UmaHistogramMediumTimes(
      histogram_name, base::TimeTicks::Now() - policy_fetch_start_time_);
  std::optional<std::string> restriction;
  const std::string policy_name = IsFetchingArcPolicy()
                                      ? "SecondaryAccountAllowedInArcPolicy"
                                      : "SecondaryGoogleAccountUsage";
  Status status = Status::kUnknownError;
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
  std::optional<int> response_code;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  // Check for network or HTTP errors.
  if (url_loader->NetError() != net::OK || !response_body) {
    if (response_code) {
      LOG(ERROR) << policy_name
                 << " request "
                    "failed with HTTP code: "
                 << response_code.value();
      status = Status::kHttpError;
    } else {
      error =
          GoogleServiceAuthError::FromConnectionError(url_loader->NetError());
      LOG(ERROR) << policy_name
                 << " request "
                    "failed with error: "
                 << url_loader->NetError();
      status = Status::kNetworkError;
    }
    FinalizeResult(status, restriction);
    return;
  }

  if (error.state() == GoogleServiceAuthError::NONE) {
    auto result = base::JSONReader::Read(*response_body, base::JSON_PARSE_RFC);
    const std::string* policy_value =
        result ? result->GetDict().FindString("policyValue") : nullptr;

    if (policy_value) {
      restriction = *policy_value;
      status = Status::kSuccess;
    } else {
      LOG(ERROR) << "Failed to parse " << policy_name << " response";
      status = Status::kParsingResponseError;
    }
  }
  FinalizeResult(status, restriction);
}

std::string UserCloudSigninRestrictionPolicyFetcher::
    GetSecureConnectApiGetAccountSigninRestrictionUrl() const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(policy::switches::kSecureConnectApiUrl)) {
    return command_line->GetSwitchValueASCII(
        policy::switches::kSecureConnectApiUrl);
  }

  return IsFetchingArcPolicy()
             ? kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyUrl
             : kSecureConnectApiGetSecondaryGoogleAccountUsageUrl;
}

}  // namespace ash
