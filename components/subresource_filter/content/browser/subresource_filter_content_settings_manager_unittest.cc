// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"

#include <optional>
#include <set>
#include <string>

#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

class SubresourceFilterContentSettingsManagerTest : public testing::Test {
 public:
  SubresourceFilterContentSettingsManagerTest() = default;

  SubresourceFilterContentSettingsManagerTest(
      const SubresourceFilterContentSettingsManagerTest&) = delete;
  SubresourceFilterContentSettingsManagerTest& operator=(
      const SubresourceFilterContentSettingsManagerTest&) = delete;

  // Creates and configures the SubresourceFilterContentSettingsManager instance
  // used by the tests, first creating the dependencies that need to be supplied
  // to that instance.
  void SetUp() override {
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, true /* should_record_metrics */);

    settings_manager_ =
        std::make_unique<SubresourceFilterContentSettingsManager>(
            settings_map_.get());

    settings_manager_->set_should_use_smart_ui_for_testing(true);
  }

  void TearDown() override { settings_map_->ShutdownOnUIThread(); }

  HostContentSettingsMap* GetSettingsMap() { return settings_map_.get(); }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  SubresourceFilterContentSettingsManager* settings_manager() {
    return settings_manager_.get();
  }

  ContentSetting GetContentSettingMatchingUrlWithEmptyPath(const GURL& url) {
    GURL url_with_empty_path = url.GetWithEmptyPath();
    for (const auto& it :
         GetSettingsMap()->GetSettingsForOneType(ContentSettingsType::ADS)) {
      // Need GURL conversion to get rid of unnecessary default ports.
      if (GURL(it.primary_pattern.ToString()) == url_with_empty_path)
        return it.GetContentSetting();
    }
    return CONTENT_SETTING_DEFAULT;
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

  // Used by the HostContentSettingsMap instance.
  sync_preferences::TestingPrefServiceSyncable prefs_;

  // Used by the SubresourceFilterContentSettingsManager instance.
  scoped_refptr<HostContentSettingsMap> settings_map_;

  // Instance under test.
  std::unique_ptr<SubresourceFilterContentSettingsManager> settings_manager_;
};

TEST_F(SubresourceFilterContentSettingsManagerTest, LogDefaultSetting) {
  const char kDefaultContentSetting[] =
      "ContentSettings.RegularProfile.DefaultSubresourceFilterSetting";
  // The histogram should be logged at profile creation.
  histogram_tester().ExpectTotalCount(kDefaultContentSetting, 1);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       SetSiteMetadataBasedOnActivation) {
  GURL url("https://example.test/");
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       NoSiteMetadata_SiteActivationFalse) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataForTesting(url, std::nullopt);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       MetadataExpiryFollowingActivation) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Advance the clock, metadata is cleared.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, std::nullopt);

  // Verify once metadata has expired we revert to metadata V1 and do not set
  // activation using the metadata activation key.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, std::nullopt);
}

// TODO(crbug.com/40710549): Remove test once ability to persist metadata
// is removed from the subresource filter content settings manager.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       MetadataExpiryFavorsAdsIntervention) {
  GURL url("https://example.test/");

  // Sets metadata expiry at kMaxPersistMetadataDuration from Time::Now().
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention);

  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration -
      base::Minutes(1));

  // Setting metadata in safe browsing does not overwrite the existing
  // expiration set by the ads intervention.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);

  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_NE(dict, std::nullopt);

  // Advance the clock, metadata should be cleared.
  task_environment()->FastForwardBy(base::Minutes(1));

  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, std::nullopt);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdsInterventionMetadata_ExpiresAfterDuration) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention);
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Advance the clock, metadata is cleared.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, std::nullopt);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdditionalMetadata_SetInMetadata) {
  GURL url("https://example.test/");
  const char kTestKey[] = "Test";
  base::Value::Dict additional_metadata;
  additional_metadata.Set(kTestKey, true);

  // Set activation with additional metadata.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing,
      std::move(additional_metadata));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Verify metadata was actually persisted on site activation false.
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(dict->Find(kTestKey));
}

// TODO(crbug.com/40710549): Remove test once ability to persist metadata
// is removed from the subresource filter content settings manager.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdditionalMetadata_PersistedWithAdsIntervention) {
  GURL url("https://example.test/");
  const char kTestKey[] = "Test";
  base::Value::Dict additional_metadata;
  additional_metadata.Set(kTestKey, true);

  // Set activation with additional metadata.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention,
      std::move(additional_metadata));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Verify metadata was actually persisted if another activation source
  // sets site activation false.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(dict->Find(kTestKey));
}

// Verifies that the site activation status is True when there is
// metadata without an explicit site activation status key value
// pair in the metadata.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       SiteMetadataWithoutActivationStatus_SiteActivationTrue) {
  GURL url("https://example.test/");
  base::Value::Dict dict;
  settings_manager()->SetSiteMetadataForTesting(url, std::move(dict));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest, SmartUI) {
  GURL url("https://example.test/");
  GURL url2("https://example.test/path");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingMatchingUrlWithEmptyPath(url));
  settings_manager()->OnDidShowUI(url);

  // Subsequent same-origin navigations should not show UI.
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url2));

  // Fast forward the clock.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kDelayBeforeShowingInfobarAgain);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
}

TEST_F(SubresourceFilterContentSettingsManagerTest, NoSmartUI) {
  settings_manager()->set_should_use_smart_ui_for_testing(false);

  GURL url("https://example.test/");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingMatchingUrlWithEmptyPath(url));
  settings_manager()->OnDidShowUI(url);

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       DefaultSettingsChange_NoWebsiteMetadata) {
  GURL url("https://example.test/");
  EXPECT_FALSE(settings_manager()->GetSiteMetadata(url));

  // Set the setting to the default, should not populate the metadata.
  GetSettingsMap()->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::ADS, CONTENT_SETTING_DEFAULT);

  EXPECT_FALSE(settings_manager()->GetSiteMetadata(url));
}

// Tests that ClearSiteMetadata(origin) will result in clearing metadata for all
// sites whose origin is |origin|, but will not clear metadata for sites with
// different origins.
TEST_F(SubresourceFilterContentSettingsManagerTest, ClearSiteMetadata) {
  GURL initial_url("https://example.test/1");
  GURL same_origin_url("https://example.test/2");
  GURL different_origin_url("https://second_example.test/");

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(initial_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(different_origin_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearSiteMetadata(initial_url);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearSiteMetadata(different_origin_url);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));
}

// Tests that ClearMetadataForAllSites() does indeed clear metadata for all
// sites.
TEST_F(SubresourceFilterContentSettingsManagerTest, ClearMetadataForAllSites) {
  GURL initial_url("https://example.test/1");
  GURL same_origin_url("https://example.test/2");
  GURL different_origin_url("https://second_example.test/");

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(initial_url);
  settings_manager()->OnDidShowUI(different_origin_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearMetadataForAllSites();
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));
}

}  // namespace

}  // namespace subresource_filter
