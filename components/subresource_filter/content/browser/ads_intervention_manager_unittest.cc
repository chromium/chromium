// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_intervention_manager.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

class AdsInterventionManagerTest : public testing::Test {
 public:
  AdsInterventionManagerTest() = default;
  AdsInterventionManagerTest(const AdsInterventionManagerTest&) = delete;
  AdsInterventionManagerTest& operator=(const AdsInterventionManagerTest&) =
      delete;

  // Creates and configures the AdsInterventionManager instance used by the
  // tests, first creating the dependencies that need to be supplied to that
  // instance.
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    settings_manager_ =
        std::make_unique<SubresourceFilterContentSettingsManager>(
            settings_map_.get());

    ads_intervention_manager_ =
        std::make_unique<AdsInterventionManager>(settings_manager_.get());

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    ads_intervention_manager_->set_clock_for_testing(test_clock_.get());
  }

  void TearDown() override { settings_map_->ShutdownOnUIThread(); }

  base::SimpleTestClock* test_clock() { return test_clock_.get(); }

 protected:
  // Used by the HostContentSettingsMap instance.
  sync_preferences::TestingPrefServiceSyncable prefs_;

  // Used by the SubresourceFilterContentSettingsManager instance.
  scoped_refptr<HostContentSettingsMap> settings_map_;

  // Used by the AdsInterventionManager instance.
  std::unique_ptr<SubresourceFilterContentSettingsManager> settings_manager_;

  // Instance under test.
  std::unique_ptr<AdsInterventionManager> ads_intervention_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<base::SimpleTestClock> test_clock_;
};

TEST_F(AdsInterventionManagerTest,
       NoIntervention_NoActiveInterventionReturned) {
  GURL url("https://example.test/");

  std::optional<AdsInterventionManager::LastAdsIntervention> ads_intervention =
      ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_FALSE(ads_intervention.has_value());
}

TEST_F(AdsInterventionManagerTest, SingleIntervention_TimeSinceMatchesClock) {
  GURL url("https://example.test/");

  ads_intervention_manager_->TriggerAdsInterventionForUrlOnSubsequentLoads(
      url, mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  test_clock()->Advance(base::Hours(1));

  std::optional<AdsInterventionManager::LastAdsIntervention> ads_intervention =
      ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_TRUE(ads_intervention.has_value());
  EXPECT_EQ(ads_intervention->ads_violation,
            mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  EXPECT_EQ(ads_intervention->duration_since, base::Hours(1));

  // Advance the clock by two hours, duration since should now be 3 hours.
  test_clock()->Advance(base::Hours(2));
  ads_intervention = ads_intervention_manager_->GetLastAdsIntervention(url);
  EXPECT_TRUE(ads_intervention.has_value());
  EXPECT_EQ(ads_intervention->ads_violation,
            mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  EXPECT_EQ(ads_intervention->duration_since, base::Hours(3));
}

}  // namespace subresource_filter
