// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/aim_eligibility_service_observer.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

BASE_FEATURE(kAimServerEligibilityEnabled,
             "AimServerEligibilityEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// If disabled, AIM is completely turned off (kill switch).
BASE_FEATURE(kAimEnabled, "AimEnabled", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, uses the server response for AIM eligibility for English locales.
// Has no effect if kAimServerEligibilityEnabled is enabled.
BASE_FEATURE(kAimServerEligibilityEnabledEn,
             "AimServerEligibilityEnabledEn",
             base::FEATURE_DISABLED_BY_DEFAULT);

// For recording UMA metrics. These aren't strictly omnibox-only, but omnibox is
// a major consumer of `AimEligibilityService`, and the few metrics here don't
// warrant creating a new metric namespace.
// The status of the server request. See `ServerRequestStatus`.
static constexpr char kUmaServerRequestStatusHistogramName[] =
    "Omnibox.AimEligibility.ServerRequestStatus";
// Which AIM features were eligible according to the server request.
static constexpr char kUmaServerEligibilityHistogramPrefix[] =
    "Omnibox.AimEligibility.ServerEligibility.";

static constexpr char kRequestPath[] = "/async/folae";
static constexpr char kRequestQuery[] = "async=_fmt:pb";

// The default value for the AIM policy pref; 0 = allowed, 1 = disallowed.
constexpr int kAIModeAllowedDefault = 0;

// Returns the request URL or an empty GURL if a valid URL cannot be created;
// e.g., Google is not the default search provider.
GURL GetRequestUrl(const TemplateURLService* template_url_service) {
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
  return base_gurl.ReplaceComponents(replacements);
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

}  // namespace

// static
void AimEligibilityService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kResponsePrefName, "");
  registry->RegisterIntegerPref(omnibox::kAIModeSettings,
                                kAIModeAllowedDefault);
}

bool AimEligibilityService::IsAimAllowedByPolicy(const PrefService* prefs) {
  return prefs->GetInteger(omnibox::kAIModeSettings) == kAIModeAllowedDefault;
}

AimEligibilityService::AimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory) {
  if (base::FeatureList::IsEnabled(kAimEnabled)) {
    Initialize();
  }
}

AimEligibilityService::~AimEligibilityService() = default;

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

void AimEligibilityService::AddObserver(
    AimEligibilityServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AimEligibilityService::RemoveObserver(
    AimEligibilityServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AimEligibilityService::IsServerEligibilityEnabled() const {
  return base::FeatureList::IsEnabled(kAimServerEligibilityEnabled) ||
         (base::FeatureList::IsEnabled(kAimServerEligibilityEnabledEn) &&
          IsLanguage("en"));
}

bool AimEligibilityService::IsAimLocallyEligible() const {
  // Kill switch: If AIM is completely disabled, return false.
  if (!base::FeatureList::IsEnabled(kAimEnabled)) {
    return false;
  }

  // Always check Google DSE and Policy requirements.
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_) ||
      !IsAimAllowedByPolicy(&pref_service_.get())) {
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
    return most_recent_response_.is_eligible();
  }

  return true;
}

bool AimEligibilityService::IsPdfUploadEligible() const {
  if (!IsAimEligible()) {
    return false;
  }

  if (IsServerEligibilityEnabled()) {
    return most_recent_response_.is_pdf_upload_eligible();
  }

  return true;
}

// Private methods -------------------------------------------------------------

void AimEligibilityService::Initialize() {
  // The service should not be initialized if AIM is disabled.
  CHECK(base::FeatureList::IsEnabled(kAimEnabled));
  // The service should not be initialized twice.
  CHECK(!initialized_);

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

  LoadMostRecentResponse();
  // TODO(crbug.com/436898763): Call `StartServerEligibilityRequest()` to
  // refresh the server response when user sign-in state changes.
  StartServerEligibilityRequest();
}

void AimEligibilityService::NotifyObservers() const {
  CHECK(initialized_);

  for (auto& observer : observers_) {
    observer.OnAimEligibilityChanged();
  }
}

void AimEligibilityService::UpdateMostRecentResponse(
    const omnibox::AimEligibilityResponse& response_proto) {
  CHECK(initialized_);

  std::string response_string;
  response_proto.SerializeToString(&response_string);
  std::string encoded_response = base::Base64Encode(response_string);
  pref_service_->SetString(kResponsePrefName, encoded_response);

  most_recent_response_ = response_proto;
}

void AimEligibilityService::LoadMostRecentResponse() {
  CHECK(initialized_);

  std::string encoded_response = pref_service_->GetString(kResponsePrefName);
  if (encoded_response.empty()) {
    return;
  }

  std::string response_string;
  if (!base::Base64Decode(encoded_response, &response_string)) {
    return;
  }

  omnibox::AimEligibilityResponse response_proto;
  if (!ParseResponseString(response_string, &response_proto)) {
    return;
  }

  most_recent_response_ = response_proto;
}

void AimEligibilityService::StartServerEligibilityRequest() {
  CHECK(initialized_);

  // URLLoaderFactory may be null in tests.
  if (!url_loader_factory_) {
    return;
  }

  // Request URL may be invalid.
  GURL request_url = GetRequestUrl(template_url_service_.get());
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
  base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                ServerRequestStatus::kSent);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AimEligibilityService::OnServerEligibilityResponse,
                     weak_factory_.GetWeakPtr(), std::move(loader)));
}

void AimEligibilityService::OnServerEligibilityResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::unique_ptr<std::string> response_string) {
  CHECK(initialized_);

  const int response_code =
      loader->ResponseInfo() && loader->ResponseInfo()->headers
          ? loader->ResponseInfo()->headers->response_code()
          : 0;

  if (response_code != 200 || !response_string) {
    base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                  ServerRequestStatus::kErrorResponse);
    return;
  }
  omnibox::AimEligibilityResponse response_proto;
  if (!ParseResponseString(*response_string, &response_proto)) {
    base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                  ServerRequestStatus::kFailedToParse);
    return;
  }
  base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                ServerRequestStatus::kSuccess);

  base::UmaHistogramBoolean(
      base::StrCat({kUmaServerEligibilityHistogramPrefix, "is_eligible"}),
      response_proto.is_eligible());

  // Update the most recent response if server eligibility checking is enabled.
  // This ensures the prefs are not tainted until server eligibility checking is
  // rolled out.
  if (IsServerEligibilityEnabled()) {
    UpdateMostRecentResponse(response_proto);
  }

  NotifyObservers();
}
