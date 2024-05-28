// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_WEATHER_V2_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_WEATHER_V2_PROVIDER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/birch/birch_data_provider.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

class Profile;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}

namespace ash {

class BirchWeatherItem;

// Birch weather provider that fetches weather information using
// chromeossystemui server.
class ASH_EXPORT BirchWeatherV2Provider : public BirchDataProvider {
 public:
  using ModelUpdaterCallback =
      base::RepeatingCallback<void(std::vector<BirchWeatherItem> items)>;
  BirchWeatherV2Provider(Profile* profile, ModelUpdaterCallback model_updater);
  BirchWeatherV2Provider(const BirchWeatherV2Provider&) = delete;
  BirchWeatherV2Provider& operator=(const BirchWeatherV2Provider&) = delete;
  ~BirchWeatherV2Provider() override;

  // Called from birch model to request weather information to be displayed in
  // UI.
  void RequestBirchDataFetch() override;

  void Shutdown();

  void OverrideBaseRequestUrlForTesting(const GURL& base_url) {
    base_url_override_ = base_url;
  }

 private:
  // Starts the weather fetch.
  void FetchWeather();

  // Called in response to the weather info HTTP request. `json_response`
  // contains the response with the weather info, or nullptr if the request
  // fails.
  void OnWeatherFetched(std::unique_ptr<std::string> json_response);

  // Callback to the request to parse the weather info response JSON.
  void OnWeatherInfoParsed(
      base::expected<base::Value, std::string> weather_info);

  const raw_ptr<Profile> profile_;

  // Callback called when the weather info is fetched, and parsed. Expected
  // to update weather items in the birch model. It will get called with an
  // empty list of weather items if the request fails.
  const ModelUpdaterCallback model_updater_;

  // Whether a weather information fetch is currently in progress.
  bool is_fetching_ = false;

  // Used to override the base chromeos-system-ui server base URL.
  std::optional<GURL> base_url_override_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<BirchWeatherV2Provider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_WEATHER_V2_PROVIDER_H_
