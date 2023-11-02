// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class PermissionUmaUtilTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
  TestPermissionsClient permissions_client_;
};

TEST_F(PermissionUmaUtilTest, ScopedRevocationReporter) {
  content::TestBrowserContext browser_context;

  // TODO(tsergeant): Add more comprehensive tests of PermissionUmaUtil.
  base::HistogramTester histograms;
  HostContentSettingsMap* map =
      PermissionsClient::Get()->GetSettingsMap(&browser_context);
  GURL host("https://example.com");
  ContentSettingsPattern host_pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);
  ContentSettingsPattern host_containing_wildcards_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com/");
  ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  PermissionSourceUI source_ui = PermissionSourceUI::SITE_SETTINGS;

  // Allow->Block triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Block->Allow does not trigger a revocation.
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Allow->Default triggers a revocation when default is 'ask'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ASK);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type,
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Default does not trigger a revocation when default is 'allow'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type,
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Block with url pattern string triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_pattern, host_pattern, type, source_ui);
    map->SetContentSettingCustomScope(host_pattern,
                                      ContentSettingsPattern::Wildcard(), type,
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);

  // Allow->Block with non url pattern string does not trigger a revocation.
  map->SetContentSettingDefaultScope(host, host, type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_containing_wildcards_pattern, host_pattern, type,
        source_ui);
    map->SetContentSettingCustomScope(host_containing_wildcards_pattern,
                                      ContentSettingsPattern::Wildcard(), type,
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);
}

TEST_F(PermissionUmaUtilTest, CrowdDenyVersionTest) {
  base::HistogramTester histograms;

  const absl::optional<base::Version> empty_version;
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(empty_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 0, 1);

  const absl::optional<base::Version> valid_version =
      base::Version({2020, 10, 11, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 20201011, 1);

  const absl::optional<base::Version> valid_old_version =
      base::Version({2019, 10, 10, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_old_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 1, 1);

  const absl::optional<base::Version> valid_future_version =
      base::Version({2021, 1, 1, 1234});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(
      valid_future_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 20210101, 1);

  const absl::optional<base::Version> invalid_version =
      base::Version({2020, 10, 11});
  PermissionUmaUtil::RecordCrowdDenyVersionAtAbuseCheckTime(valid_version);
  histograms.ExpectBucketCount(
      "Permissions.CrowdDeny.PreloadData.VersionAtAbuseCheckTime", 1, 1);
}

// Test that the appropriate UMA metrics have been recorded when the DSE is
// disabled.
TEST_F(PermissionUmaUtilTest, MetricsAreRecordedWhenAutoDSEPermissionReverted) {
  const std::string kTransitionHistogramPrefix =
      "Permissions.DSE.AutoPermissionRevertTransition.";

  constexpr struct {
    ContentSetting backed_up_setting;
    ContentSetting effective_setting;
    ContentSetting end_state_setting;
    permissions::AutoDSEPermissionRevertTransition expected_transition;
  } kTests[] = {
      // Expected valid combinations.
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
       permissions::AutoDSEPermissionRevertTransition::NO_DECISION_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
       permissions::AutoDSEPermissionRevertTransition::CONFLICT_ASK},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_BLOCK},
  };

  // We test every combination of test case for notifications and geolocation to
  // basically test the entire possible transition space.
  for (const auto& test : kTests) {
    for (const auto type : {ContentSettingsType::NOTIFICATIONS,
                            ContentSettingsType::GEOLOCATION}) {
      const std::string type_string = type == ContentSettingsType::NOTIFICATIONS
                                          ? "Notifications"
                                          : "Geolocation";
      base::HistogramTester histograms;
      PermissionUmaUtil::RecordAutoDSEPermissionReverted(
          type, test.backed_up_setting, test.effective_setting,
          test.end_state_setting);

      // Test that the expected samples are recorded in histograms.
      histograms.ExpectBucketCount(kTransitionHistogramPrefix + type_string,
                                   test.expected_transition, 1);
      histograms.ExpectTotalCount(kTransitionHistogramPrefix + type_string, 1);
    }
  }
}

}  // namespace permissions
