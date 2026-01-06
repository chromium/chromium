// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

namespace {

// UMA histograms:
// Histogram for whether the primary account exists.
static constexpr char kEligibilityRequestPrimaryAccountExistsHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountExists";
// Histogram for whether the primary account was found in the cookie jar.
static constexpr char
    kEligibilityRequestPrimaryAccountInCookieJarHistogramName[] =
        "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountInCookieJar";
// Histogram for the eligibility request session index.
static constexpr char kEligibilityRequestPrimaryAccountIndexHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestPrimaryAccountIndex";
// Histogram for the eligibility request status.
static constexpr char kEligibilityRequestStatusHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestStatus";
// Histogram for the eligibility request response code.
static constexpr char kEligibilityRequestResponseCodeHistogramName[] =
    "Omnibox.AimEligibility.EligibilityResponseCode";
// Histogram for the eligibility response source.
static constexpr char kEligibilityResponseSourceHistogramName[] =
    "Omnibox.AimEligibility.EligibilityResponseSource";
// Histogram prefix for the eligibility response.
static constexpr char kEligibilityResponseHistogramPrefix[] =
    "Omnibox.AimEligibility.EligibilityResponse";
// Histogram prefix for changes to the eligibility response.
static constexpr char kEligibilityResponseChangeHistogramPrefix[] =
    "Omnibox.AimEligibility.EligibilityResponseChange";
// Histograms for the number of retries for eligibility requests.
static constexpr char kEligibilityRequestRetriesFailedHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestRetries.Failed";
static constexpr char kEligibilityRequestRetriesSucceededHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestRetries.Succeeded";

static constexpr char kRequestPath[] = "/async/folae";
static constexpr char kRequestQuery[] = "async=_fmt:pb";
static constexpr char kAuthUserQueryKey[] = "authuser";

// Reflects the default value for the `kAIModeSettings` pref; 0 = allowed, 1 =
// disallowed. Pref value is determined by: `AIModeSettings` policy,
// `GenAiDefaultSettings` policy if `AIModeSettings` isn't set, or the default
// pref value (0) if neither policy is set. Do not change this value without
// migrating the existing prefs and the policy's prefs mapping.
constexpr int kAiModeAllowedDefault = 0;

// The maximum number of retries for eligibility requests.
constexpr int kMaxRetries = 3;

// The pref name used for storing the eligibility response proto.
constexpr char kResponsePrefName[] =
    "aim_eligibility_service.aim_eligibility_response";

// Returns a non-empty account info if the primary account exists.
CoreAccountInfo GetPrimaryAccountInfo(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager) {
    return CoreAccountInfo();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
}

// Returns the index of the primary account in the cookie jar or std::nullopt if
// the primary account does not exist or is not found in the cookie jar.
std::optional<size_t> GetSessionIndexForPrimaryAccount(
    signin::IdentityManager* identity_manager) {
  CoreAccountInfo primary_account_info =
      GetPrimaryAccountInfo(identity_manager);
  if (primary_account_info.gaia.empty()) {
    return std::nullopt;
  }

  auto accounts_in_cookie_jar = identity_manager->GetAccountsInCookieJar();
  const auto& accounts = accounts_in_cookie_jar.GetAllAccounts();
  for (size_t i = 0; i < accounts.size(); ++i) {
    if (accounts[i].gaia_id == primary_account_info.gaia) {
      return i;
    }
  }

  return std::nullopt;
}

const net::NetworkTrafficAnnotationTag kRequestTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("aim_eligibility_fetch", R"(
      semantics {
        sender: "Chrome AI Mode Eligibility Service"
        description:
          "Retrieves the set of AI Mode features the client is eligible for "
          "from the server."
        trigger:
          "Requests are made on startup, when user's profile state changes, "
          "and periodically while Chrome is running."
        user_data {
          type: NONE
        }
        data:
          "No request body is sent; this is a GET request with no query params."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts { email: "chrome-desktop-search@google.com" }
        }
        last_reviewed: "2025-08-06"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "Coupled to Google default search."
        policy_exception_justification:
          "Not gated by policy. Setting AIModeSetting to '1' prevents the "
          "response from being used. But Google Chrome still makes the "
          "requests and saves the response to disk so that it's available when "
          "the policy is unset."
      })");

// Parses `response_string` into `response_proto`. Does not modify
// `response_proto` if parsing fails. Returns false on failure.
bool ParseResponseString(const std::string& response_string,
                         omnibox::AimEligibilityResponse* response_proto) {
  omnibox::AimEligibilityResponse proto;
  if (!proto.ParseFromString(response_string)) {
    return false;
  }
  *response_proto = proto;
  return true;
}

// Reads `kResponsePrefName` and parses it into `response_proto`. Does not
// modify `response_proto` if parsing fails. Returns false on failure.
bool GetResponseFromPrefs(const PrefService* prefs,
                          omnibox::AimEligibilityResponse* response_proto) {
  std::string encoded_response = prefs->GetString(kResponsePrefName);
  if (encoded_response.empty()) {
    return false;
  }
  std::string response_string;
  if (!base::Base64Decode(encoded_response, &response_string)) {
    return false;
  }
  if (!ParseResponseString(response_string, response_proto)) {
    return false;
  }
  return true;
}

}  // namespace

// static
bool AimEligibilityService::GenericKillSwitchFeatureCheck(
    const AimEligibilityService* aim_eligibility_service,
    const base::Feature& feature,
    const std::optional<std::reference_wrapper<const base::Feature>>
        feature_en_us) {
  if (!aim_eligibility_service) {
    return false;
  }

  // If not locally eligible, return false.
  if (!aim_eligibility_service->IsAimLocallyEligible()) {
    return false;
  }

  // If the generic feature is overridden, it takes precedence.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list && feature_list->IsFeatureOverridden(feature.name)) {
    return base::FeatureList::IsEnabled(feature);
  }

  // If the server eligibility is enabled, check overall eligibility alone.
  // The service will control locale rollout so there's no need to check locale
  // or the state of kMyFeature below.
  if (aim_eligibility_service->IsServerEligibilityEnabled()) {
    return aim_eligibility_service->IsAimEligible();
  }

  // Otherwise, check the generic entrypoint feature default value.
  return base::FeatureList::IsEnabled(feature) ||
         (feature_en_us &&
          base::FeatureList::IsEnabled(feature_en_us.value()) &&
          aim_eligibility_service->IsLanguage("en") &&
          aim_eligibility_service->IsCountry("us"));
}

// static
void AimEligibilityService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kResponsePrefName, "");
  registry->RegisterIntegerPref(omnibox::kAIModeSettings,
                                kAiModeAllowedDefault);
}

// static
bool AimEligibilityService::IsAimAllowedByPolicy(const PrefService* prefs) {
  return prefs->GetInteger(omnibox::kAIModeSettings) == kAiModeAllowedDefault;
}

// static
std::string AimEligibilityService::EligibilityResponseSourceToString(
    EligibilityResponseSource source) {
  switch (source) {
    case EligibilityResponseSource::kDefault:
      return "Default";
    case EligibilityResponseSource::kPrefs:
      return "Prefs";
    case EligibilityResponseSource::kServer:
      return "Server";
    case EligibilityResponseSource::kBrowserCache:
      return "Browser Cache";
    case EligibilityResponseSource::kUser:
      return "User";
  }
}

AimEligibilityService::AimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_off_the_record)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      is_off_the_record_(is_off_the_record) {
  if (base::FeatureList::IsEnabled(omnibox::kAimEnabled)) {
    Initialize();
  }
}

AimEligibilityService::~AimEligibilityService() {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled)) {
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }
}

bool AimEligibilityService::IsCountry(const std::string& country) const {
  // Country codes are in lowercase ISO 3166-1 alpha-2 format; e.g., us, br, in.
  // See components/variations/service/variations_service.h
  return GetCountryCode() == country;
}

bool AimEligibilityService::IsLanguage(const std::string& language) const {
  // Locale follows BCP 47 format; e.g., en-US, fr-FR, ja-JP.
  // See ui/base/l10n/l10n_util.h
  return base::StartsWith(GetLocale(), language, base::CompareCase::SENSITIVE);
}

base::CallbackListSubscription
AimEligibilityService::RegisterEligibilityChangedCallback(
    base::RepeatingClosure callback) {
  return eligibility_changed_callbacks_.Add(callback);
}

bool AimEligibilityService::IsServerEligibilityEnabled() const {
  return base::FeatureList::IsEnabled(omnibox::kAimServerEligibilityEnabled);
}

bool AimEligibilityService::IsAimAllowedByDse() const {
  return search::DefaultSearchProviderIsGoogle(template_url_service_);
}

bool AimEligibilityService::IsAimLocallyEligible() const {
  // Kill switch: If AIM is completely disabled, return false.
  if (!base::FeatureList::IsEnabled(omnibox::kAimEnabled)) {
    return false;
  }

  // Always check Google DSE and Policy requirements.
  if (!IsAimAllowedByDse() || !IsAimAllowedByPolicy(&pref_service_.get())) {
    return false;
  }

  return true;
}

bool AimEligibilityService::IsAimEligible() const {
  // Check local eligibility first.
  if (!IsAimLocallyEligible()) {
    return false;
  }

  // Conditionally check server response eligibility requirement.
  if (IsServerEligibilityEnabled()) {
    base::UmaHistogramEnumeration(kEligibilityResponseSourceHistogramName,
                                  most_recent_response_source_);
    return most_recent_response_.is_eligible();
  }

  return true;
}

bool AimEligibilityService::IsPdfUploadEligible() const {
  return IsEligibleByServer(most_recent_response_.is_pdf_upload_eligible());
}

bool AimEligibilityService::IsDeepSearchEligible() const {
  return IsEligibleByServer(most_recent_response_.is_deep_search_eligible());
}

bool AimEligibilityService::IsCreateImagesEligible() const {
  if (is_off_the_record_) {
    return false;
  }
  return IsEligibleByServer(
      most_recent_response_.is_image_generation_eligible());
}

bool AimEligibilityService::IsCanvasEligible() const {
  return IsEligibleByServer(most_recent_response_.is_canvas_eligible());
}

const omnibox::AimEligibilityResponse&
AimEligibilityService::GetMostRecentResponse() const {
  return most_recent_response_;
}

AimEligibilityService::EligibilityResponseSource
AimEligibilityService::GetMostRecentResponseSource() const {
  return most_recent_response_source_;
}

void AimEligibilityService::StartServerEligibilityRequestForDebugging() {
  if (!initialized_) {
    return;
  }
  StartServerEligibilityRequest(RequestSource::kUser);
}

bool AimEligibilityService::SetEligibilityResponseForDebugging(
    const std::string& base64_encoded_response) {
  if (!initialized_) {
    return false;
  }
  std::string response_string;
  if (!base::Base64Decode(base64_encoded_response, &response_string)) {
    return false;
  }
  omnibox::AimEligibilityResponse response_proto;
  if (!ParseResponseString(response_string, &response_proto)) {
    return false;
  }
  UpdateMostRecentResponse(response_proto, EligibilityResponseSource::kUser);
  return true;
}

// Private methods -------------------------------------------------------------

// static
std::string AimEligibilityService::RequestSourceToString(RequestSource source) {
  switch (source) {
    case RequestSource::kStartup:
      return "Startup";
    case RequestSource::kCookieChange:
      return "CookieChange";
    case RequestSource::kPrimaryAccountChange:
      return "PrimaryAccountChange";
    case RequestSource::kNetworkChange:
      return "NetworkChange";
    case RequestSource::kUser:
      return "User";
  }
}

bool AimEligibilityService::IsEligibleByServer(bool server_eligibility) const {
  if (!IsAimEligible()) {
    return false;
  }

  if (IsServerEligibilityEnabled()) {
    return server_eligibility;
  }

  return true;
}

void AimEligibilityService::Initialize() {
  // The service should not be initialized if AIM is disabled.
  CHECK(base::FeatureList::IsEnabled(omnibox::kAimEnabled));
  // The service should not be initialized twice.
  CHECK(!initialized_);

  // Always load the most recent response from prefs even if the template
  // URL service is not loaded.
  LoadMostRecentResponse();

  if (!template_url_service_) {
    return;
  }

  if (!template_url_service_->loaded()) {
    template_url_service_subscription_ =
        template_url_service_->RegisterOnLoadedCallback(base::BindOnce(
            &AimEligibilityService::Initialize, weak_factory_.GetWeakPtr()));
    return;
  }

  initialized_ = true;

  pref_change_registrar_.Init(&pref_service_.get());
  pref_change_registrar_.Add(
      kResponsePrefName,
      base::BindRepeating(&AimEligibilityService::OnEligibilityResponseChanged,
                          weak_factory_.GetWeakPtr()));

  bool startup_request_enabled =
      base::FeatureList::IsEnabled(omnibox::kAimServerRequestOnStartupEnabled);
  bool startup_request_delayed_until_network_available_enabled =
      base::FeatureList::IsEnabled(
          omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled);
  bool is_offline = net::NetworkChangeNotifier::IsOffline();

  if (startup_request_enabled &&
      startup_request_delayed_until_network_available_enabled && is_offline) {
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  } else if (startup_request_enabled) {
    startup_request_sent_ = true;
    StartServerEligibilityRequest(RequestSource::kStartup);
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

void AimEligibilityService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnPrimaryAccountChanges.Get()) {
    return;
  }
  // Change to the primary account might affect AIM eligibility.
  // Refresh the server eligibility state.
  StartServerEligibilityRequest(RequestSource::kPrimaryAccountChange);
}

void AimEligibilityService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnCookieJarChanges.Get()) {
    return;
  }
  // Change to the accounts in the cookie jar might affect AIM eligibility.
  // Refresh the server eligibility state.
  StartServerEligibilityRequest(RequestSource::kCookieChange);
}

void AimEligibilityService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  bool startup_request_enabled =
      base::FeatureList::IsEnabled(omnibox::kAimServerRequestOnStartupEnabled);
  bool startup_request_delayed_until_network_available_enabled =
      base::FeatureList::IsEnabled(
          omnibox::kAimStartupRequestDelayedUntilNetworkAvailableEnabled);
  CHECK(startup_request_enabled);
  CHECK(startup_request_delayed_until_network_available_enabled);
  bool is_online = !net::NetworkChangeNotifier::IsOffline();
  if (is_online && !startup_request_sent_) {
    startup_request_sent_ = true;
    StartServerEligibilityRequest(RequestSource::kNetworkChange);
  }
}

void AimEligibilityService::OnEligibilityResponseChanged() {
  CHECK(initialized_);
  eligibility_changed_callbacks_.Notify();
}

void AimEligibilityService::UpdateMostRecentResponse(
    const omnibox::AimEligibilityResponse& response_proto,
    EligibilityResponseSource response_source) {
  CHECK(initialized_);

  // Read the old response from prefs before updating it to log changes below.
  omnibox::AimEligibilityResponse old_response;
  GetResponseFromPrefs(&pref_service_.get(), &old_response);

  // Update the in-memory state before updating the prefs. Writing to prefs may
  // notify subscribers synchronously. This ensures the in-memory state is
  // correct.
  most_recent_response_ = response_proto;
  most_recent_response_source_ = response_source;

  // Update the prefs.
  std::string response_string;
  response_proto.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);
  pref_service_->SetString(kResponsePrefName, encoded_response);

  // Log changes.
  LogEligibilityResponseChanges(old_response, response_proto);
}

void AimEligibilityService::LoadMostRecentResponse() {
  omnibox::AimEligibilityResponse prefs_response;
  if (!GetResponseFromPrefs(&pref_service_.get(), &prefs_response)) {
    return;
  }

  most_recent_response_ = prefs_response;
  most_recent_response_source_ = EligibilityResponseSource::kPrefs;
}

GURL AimEligibilityService::GetRequestUrl(
    RequestSource request_source,
    const TemplateURLService* template_url_service,
    signin::IdentityManager* identity_manager) {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service)) {
    return GURL();
  }

  GURL base_gurl(
      template_url_service->search_terms_data().GoogleBaseURLValue());
  if (!base_gurl.is_valid()) {
    return GURL();
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(kRequestPath);
  replacements.SetQueryStr(kRequestQuery);
  GURL url = base_gurl.ReplaceComponents(replacements);

  // Get the index of the primary account in the cookie jar.
  std::optional<size_t> session_index =
      GetSessionIndexForPrimaryAccount(identity_manager);
  // Log whether the primary account exists, if so whether it was found in the
  // cookie jar, and if so its index in the cookie jar.
  auto primary_account_info = GetPrimaryAccountInfo(identity_manager);
  const bool primary_account_exists = !primary_account_info.gaia.empty();
  LogEligibilityRequestPrimaryAccountExists(primary_account_exists,
                                            request_source);
  if (primary_account_exists) {
    const bool primary_account_in_cookie_jar = session_index.has_value();
    LogEligibilityRequestPrimaryAccountInCookieJar(
        primary_account_in_cookie_jar, request_source);
    if (primary_account_in_cookie_jar) {
      LogEligibilityRequestPrimaryAccountIndex(*session_index, request_source);
      // Add authuser=<primary account session index> if applicable.
      // By default the endpoint uses the first account in the cookie jar to
      // authenticate the request. When the primary account is not set or found
      // in the cookie jar, the endpoint should not assume the first account in
      // the cookie jar is the primary account.
      // TODO(crbug.com/452304766): Find a way to force the endpoint to treat
      // these as signed-out sessions.
      if (base::FeatureList::IsEnabled(
              omnibox::kAimServerEligibilityForPrimaryAccountEnabled)) {
        return net::AppendQueryParameter(url, kAuthUserQueryKey,
                                         base::NumberToString(*session_index));
      }
    }
  }

  return url;
}

void AimEligibilityService::StartServerEligibilityRequest(
    RequestSource request_source) {
  CHECK(initialized_);

  // URLLoaderFactory may be null in tests.
  if (!url_loader_factory_) {
    return;
  }

  // Request URL may be invalid.
  GURL request_url = GetRequestUrl(request_source, template_url_service_.get(),
                                   identity_manager_);
  if (!request_url.is_valid()) {
    return;
  }

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = request_url;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  // Set the SiteForCookies to the request URL's site to avoid cookie blocking.
  request->site_for_cookies = net::SiteForCookies::FromUrl(request->url);
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       kRequestTrafficAnnotation);

  LogEligibilityRequestStatus(EligibilityRequestStatus::kSent, request_source);

  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilityCustomRetryPolicyEnabled)) {
    // Other places in Chrome suggest that DNS and network change related
    // failures are common on startup and use the retry policy below.
    loader->SetRetryOptions(
        kMaxRetries, network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED |
                         network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  }

  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AimEligibilityService::OnServerEligibilityResponse,
                     weak_factory_.GetWeakPtr(), std::move(loader),
                     request_source));
}

void AimEligibilityService::OnServerEligibilityResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    RequestSource request_source,
    std::optional<std::string> response_string) {
  CHECK(initialized_);

  const int response_code =
      loader->ResponseInfo() && loader->ResponseInfo()->headers
          ? loader->ResponseInfo()->headers->response_code()
          : 0;
  const bool was_fetched_via_cache =
      loader->ResponseInfo() ? loader->ResponseInfo()->was_fetched_via_cache
                             : false;
  const EligibilityRequestStatus request_status =
      was_fetched_via_cache ? EligibilityRequestStatus::kSuccessBrowserCache
                            : EligibilityRequestStatus::kSuccess;

  ProcessServerEligibilityResponse(request_source, response_code,
                                   request_status, loader->GetNumRetries(),
                                   std::move(response_string));
}

void AimEligibilityService::ProcessServerEligibilityResponse(
    RequestSource request_source,
    int response_code,
    EligibilityRequestStatus request_status,
    int num_retries,
    std::optional<std::string> response_string) {
  LogEligibilityRequestResponseCode(response_code, request_source);

  const bool custom_retry_policy_enabled = base::FeatureList::IsEnabled(
      omnibox::kAimServerEligibilityCustomRetryPolicyEnabled);

  if (response_code != 200 || !response_string) {
    LogEligibilityRequestStatus(EligibilityRequestStatus::kErrorResponse,
                                request_source);
    if (custom_retry_policy_enabled) {
      base::UmaHistogramExactLinear(
          kEligibilityRequestRetriesFailedHistogramName, num_retries,
          kMaxRetries + 1);
    }
    return;
  }

  if (custom_retry_policy_enabled) {
    base::UmaHistogramExactLinear(
        kEligibilityRequestRetriesSucceededHistogramName, num_retries,
        kMaxRetries + 1);
  }

  omnibox::AimEligibilityResponse response_proto;
  if (!ParseResponseString(*response_string, &response_proto)) {
    LogEligibilityRequestStatus(EligibilityRequestStatus::kFailedToParse,
                                request_source);
    return;
  }

  LogEligibilityRequestStatus(request_status, request_source);
  const EligibilityResponseSource response_source =
      request_status == EligibilityRequestStatus::kSuccessBrowserCache
          ? EligibilityResponseSource::kBrowserCache
          : EligibilityResponseSource::kServer;
  UpdateMostRecentResponse(response_proto, response_source);
  LogEligibilityResponse(request_source);
}

std::string AimEligibilityService::GetHistogramNameSlicedByRequestSource(
    const std::string& histogram_name,
    RequestSource request_source) const {
  return base::StrCat(
      {histogram_name, ".", RequestSourceToString(request_source)});
}

void AimEligibilityService::LogEligibilityRequestPrimaryAccountExists(
    bool exists,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestPrimaryAccountExistsHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, exists);
  base::UmaHistogramBoolean(sliced_name, exists);
}

void AimEligibilityService::LogEligibilityRequestPrimaryAccountInCookieJar(
    bool in_cookie_jar,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestPrimaryAccountInCookieJarHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, in_cookie_jar);
  base::UmaHistogramBoolean(sliced_name, in_cookie_jar);
}

void AimEligibilityService::LogEligibilityRequestPrimaryAccountIndex(
    size_t session_index,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestPrimaryAccountIndexHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramSparse(name, session_index);
  base::UmaHistogramSparse(sliced_name, session_index);
}

void AimEligibilityService::LogEligibilityRequestStatus(
    EligibilityRequestStatus status,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestStatusHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramEnumeration(name, status);
  base::UmaHistogramEnumeration(sliced_name, status);
}

void AimEligibilityService::LogEligibilityRequestResponseCode(
    int response_code,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestResponseCodeHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramSparse(name, response_code);
  base::UmaHistogramSparse(sliced_name, response_code);
}

void AimEligibilityService::LogEligibilityResponse(
    RequestSource request_source) const {
  const auto& prefix = kEligibilityResponseHistogramPrefix;
  const auto& sliced_prefix =
      GetHistogramNameSlicedByRequestSource(prefix, request_source);
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_eligible"}),
                            most_recent_response_.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({sliced_prefix, ".is_eligible"}),
                            most_recent_response_.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_pdf_upload_eligible"}),
                            most_recent_response_.is_pdf_upload_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_pdf_upload_eligible"}),
      most_recent_response_.is_pdf_upload_eligible());
  base::UmaHistogramSparse(base::StrCat({prefix, ".session_index"}),
                           most_recent_response_.session_index());
  base::UmaHistogramSparse(base::StrCat({sliced_prefix, ".session_index"}),
                           most_recent_response_.session_index());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_deep_search_eligible"}),
                            most_recent_response_.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_deep_search_eligible"}),
      most_recent_response_.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_image_generation_eligible"}),
      most_recent_response_.is_image_generation_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_image_generation_eligible"}),
      most_recent_response_.is_image_generation_eligible());
}

void AimEligibilityService::LogEligibilityResponseChanges(
    const omnibox::AimEligibilityResponse& old_response,
    const omnibox::AimEligibilityResponse& new_response) const {
  const auto& prefix = kEligibilityResponseChangeHistogramPrefix;
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_eligible"}),
      old_response.is_eligible() != new_response.is_eligible());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_pdf_upload_eligible"}),
                            old_response.is_pdf_upload_eligible() !=
                                new_response.is_pdf_upload_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".session_index"}),
      old_response.session_index() != new_response.session_index());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_deep_search_eligible"}),
                            old_response.is_deep_search_eligible() !=
                                new_response.is_deep_search_eligible());
  base::UmaHistogramBoolean(
      base::StrCat({prefix, ".is_image_generation_eligible"}),
      old_response.is_image_generation_eligible() !=
          new_response.is_image_generation_eligible());
}
