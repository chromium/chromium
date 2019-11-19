// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "components/language/content/browser/test_utils.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/common/language_experiments.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

class GeoLanguageProviderTest : public testing::Test {
 public:
  GeoLanguageProviderTest()
      : geo_language_provider_(task_environment_.GetMainThreadTaskRunner()),
        mock_ip_geo_location_provider_(&mock_geo_location_) {
    mojo::PendingReceiver<service_manager::mojom::Connector> receiver;
    connector_ = service_manager::Connector::Create(&receiver);
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
        device::mojom::PublicIpAddressGeolocationProvider::Name_,
        base::BindRepeating(&MockIpGeoLocationProvider::Bind,
                            base::Unretained(&mock_ip_geo_location_provider_)));
    language::GeoLanguageProvider::RegisterLocalStatePrefs(
        local_state_.registry());
    language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(
        local_state_.registry());
  }

 protected:
  std::vector<std::string> GetCurrentGeoLanguages() {
    return geo_language_provider_.CurrentGeoLanguages();
  }

  void StartGeoLanguageProvider() {
    geo_language_provider_.StartUp(std::move(connector_), &local_state_);
  }

  void MoveToLocation(float latitude, float longitude) {
    mock_geo_location_.MoveToLocation(latitude, longitude);
  }

  int GetQueryNextPositionCalledTimes() {
    return mock_geo_location_.query_next_position_called_times();
  }

  void SetGeoLanguages(const std::vector<std::string>& languages) {
    geo_language_provider_.SetGeoLanguages(languages);
  }

  void SetUpCachedLanguages(const std::vector<std::string>& languages) {
    base::ListValue cache_list;
    for (size_t i = 0; i < languages.size(); ++i) {
      cache_list.Set(i, std::make_unique<base::Value>(languages[i]));
    }
    local_state_.Set(GeoLanguageProvider::kCachedGeoLanguagesPref, cache_list);
  }

  const std::vector<std::string> GetCachedLanguages() {
    std::vector<std::string> languages;
    const base::ListValue* const cached_languages_list =
        local_state_.GetList(GeoLanguageProvider::kCachedGeoLanguagesPref);
    for (const auto& language_value : *cached_languages_list) {
      languages.push_back(language_value.GetString());
    }
    return languages;
  }
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  // Object under test.
  GeoLanguageProvider geo_language_provider_;
  MockGeoLocation mock_geo_location_;
  MockIpGeoLocationProvider mock_ip_geo_location_provider_;
  std::unique_ptr<service_manager::Connector> connector_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(GeoLanguageProviderTest, GetCurrentGeoLanguages_India) {
  // Setup a random place in West Bengal, India.
  MoveToLocation(23.0, 85.7);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();

  std::vector<std::string> expected_langs = {"hi", "bn", "en"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
}

TEST_F(GeoLanguageProviderTest, GetCurrentGeoLanguages_OutsideIndia) {
  // Setup a random place in Montreal, Canada.
  MoveToLocation(45.5, 73.5);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();

  std::vector<std::string> expected_langs = {};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
}

TEST_F(GeoLanguageProviderTest, NoFrequentCalls) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();

  const std::vector<std::string>& result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs = {"hi", "en"};
  EXPECT_EQ(expected_langs, result);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(12));
  EXPECT_EQ(1, GetQueryNextPositionCalledTimes());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, ButDoCallInTheNextDay) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();

  std::vector<std::string> result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs = {"hi", "en"};
  EXPECT_EQ(expected_langs, result);
  EXPECT_EQ(expected_langs, GetCachedLanguages());

  // Move to another random place in Karnataka, India.
  MoveToLocation(23.0, 85.7);
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(25));
  EXPECT_EQ(2, GetQueryNextPositionCalledTimes());

  result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs_2 = {"hi", "bn", "en"};
  EXPECT_EQ(expected_langs_2, result);
  EXPECT_EQ(expected_langs_2, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, CachedLanguagesPresent) {
  SetUpCachedLanguages({"en", "fr"});
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();

  std::vector<std::string> expected_langs = {"en", "fr"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());

  task_environment_.RunUntilIdle();

  expected_langs = {"hi", "en"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

}  // namespace language
