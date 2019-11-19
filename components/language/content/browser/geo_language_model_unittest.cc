// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_model.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/test_utils.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {
namespace {

// Compares LanguageDetails.
MATCHER_P(EqualsLd, lang_details, "") {
  constexpr static float kFloatEps = 0.00001f;
  return arg.lang_code == lang_details.lang_code &&
         std::abs(arg.score - lang_details.score) < kFloatEps;
}

}  // namespace

class GeoLanguageModelTest : public testing::Test {
 public:
  GeoLanguageModelTest()
      : geo_language_provider_(task_environment_.GetMainThreadTaskRunner()),
        geo_language_model_(&geo_language_provider_),
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
  void StartGeoLanguageProvider() {
    geo_language_provider_.StartUp(std::move(connector_), &local_state_);
  }

  void MoveToLocation(float latitude, float longitude) {
    mock_geo_location_.MoveToLocation(latitude, longitude);
  }

  GeoLanguageModel* language_model() { return &geo_language_model_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  GeoLanguageProvider geo_language_provider_;
  // Object under test.
  GeoLanguageModel geo_language_model_;
  MockGeoLocation mock_geo_location_;
  MockIpGeoLocationProvider mock_ip_geo_location_provider_;
  std::unique_ptr<service_manager::Connector> connector_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(GeoLanguageModelTest, InsideIndia) {
  // Setup a random place in Madhya Pradesh, India.
  MoveToLocation(23.0, 80.0);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(language_model()->GetLanguages(),
              testing::ElementsAre(
                  EqualsLd(LanguageModel::LanguageDetails("hi", 0.f)),
                  EqualsLd(LanguageModel::LanguageDetails("en", 0.f))));
}

TEST_F(GeoLanguageModelTest, OutsideIndia) {
  // Setup a random place outside of India.
  MoveToLocation(45.5, 73.5);
  StartGeoLanguageProvider();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0UL, language_model()->GetLanguages().size());
}

}  // namespace language
