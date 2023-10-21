// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
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
    language::GeoLanguageProvider::OverrideBinderForTesting(
        base::BindRepeating(&MockIpGeoLocationProvider::Bind,
                            base::Unretained(&mock_ip_geo_location_provider_)));
    language::GeoLanguageProvider::RegisterLocalStatePrefs(
        local_state_.registry());
    language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  ~GeoLanguageProviderTest() override {
    language::GeoLanguageProvider::OverrideBinderForTesting(
        base::NullCallback());
  }

 protected:
  std::vector<std::string> GetCurrentGeoLanguages() {
    return geo_language_provider_.CurrentGeoLanguages();
  }

  void StartGeoLanguageProvider() {
    geo_language_provider_.StartUp(&local_state_);
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

  void SetUpCachedLanguages(const std::vector<std::string>& languages,
                            const double update_time) {
    base::Value::List cache_list;
    for (const std::string& language : languages) {
      cache_list.Append(language);
    }
    local_state_.SetList(GeoLanguageProvider::kCachedGeoLanguagesPref,
                         std::move(cache_list));
    local_state_.SetDouble(
        GeoLanguageProvider::kTimeOfLastGeoLanguagesUpdatePref, update_time);
  }

  const std::vector<std::string> GetCachedLanguages() {
    std::vector<std::string> languages;
    const base::Value::List& cached_languages_list =
        local_state_.GetList(GeoLanguageProvider::kCachedGeoLanguagesPref);
    for (const auto& language_value : cached_languages_list) {
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

  task_environment_.FastForwardBy(base::Hours(12));
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
  task_environment_.FastForwardBy(base::Hours(25));
  EXPECT_EQ(2, GetQueryNextPositionCalledTimes());

  result = GetCurrentGeoLanguages();
  std::vector<std::string> expected_langs_2 = {"hi", "bn", "en"};
  EXPECT_EQ(expected_langs_2, result);
  EXPECT_EQ(expected_langs_2, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, CachedLanguagesUpdatedOnStartup) {
  SetUpCachedLanguages(
      {"en", "fr"},
      (base::Time::Now() - base::Hours(25)).InSecondsFSinceUnixEpoch());
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();

  std::vector<std::string> expected_langs = {"en", "fr"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());

  task_environment_.RunUntilIdle();

  expected_langs = {"hi", "en"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

TEST_F(GeoLanguageProviderTest, CachedLanguagesNotUpdatedOnStartup) {
  SetUpCachedLanguages({"en", "fr"},
                       base::Time::Now().InSecondsFSinceUnixEpoch());
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();

  std::vector<std::string> expected_langs = {"en", "fr"};
  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(expected_langs, GetCurrentGeoLanguages());
  EXPECT_EQ(expected_langs, GetCachedLanguages());
}

}  // namespace language
