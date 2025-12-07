// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_result.h"

#include <list>
#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/safety_hub/abusive_notification_permissions_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Field;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

const char url1[] = "https://example1.com:443";
const char url2[] = "https://example2.com:443";
const char url3[] = "https://example3.com:443";
const ContentSettingsType geolocation_type = ContentSettingsType::GEOLOCATION;
const ContentSettingsType mediastream_type =
    ContentSettingsType::MEDIASTREAM_CAMERA;
const ContentSettingsType notifications_type =
    ContentSettingsType::NOTIFICATIONS;
const ContentSettingsType chooser_type =
    ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA;

std::set<ContentSettingsType> abusive_permission_types({notifications_type});
std::set<ContentSettingsType> unused_permission_types({geolocation_type,
                                                       chooser_type});
std::set<ContentSettingsType> abusive_and_unused_permission_types(
    {notifications_type, geolocation_type, chooser_type});

PermissionsData CreatePermissionsData(
    ContentSettingsPattern& primary_pattern,
    std::set<ContentSettingsType>& permission_types) {
  PermissionsData permissions_data;
  permissions_data.primary_pattern = primary_pattern;
  permissions_data.permission_types = permission_types;
  return permissions_data;
}

}  // namespace

// TODO(crbug.com/399056993): Clean-up feature flags and remove
// parameterization.
class RevokedPermissionsResultTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<
          std::tuple</*should_setup_abusive_notification_sites*/ bool,
                     /*should_setup_unused_sites*/ bool,
                     /*should_setup_disruptive_sites*/ bool>> {
 public:
  RevokedPermissionsResultTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(
        content_settings::features::kSafetyCheckUnusedSitePermissions);
    enabled_features.push_back(
        content_settings::features::
            kSafetyCheckUnusedSitePermissionsForSupportedChooserPermissions);
    if (ShouldSetupDisruptiveSites()) {
      enabled_features.push_back(
          features::kSafetyHubDisruptiveNotificationRevocation);
    }
    feature_list_.InitWithFeatures(
        /*enabled_features=*/enabled_features,
        /*disabled_features=*/{});
  }

  // There are two variations of the test: where safe browsing is enabled and
  // disabled. The former should allow abusive notifications to be revoked and
  // the latter should not. However, other permission revocations are not gated
  // by the safe browsing setting.
  bool ShouldSetupSafeBrowsing() { return get<0>(GetParam()); }
  bool ShouldSetupUnusedSites() { return get<1>(GetParam()); }
  bool ShouldSetupDisruptiveSites() { return get<2>(GetParam()); }

  void AddRevokedPermissionToResult(
      RevokedPermissionsResult* result,
      std::set<ContentSettingsType> permission_types,
      std::string url) {
    auto origin = ContentSettingsPattern::FromString(url);
    result->AddRevokedPermission(
        CreatePermissionsData(origin, permission_types));
  }

  bool IsUrlInRevokedSettings(std::list<PermissionsData> permissions_data,
                              std::string url) {
    // TODO(crbug.com/40250875): Replace the below with a lambda method and
    // base::Contains.
    std::string url_pattern =
        ContentSettingsPattern::FromURLNoWildcard(GURL(url)).ToString();
    for (const auto& permission : permissions_data) {
      if (permission.primary_pattern.ToString() == url ||
          permission.primary_pattern.ToString() == url_pattern) {
        return true;
      }
    }
    return false;
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(RevokedPermissionsResultTest, ResultToFromDict) {
  auto result = std::make_unique<RevokedPermissionsResult>();
  // This is necessary for revoked abusive notification permissions, since
  // checking URLs is asynchronous.
  base::RunLoop().RunUntilIdle();
  if (ShouldSetupUnusedSites()) {
    AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
    if (ShouldSetupSafeBrowsing()) {
      AddRevokedPermissionToResult(result.get(),
                                   abusive_and_unused_permission_types, url2);
    } else {
      AddRevokedPermissionToResult(result.get(), unused_permission_types, url2);
    }
  }
  if (ShouldSetupSafeBrowsing()) {
    if (!ShouldSetupUnusedSites()) {
      AddRevokedPermissionToResult(result.get(), abusive_permission_types,
                                   url2);
    }
    AddRevokedPermissionToResult(result.get(), abusive_permission_types, url3);
  }

  if (ShouldSetupUnusedSites() && ShouldSetupSafeBrowsing()) {
    EXPECT_EQ(3U, result->GetRevokedPermissions().size());
    EXPECT_EQ(ContentSettingsPattern::FromString(url1),
              result->GetRevokedPermissions().front().primary_pattern);
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  } else if (ShouldSetupUnusedSites()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url1));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
  } else if (ShouldSetupSafeBrowsing()) {
    EXPECT_EQ(2U, result->GetRevokedPermissions().size());
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url2));
    EXPECT_TRUE(IsUrlInRevokedSettings(result->GetRevokedPermissions(), url3));
  }

  // When converting to dict, the values of the revoked permissions should be
  // correctly converted to base::Value.
  base::Value::Dict dict = result->ToDictValue();
  auto* revoked_origins_list = dict.FindList(kRevokedPermissionsResultKey);
  if (ShouldSetupUnusedSites() && ShouldSetupSafeBrowsing()) {
    EXPECT_THAT(*revoked_origins_list, UnorderedElementsAre(url1, url2, url3));
  } else if (ShouldSetupUnusedSites()) {
    EXPECT_THAT(*revoked_origins_list, UnorderedElementsAre(url1, url2));
  } else if (ShouldSetupSafeBrowsing()) {
    EXPECT_THAT(*revoked_origins_list, UnorderedElementsAre(url2, url3));
  }
}

TEST_P(RevokedPermissionsResultTest, ResultGetRevokedOrigins) {
  auto result = std::make_unique<RevokedPermissionsResult>();
  EXPECT_EQ(0U, result->GetRevokedOrigins().size());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
  EXPECT_EQ(1U, result->GetRevokedOrigins().size());
  EXPECT_EQ(ContentSettingsPattern::FromString(url1),
            *result->GetRevokedOrigins().begin());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url2);
  EXPECT_EQ(2U, result->GetRevokedOrigins().size());
  EXPECT_TRUE(result->GetRevokedOrigins().contains(
      ContentSettingsPattern::FromString(url1)));
  EXPECT_TRUE(result->GetRevokedOrigins().contains(
      ContentSettingsPattern::FromString(url2)));

  // Adding another permission type to `url2` does not change the size of the
  // revoked origin list.
  std::set<ContentSettingsType> permission_types({mediastream_type});
  auto origin = ContentSettingsPattern::FromString(url2);
  result->AddRevokedPermission(CreatePermissionsData(origin, permission_types));
  EXPECT_EQ(2U, result->GetRevokedOrigins().size());
}

TEST_P(RevokedPermissionsResultTest, ResultIsTriggerForMenuNotification) {
  auto result = std::make_unique<RevokedPermissionsResult>();
  EXPECT_FALSE(result->IsTriggerForMenuNotification());
  AddRevokedPermissionToResult(result.get(), unused_permission_types, url1);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
}

TEST_P(RevokedPermissionsResultTest, ResultWarrantsNewMenuNotification) {
  auto old_result = std::make_unique<RevokedPermissionsResult>();
  auto new_result = std::make_unique<RevokedPermissionsResult>();
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 revoked in new, but not in old -> warrants notification
  AddRevokedPermissionToResult(new_result.get(), unused_permission_types, url1);
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both new and old -> no notification
  AddRevokedPermissionToResult(old_result.get(), unused_permission_types, url1);
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 in both, origin2 in new -> warrants notification
  AddRevokedPermissionToResult(new_result.get(), unused_permission_types, url2);
  EXPECT_TRUE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
  // origin1 and origin2 in both new and old -> no notification
  AddRevokedPermissionToResult(old_result.get(), unused_permission_types, url2);
  EXPECT_FALSE(
      new_result->WarrantsNewMenuNotification(old_result->ToDictValue()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RevokedPermissionsResultTest,
    testing::Combine(
        /*should_setup_abusive_notification_sites=*/testing::Bool(),
        /*should_setup_unused_sites=*/testing::Bool(),
        /*should_setup_disruptive_sites=*/testing::Bool()));
