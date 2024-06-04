// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_weather_v2_provider.h"

#include <string>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash {

// Components of the chromeossystemui API request URL. Split in two parts so
// the base URL can be overridden in tests.
constexpr char kDefaultBaseUrl[] = "https://chromeossystemui-pa.googleapis.com";
constexpr char kRequestRelativeUrl[] = "/v1/weather?feature_id=1";

constexpr size_t kMaxDownloadBytes = 20 * 1024;

// TODO(b/343206102): The plan for the weather provider is to send location
//                    information to the weather service - update network
//                    annotations when that's implemented.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("birch_weather_provider", R"(
       semantics {
         sender: "Post-login glanceables"
         description:
            "Fetch current, or forecasted weather information for the user's "
            "current location. The weather is used in a suggestion chip button "
            "for an activity the user might want to perform after login or "
            "from overview mode (e.g. view the weather)."
          trigger:
              "User logs in to the device or enters overview mode."
          data: "None"
          user_data: {
            type: NONE
          }
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "tbarzic@google.com"
            }
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          last_reviewed: "2024-05-30"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is off by default - guarded by ForestFeature, and "
            "BirchWeatherV2 feature flags. If the feature flags are enabled, "
            "the feature can be disabled by disabling weather in the context "
            "menu on the suggestion chips."
          chrome_policy {
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
        })");

BirchWeatherV2Provider::BirchWeatherV2Provider(
    Profile* profile,
    ModelUpdaterCallback model_updater)
    : profile_(profile), model_updater_(model_updater) {
  url_loader_factory_ = profile_->GetURLLoaderFactory();
}

BirchWeatherV2Provider::~BirchWeatherV2Provider() = default;

void BirchWeatherV2Provider::RequestBirchDataFetch() {
  const auto* const pref_service = profile_->GetPrefs();
  if (!pref_service ||
      !base::Contains(pref_service->GetList(
                          prefs::kContextualGoogleIntegrationsConfiguration),
                      prefs::kWeatherIntegrationName)) {
    // Weather integration is disabled by policy.
    model_updater_.Run({});
    return;
  }
  if (!SimpleGeolocationProvider::GetInstance()
           ->IsGeolocationUsageAllowedForSystem()) {
    // Weather is not allowed if geolocation is off.
    model_updater_.Run({});
    return;
  }
  // Only allow one fetch at a time.
  if (is_fetching_) {
    return;
  }
  is_fetching_ = true;

  FetchWeather();
}

void BirchWeatherV2Provider::Shutdown() {
  url_loader_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void BirchWeatherV2Provider::FetchWeather() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = base_url_override_.value_or(GURL(kDefaultBaseUrl))
                              .Resolve(kRequestRelativeUrl);
  DCHECK(resource_request->url.is_valid());

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);

  // Perform the request.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&BirchWeatherV2Provider::OnWeatherFetched,
                     weak_factory_.GetWeakPtr()),
      kMaxDownloadBytes);
}

void BirchWeatherV2Provider::OnWeatherFetched(
    std::unique_ptr<std::string> json_response) {
  if (!json_response) {
    is_fetching_ = false;
    model_updater_.Run({});
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_response,
      base::BindOnce(&BirchWeatherV2Provider::OnWeatherInfoParsed,
                     weak_factory_.GetWeakPtr()));
}

void BirchWeatherV2Provider::OnWeatherInfoParsed(
    base::expected<base::Value, std::string> weather_info) {
  is_fetching_ = false;

  if (!weather_info.has_value() || !weather_info->is_dict()) {
    model_updater_.Run({});
    return;
  }

  std::optional<double> temp_f = weather_info->GetDict().FindDouble("tempF");
  if (!temp_f) {
    model_updater_.Run({});
    return;
  }

  std::vector<BirchWeatherItem> items;
  items.emplace_back(u"[i18n] Current weather", temp_f.value(),
                     ui::ImageModel());

  model_updater_.Run(std::move(items));
}

}  // namespace ash
