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
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/omnibox_proto/aim_eligibility_client_request.pb.h"
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
// Histogram for OAuth fallback.
static constexpr char kEligibilityRequestOAuthFallbackHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestOAuthFallback";
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
// Histogram for the access token fetch status.
static constexpr char kEligibilityRequestOAuthTokenFetchStatusHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestOAuthTokenFetchStatus";
// Histogram for whether the OAuth token was provided.
static constexpr char kEligibilityRequestOAuthTokenProvidedHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestOAuthTokenProvided";
// Histogram for the eligibility request debounced.
static constexpr char kEligibilityRequestDebouncedHistogramName[] =
    "Omnibox.AimEligibility.EligibilityRequestDebounced";
// Histogram for whether the eligibility response account mismatches.
static constexpr char kEligibilityResponseAccountMismatchHistogramName[] =
    "Omnibox.AimEligibility.EligibilityResponseAccountMismatch";

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
          "The client's locale information (e.g. en-US) may be sent as a "
          "query parameter or within a proto body in the request."
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

// Determines whether the specified tool mode is permitted based on the
// allowed tools list within the `SearchboxConfig` rule set.
bool IsToolAllowed(const omnibox::SearchboxConfig& config,
                   omnibox::ToolMode tool_mode) {
  if (config.has_rule_set()) {
    for (const auto& allowed_tool : config.rule_set().allowed_tools()) {
      if (allowed_tool == tool_mode) {
        return true;
      }
    }
  }
  return false;
}

// Determines whether the specified input type is permitted based on the
// allowed input types list within the `SearchboxConfig` rule set.
bool IsInputTypeAllowed(const omnibox::SearchboxConfig& config,
                        omnibox::InputType input_type) {
  if (config.has_rule_set()) {
    for (const auto& allowed_type : config.rule_set().allowed_input_types()) {
      if (allowed_type == input_type) {
        return true;
      }
    }
  }
  return false;
}

// Builds a fallback `SearchboxConfig` from the legacy eligibility fields in
// `response`.
void BuildFallbackConfig(const omnibox::AimEligibilityResponse& response,
                         omnibox::SearchboxConfig& fallback_config) {
  fallback_config.Clear();
  auto* rule_set = fallback_config.mutable_rule_set();
  rule_set->set_max_total_inputs(10);

  auto* lens_image_rule = rule_set->add_input_type_rules();
  lens_image_rule->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  lens_image_rule->set_max_instance(10);

  auto* lens_file_rule = rule_set->add_input_type_rules();
  lens_file_rule->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_FILE);
  lens_file_rule->set_max_instance(10);

  auto* browser_tab_rule = rule_set->add_input_type_rules();
  browser_tab_rule->set_input_type(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  browser_tab_rule->set_max_instance(10);

  if (response.is_deep_search_eligible()) {
    rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    auto* tool_config = fallback_config.add_tool_configs();
    tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    auto* deep_search_rule = rule_set->add_tool_rules();
    deep_search_rule->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    deep_search_rule->set_allow_all_input_types(false);
  }
  if (response.is_canvas_eligible()) {
    rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_CANVAS);
  }
  if (response.is_image_generation_eligible()) {
    rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    auto* tool_config = fallback_config.add_tool_configs();
    tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    auto* image_gen_rule = rule_set->add_tool_rules();
    image_gen_rule->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    image_gen_rule->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  }
  if (response.is_pdf_upload_eligible()) {
    rule_set->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);
    rule_set->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
    rule_set->add_allowed_input_types(
        omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  }
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

// static
AimEligibilityService::ServerEligibilityRequestMode
AimEligibilityService::GetServerEligibilityRequestMode() {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilityIncludeClientLocale)) {
    switch (omnibox::kAimServerEligibilityIncludeClientLocaleMode.Get()) {
      case omnibox::AimServerEligibilityIncludeClientLocaleMode::kPostWithProto:
        return ServerEligibilityRequestMode::kPostWithProto;
      case omnibox::AimServerEligibilityIncludeClientLocaleMode::kGetWithLocale:
        return ServerEligibilityRequestMode::kGetWithLocale;
      case omnibox::AimServerEligibilityIncludeClientLocaleMode::kLegacyGet:
        return ServerEligibilityRequestMode::kLegacyGet;
    }
  }

  // Default behavior is legacy GET (no locale).
  return ServerEligibilityRequestMode::kLegacyGet;
}

AimEligibilityService::AimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const std::string& locale,
    Configuration configuration)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      configuration_(std::move(configuration)) {
  if (!base::FeatureList::IsEnabled(omnibox::kAimEnabled)) {
    return;
  }

  if (!template_url_service_) {
    return;
  }

  pref_change_registrar_.Init(&pref_service_.get());
  pref_change_registrar_.Add(
      kResponsePrefName,
      base::BindRepeating(&AimEligibilityService::OnEligibilityResponseChanged,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      omnibox::kAIModeSettings,
      base::BindRepeating(&AimEligibilityService::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  is_dse_google_ = search::DefaultSearchProviderIsGoogle(template_url_service_);
  template_url_service_->AddObserver(this);

  LoadMostRecentResponse();

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
    ScheduleServerEligibilityRequest(RequestSource::kStartup, locale);
  }

  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

AimEligibilityService::~AimEligibilityService() {
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
  }
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
  bool server_eligible = IsInputTypeAllowed(
      *GetSearchboxConfig(), omnibox::InputType::INPUT_TYPE_LENS_FILE);
  return IsEligibleByServer(server_eligible);
}

bool AimEligibilityService::IsDeepSearchEligible() const {
  bool server_eligible = IsToolAllowed(
      *GetSearchboxConfig(), omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  return IsEligibleByServer(server_eligible);
}

bool AimEligibilityService::IsCreateImagesEligible() const {
  if (configuration_.is_off_the_record) {
    return false;
  }
  bool server_eligible = IsToolAllowed(*GetSearchboxConfig(),
                                       omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  return IsEligibleByServer(server_eligible);
}

bool AimEligibilityService::IsCanvasEligible() const {
  bool server_eligible =
      IsToolAllowed(*GetSearchboxConfig(), omnibox::ToolMode::TOOL_MODE_CANVAS);
  return IsEligibleByServer(server_eligible);
}

bool AimEligibilityService::IsCobrowseEligible() const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimCoBrowseEligibilityCheckEnabled)) {
    return true;
  }
  return GetMostRecentResponse().is_cobrowse_eligible();
}

bool AimEligibilityService::HasAimUrlParams(const GURL& url) const {
  for (const auto& rule : GetMostRecentResponse().aim_detection_url_rule()) {
    int matched_params = 0;
    for (const auto& required_param : rule.required_params()) {
      std::string param_value;
      if (!net::GetValueForKeyInQuery(url, required_param.key(),
                                      &param_value) ||
          param_value != required_param.value()) {
        break;
      }
      matched_params++;
    }
    if (matched_params == rule.required_params_size()) {
      return true;
    }
  }

  return false;
}

bool AimEligibilityService::HasValidPrimaryAccount() const {
  if (!identity_manager_) {
    return false;
  }

  CoreAccountId primary_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  return !primary_account_id.empty() &&
         !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
             primary_account_id);
}

void AimEligibilityService::ConfigureRequestCookiesAndCredentials(
    network::ResourceRequest* request,
    bool use_oauth) const {
  if (use_oauth) {
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  } else {
    request->credentials_mode = network::mojom::CredentialsMode::kInclude;
    // Set the SiteForCookies to the request URL's site to avoid cookie
    // blocking.
    request->site_for_cookies = net::SiteForCookies::FromUrl(request->url);
  }
  request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
}

GaiaId AimEligibilityService::GetActiveAccount() const {
  if (!base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements)) {
    return GaiaId();
  }

  if (omnibox::kAimIdentityOauthEnabled.Get() && HasValidPrimaryAccount()) {
    CoreAccountInfo primary_account_info =
        GetPrimaryAccountInfo(identity_manager_);
    CHECK(!primary_account_info.IsEmpty());
    return primary_account_info.gaia;
  }

  return GetFirstAccountInCookieJarIfValid();
}

GaiaId AimEligibilityService::GetFirstAccountInCookieJarIfValid() const {
  if (!identity_manager_) {
    return GaiaId();
  }

  auto accounts_in_cookie_jar = identity_manager_->GetAccountsInCookieJar();
  const auto& all_accounts = accounts_in_cookie_jar.GetAllAccounts();
  if (!all_accounts.empty()) {
    auto first_account = all_accounts[0];
    bool valid_and_signed_in = first_account.valid && !first_account.signed_out;
    return valid_and_signed_in ? first_account.gaia_id : GaiaId();
  }

  return GaiaId();
}

// Drop the request if the accounts in the cookie jar are not fresh and the
// profile is not OTR. It is expected that `OnAccountsInCookieUpdated()` will
// be called when the accounts in the cookie jar become fresh which will trigger
// another request.
bool AimEligibilityService::ShouldDropRequest() const {
  if (!omnibox::kAimIdentityDropRequestIfCookiesStale.Get()) {
    return false;
  }

  if (!identity_manager_) {
    // If there is no identity manager (OTR profile) do not drop the request
    // since there will be no cookie events to eventually trigger a request.
    return false;
  }

  return !identity_manager_->GetAccountsInCookieJar().AreAccountsFresh();
}

void AimEligibilityService::ScheduleServerEligibilityRequestIfNeeded(
    RequestSource source) {
  GaiaId current_active_id = GetActiveAccount();
  // Schedule a server eligibility request if:
  // a) The most recent response source is default or prefs (no successful
  // startup request).
  // b) The most recent response account does not match the current active
  // account.
  if (most_recent_response_source_ == EligibilityResponseSource::kDefault ||
      most_recent_response_source_ == EligibilityResponseSource::kPrefs ||
      current_active_id != most_recent_response_account_) {
    ScheduleServerEligibilityRequest(source, GetLocale());
  }
}

const omnibox::AimEligibilityResponse&
AimEligibilityService::GetMostRecentResponse() const {
  return most_recent_response_;
}

AimEligibilityService::EligibilityResponseSource
AimEligibilityService::GetMostRecentResponseSource() const {
  return most_recent_response_source_;
}

const omnibox::SearchboxConfig* AimEligibilityService::GetSearchboxConfig()
    const {
  // If the response has `SearchboxConfig` and the PEC API Feature is enabled,
  // use `SearchboxConfig`.
  if (most_recent_response_.has_searchbox_config() &&
      base::FeatureList::IsEnabled(omnibox::kAimUsePecApi)) {
    return &most_recent_response_.searchbox_config();
  }

  BuildFallbackConfig(most_recent_response_, fallback_config_);
  return &fallback_config_;
}

void AimEligibilityService::StartServerEligibilityRequestForDebugging() {
  StartServerEligibilityRequest(RequestSource::kUser, GetLocale());
}

void AimEligibilityService::FetchEligibility(RequestSource source) {
  StartServerEligibilityRequest(source, GetLocale());
}

bool AimEligibilityService::SetEligibilityResponseForDebugging(
    const std::string& base64_encoded_response) {
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
    case RequestSource::kAimUrlNavigation:
      return "AimUrlNavigation";
    case RequestSource::kRefreshTokenUpdated:
      return "RefreshTokenUpdated";
    case RequestSource::kRefreshTokenRemoved:
      return "RefreshTokenRemoved";
    case RequestSource::kRefreshTokenError:
      return "RefreshTokenError";
    case RequestSource::kOAuthFallbackCookieChange:
      return "OAuthFallbackCookieChange";
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

void AimEligibilityService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityRefreshOnAccountChanges.Get()) {
    ScheduleServerEligibilityRequestIfNeeded(
        RequestSource::kPrimaryAccountChange);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnPrimaryAccountChanges.Get()) {
    return;
  }
  // Change to the primary account might affect AIM eligibility.
  // Refresh the server eligibility state.
  ScheduleServerEligibilityRequest(RequestSource::kPrimaryAccountChange,
                                   GetLocale());
}

void AimEligibilityService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityRefreshOnAccountChanges.Get()) {
    ScheduleServerEligibilityRequestIfNeeded(
        RequestSource::kRefreshTokenUpdated);
  }
}

void AimEligibilityService::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityRefreshOnAccountChanges.Get()) {
    ScheduleServerEligibilityRequestIfNeeded(
        RequestSource::kRefreshTokenRemoved);
  }
}

void AimEligibilityService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityRefreshOnAccountChanges.Get()) {
    ScheduleServerEligibilityRequestIfNeeded(RequestSource::kRefreshTokenError);
  }
}

void AimEligibilityService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  bool refresh_on_cookie_changes_enabled =
      base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityRefreshOnCookieChanges.Get();
  // Refresh on cookie changes if the primary account is not valid (i.e. no
  // OAuth token is available).
  bool should_refresh_on_cookie_changes = !HasValidPrimaryAccount();
  if (refresh_on_cookie_changes_enabled && should_refresh_on_cookie_changes) {
    ScheduleServerEligibilityRequestIfNeeded(
        RequestSource::kOAuthFallbackCookieChange);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          omnibox::kAimServerRequestOnIdentityChangeEnabled) ||
      !omnibox::kRequestOnCookieJarChanges.Get()) {
    return;
  }
  // Change to the accounts in the cookie jar might affect AIM eligibility.
  // Refresh the server eligibility state.
  ScheduleServerEligibilityRequest(RequestSource::kCookieChange, GetLocale());
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
    ScheduleServerEligibilityRequest(RequestSource::kNetworkChange,
                                     GetLocale());
  }
}

void AimEligibilityService::OnTemplateURLServiceChanged() {
  // `OnTemplateURLServiceChanged()` will capture:
  // a) On completing loading TURL service (i.e. syncing keywords).
  // b) The user switches the DSE TURL.
  // c) The user edits the URL of the DSE TURL without switching the TURL
  //    itself.
  // d) Other changes that don't affect the DSE and we don't need to
  //    notify observers of.
  // TODO(crbug.com/474399812): (c) is bugged;
  // `search::DefaultSearchProviderIsGoogle()` returns stale values when the
  // user edits TURL URLs.
  bool is_dse_google =
      search::DefaultSearchProviderIsGoogle(template_url_service_);
  if (is_dse_google != is_dse_google_) {
    is_dse_google_ = is_dse_google;
    eligibility_changed_callbacks_.Notify();
  }
}

void AimEligibilityService::OnTemplateURLServiceShuttingDown() {
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
    template_url_service_ = nullptr;
  }
}

void AimEligibilityService::OnPolicyChanged() {
  // Notify observers that eligibility might have changed.
  eligibility_changed_callbacks_.Notify();
}

void AimEligibilityService::OnEligibilityResponseChanged() {
  eligibility_changed_callbacks_.Notify();
}

void AimEligibilityService::UpdateMostRecentResponse(
    const omnibox::AimEligibilityResponse& response_proto,
    EligibilityResponseSource response_source) {
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
    signin::IdentityManager* identity_manager,
    const std::string& locale) {
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

  if (base::FeatureList::IsEnabled(omnibox::kAimUrlInterceptPassthrough) &&
      !omnibox::kAimUrlInterceptionParams.Get().empty()) {
    url = net::AppendQueryParameter(url, "url_intercept_params",
                                    omnibox::kAimUrlInterceptionParams.Get());
  }

  // Append locale if mode is `kGetWithLocale`.
  if (GetServerEligibilityRequestMode() ==
      ServerEligibilityRequestMode::kGetWithLocale) {
    url = net::AppendQueryParameter(url, "client_locale", locale);
  }

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

void AimEligibilityService::ScheduleServerEligibilityRequest(
    RequestSource request_source,
    const std::string& locale) {
  bool is_debounced = false;
  if (base::FeatureList::IsEnabled(omnibox::kAimEligibilityServiceDebounce)) {
    if (request_debounce_timer_.IsRunning()) {
      is_debounced = true;
    }
    request_debounce_timer_.Start(
        FROM_HERE, omnibox::kAimEligibilityServiceDebounceDelay.Get(),
        base::BindOnce(&AimEligibilityService::StartServerEligibilityRequest,
                       base::Unretained(this), request_source, locale));
  } else {
    StartServerEligibilityRequest(request_source, locale);
  }
  LogEligibilityRequestDebounced(is_debounced, request_source);
}

void AimEligibilityService::StartServerEligibilityRequest(
    RequestSource request_source,
    const std::string& locale) {
  // Cancel pending requests.
  active_loader_.reset();

  // URLLoaderFactory may be null in tests.
  if (!url_loader_factory_ || !template_url_service_) {
    return;
  }
  bool use_oauth = false;
  if (base::FeatureList::IsEnabled(
          omnibox::kAimEligibilityServiceIdentityImprovements) &&
      omnibox::kAimIdentityOauthEnabled.Get()) {
    use_oauth = HasValidPrimaryAccount();
    LogEligibilityRequestOAuthFallback(use_oauth, request_source);

    if (!use_oauth && ShouldDropRequest()) {
      return;
    }
  }

  // Request URL may be invalid.
  GURL request_url = GetRequestUrl(request_source, template_url_service_.get(),
                                   identity_manager_, locale);
  if (!request_url.is_valid()) {
    return;
  }
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = request_url;

  ConfigureRequestCookiesAndCredentials(request.get(), use_oauth);
  // If mode is POST with Proto, set method to POST.
  if (GetServerEligibilityRequestMode() ==
      ServerEligibilityRequestMode::kPostWithProto) {
    request->method = "POST";
  }

  if (request_source == RequestSource::kAimUrlNavigation &&
      base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilitySendCoBrowseUserAgentSuffixEnabled) &&
      !configuration_.user_agent_with_cobrowse_suffix.empty()) {
    request->headers.SetHeader("User-Agent",
                               configuration_.user_agent_with_cobrowse_suffix);
  }

  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilitySendFullVersionListEnabled) &&
      !configuration_.full_version_list.empty()) {
    request->headers.SetHeader("Sec-CH-UA-Full-Version-List",
                               configuration_.full_version_list);
  }

  GaiaId pending_request_account = GetActiveAccount();

  if (use_oauth) {
    // Avoid starting a new token fetch if one is already in progress. In the
    // event a request fires multiple times skip re-fetching to use the
    // original in-flight `PrimaryAccountAccessTokenFetcher`.
    if (access_token_fetcher_) {
      return;
    }

    signin::AccessTokenFetcher::TokenCallback callback =
        base::BindOnce(&AimEligibilityService::OnAccessTokenAvailable,
                       weak_factory_.GetWeakPtr(), request_source, locale,
                       pending_request_account, std::move(request));

    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kAimEligibilityService, identity_manager_,
            std::move(callback),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
            signin::ConsentLevel::kSignin);
  } else {
    SendServerEligibilityRequest(request_source, locale,
                                 pending_request_account, std::move(request));
  }
}

void AimEligibilityService::OnAccessTokenAvailable(
    RequestSource request_source,
    const std::string& locale,
    GaiaId pending_request_account,
    std::unique_ptr<network::ResourceRequest> request,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  const bool has_token = !access_token_info.token.empty();
  LogEligibilityRequestOAuthTokenProvided(has_token, request_source);
  LogEligibilityRequestOAuthTokenFetchStatus(error.state(), request_source);

  if (has_token) {
    request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({"Bearer ", access_token_info.token}));
  } else {
    if (ShouldDropRequest()) {
      // Drop the request if the accounts in the cookie jar are not fresh. It
      // expected that `OnAccountsInCookieUpdated()` will be called when
      // the accounts in the cookie jar become fresh which will trigger another
      // request.
      return;
    }
    // If this state is reached then the effective account id is not the
    // primary account.
    pending_request_account = GetFirstAccountInCookieJarIfValid();
    ConfigureRequestCookiesAndCredentials(request.get(),
                                          /*use_oauth=*/false);
  }

  SendServerEligibilityRequest(request_source, locale, pending_request_account,
                               std::move(request));
}

void AimEligibilityService::SendServerEligibilityRequest(
    RequestSource request_source,
    const std::string& locale,
    GaiaId request_account,
    std::unique_ptr<network::ResourceRequest> request) {
  active_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                    kRequestTrafficAnnotation);

  if (GetServerEligibilityRequestMode() ==
      ServerEligibilityRequestMode::kPostWithProto) {
    omnibox::AimEligibilityClientRequest client_request;
    client_request.set_client_locale(locale);
    std::string request_body;
    client_request.SerializeToString(&request_body);
    active_loader_->AttachStringForUpload(request_body,
                                          "application/x-protobuf");
  }

  LogEligibilityRequestStatus(EligibilityRequestStatus::kSent, request_source);

  if (base::FeatureList::IsEnabled(
          omnibox::kAimServerEligibilityCustomRetryPolicyEnabled)) {
    // Other places in Chrome suggest that DNS and network change related
    // failures are common on startup and use the retry policy below.
    active_loader_->SetRetryOptions(
        kMaxRetries, network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED |
                         network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  }

  active_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AimEligibilityService::OnServerEligibilityResponse,
                     weak_factory_.GetWeakPtr(), request_source,
                     request_account));
}

void AimEligibilityService::OnServerEligibilityResponse(
    RequestSource request_source,
    GaiaId response_account,
    std::optional<std::string> response_string) {
  const int response_code =
      active_loader_->ResponseInfo() && active_loader_->ResponseInfo()->headers
          ? active_loader_->ResponseInfo()->headers->response_code()
          : 0;
  const bool was_fetched_via_cache =
      active_loader_->ResponseInfo()
          ? active_loader_->ResponseInfo()->was_fetched_via_cache
          : false;
  const EligibilityRequestStatus request_status =
      was_fetched_via_cache ? EligibilityRequestStatus::kSuccessBrowserCache
                            : EligibilityRequestStatus::kSuccess;

  int num_retries = active_loader_->GetNumRetries();
  active_loader_.reset();

  ProcessServerEligibilityResponse(request_source, response_account,
                                   response_code, request_status, num_retries,
                                   std::move(response_string));
}

void AimEligibilityService::ProcessServerEligibilityResponse(
    RequestSource request_source,
    GaiaId response_account,
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
  most_recent_response_account_ = response_account;

  UpdateMostRecentResponse(response_proto, response_source);

  bool response_account_mismatch =
      most_recent_response_account_ != GetActiveAccount();
  LogEligibilityResponseAccountMismatch(response_account_mismatch,
                                        request_source);

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

void AimEligibilityService::LogEligibilityRequestOAuthTokenFetchStatus(
    GoogleServiceAuthError::State state,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestOAuthTokenFetchStatusHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramEnumeration(name, state,
                                GoogleServiceAuthError::NUM_STATES);
  base::UmaHistogramEnumeration(sliced_name, state,
                                GoogleServiceAuthError::NUM_STATES);
}

void AimEligibilityService::LogEligibilityRequestOAuthTokenProvided(
    bool has_token,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestOAuthTokenProvidedHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, has_token);
  base::UmaHistogramBoolean(sliced_name, has_token);
}

void AimEligibilityService::LogEligibilityRequestOAuthFallback(
    bool fallback_happened,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestOAuthFallbackHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, fallback_happened);
  base::UmaHistogramBoolean(sliced_name, fallback_happened);
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
  base::UmaHistogramBoolean(
      base::StrCat({sliced_prefix, ".is_cobrowse_eligible"}),
      most_recent_response_.is_cobrowse_eligible());
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_cobrowse_eligible"}),
                            most_recent_response_.is_cobrowse_eligible());
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
  base::UmaHistogramBoolean(base::StrCat({prefix, ".is_cobrowse_eligible"}),
                            old_response.is_cobrowse_eligible() !=
                                new_response.is_cobrowse_eligible());
}

void AimEligibilityService::LogEligibilityRequestDebounced(
    bool is_debounced,
    RequestSource request_source) const {
  const auto& name = kEligibilityRequestDebouncedHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, is_debounced);
  base::UmaHistogramBoolean(sliced_name, is_debounced);
}

void AimEligibilityService::LogEligibilityResponseAccountMismatch(
    bool response_account_mismatch,
    RequestSource request_source) const {
  const auto& name = kEligibilityResponseAccountMismatchHistogramName;
  const auto& sliced_name =
      GetHistogramNameSlicedByRequestSource(name, request_source);
  base::UmaHistogramBoolean(name, response_account_mismatch);
  base::UmaHistogramBoolean(sliced_name, response_account_mismatch);
}
