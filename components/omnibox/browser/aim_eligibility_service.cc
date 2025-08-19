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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

namespace {

// If disabled, AIM is completely turned off (kill switch).
BASE_FEATURE(kAimEnabled, "AimEnabled", base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, uses the server response for AIM eligibility for all locales.
BASE_FEATURE(kAimServerEligibilityEnabled,
             "AimServerEligibilityEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ServerAimEligibilityRequestStatus)
enum class ServerRequestStatus {
  kSent = 0,
  kErrorResponse = 1,
  kFailedToParse = 2,
  kSuccess = 3,
  kMaxValue = kSuccess,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:ServerAimEligibilityRequestStatus)

static constexpr char kRequestEndpoint[] =
    "http://www.google.com/async/folae?async=_fmt:pb";

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

}  // namespace

// static
void AimEligibilityService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kResponsePrefName, "");
}

AimEligibilityService::AimEligibilityService(
    PrefService& pref_service,
    TemplateURLService& template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory) {
  // TODO(crbug.com/436898763): Call `StartServerEligibilityRequest()` to
  // refresh the server response when service is constructed and when user state
  // changes. E.g. user signs in/out, starts/stops syncing, switches profiles.
  // Some of those actions may create a new service; if so, we don't need to
  // listen to those events and start StartServerEligibilityRequest manually,
  // because it'll be called in the constructor anyways. Switching profiles
  // probably creates a new service.
  ReadFromPref();
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
  if (!search::DefaultSearchProviderIsGoogle(&template_url_service_.get()) ||
      !omnibox::IsAimAllowedByPolicy(&pref_service_.get())) {
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

void AimEligibilityService::NotifyObservers() const {
  for (auto& observer : observers_) {
    observer.OnAimEligibilityChanged();
  }
}

bool AimEligibilityService::ParseResponseString(
    const std::string& response_string) {
  // Parse into a temporary variable 1st so that if parsing fails,
  // `most_recent_response_` isn't cleared.
  omnibox::AimEligibilityResponse response_proto;
  if (!response_proto.ParseFromString(response_string)) {
    return false;
  }
  most_recent_response_ = response_proto;
  return true;
}

void AimEligibilityService::WriteToPref(
    const std::string& response_string) const {
  pref_service_->SetString(kResponsePrefName,
                           base::Base64Encode(response_string));
}

void AimEligibilityService::ReadFromPref() {
  const std::string& read_string = pref_service_->GetString(kResponsePrefName);
  std::string decoded;
  if (base::Base64Decode(read_string, &decoded)) {
    ParseResponseString(decoded);
  }
}

void AimEligibilityService::StartServerEligibilityRequest() {
  // Don't make server requests if AIM or server requests are disabled.
  if (!base::FeatureList::IsEnabled(kAimEnabled) ||
      !IsServerEligibilityEnabled()) {
    return;
  }

  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = GURL{kRequestEndpoint};
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
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
  // TODO(crbug.com/436900259): Add UMA metrics for whether the response
  //   returned 200, was parsed successfully, and which features were eligible.
  //   This will let us know how watered down UMA and finch are compared due to
  //   mismatched server eligibility criteria and estimate the actual population
  //   size.
  if (!response_string) {
    base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                  ServerRequestStatus::kErrorResponse);
    return;
  }
  if (!ParseResponseString(*response_string)) {
    base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                  ServerRequestStatus::kFailedToParse);
    return;
  }
  base::UmaHistogramEnumeration(kUmaServerRequestStatusHistogramName,
                                ServerRequestStatus::kSuccess);
  base::UmaHistogramBoolean(
      base::StrCat({kUmaServerEligibilityHistogramPrefix, "is_eligible"}),
      most_recent_response_.is_eligible());
  WriteToPref(*response_string);
  NotifyObservers();
}
