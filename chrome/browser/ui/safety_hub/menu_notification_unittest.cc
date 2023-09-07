// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

class SafetyHubMenuNotificationTest : public testing::Test {
 public:
  SafetyHubMenuNotificationTest() = default;
  ~SafetyHubMenuNotificationTest() override = default;
};

TEST_F(SafetyHubMenuNotificationTest, ToFromDictValue) {
  // Content settings registry needs to be reset to ensure that it has loaded
  // the correct permission types.
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  registry->ResetForTest();

  // Creating a mock menu notification.
  const std::string url1 = "https://example1.com:443";
  auto origin = ContentSettingsPattern::FromString(url1);
  base::Time first = base::Time::Now() - base::Days(60);
  base::Time last = base::Time::Now() - base::Days(5);
  std::set<ContentSettingsType> permission_types(
      {ContentSettingsType::GEOLOCATION});
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  result->AddRevokedPermission(origin, permission_types, first);
  auto notification = std::make_unique<SafetyHubMenuNotification>();
  notification->is_currently_active_ = true;
  notification->impression_count_ = 42;
  notification->first_impression_time_ = first;
  notification->last_impression_time_ = last;
  notification->result_ = std::move(result);

  // When transforming the notification to a Dict, the properties of the
  // notification should be correct.
  base::Value::Dict dict = notification->ToDictValue();
  EXPECT_TRUE(dict.FindBool(kSafetyHubMenuNotificationActiveKey).value());
  EXPECT_EQ(42, dict.FindInt(kSafetyHubMenuNotificationImpressionCountKey));
  EXPECT_EQ(base::TimeToValue(first),
            *dict.Find(kSafetyHubMenuNotificationFirstImpressionKey));
  EXPECT_EQ(base::TimeToValue(last),
            *dict.Find(kSafetyHubMenuNotificationLastImpressionKey));
  EXPECT_TRUE(dict.contains(kSafetyHubMenuNotificationResultKey));
  // The properties of the contained result should also be correct.
  auto* result_dict = dict.FindDict(kSafetyHubMenuNotificationResultKey);
  EXPECT_TRUE(result_dict->contains(kUnusedSitePermissionsResultKey));
  EXPECT_EQ(1U, result_dict->FindList(kUnusedSitePermissionsResultKey)->size());
  base::Value::Dict& revoked_perm =
      result_dict->FindList(kUnusedSitePermissionsResultKey)->front().GetDict();
  EXPECT_EQ(url1,
            *revoked_perm.FindString(kUnusedSitePermissionsResultOriginKey));

  // Using the dict from before, we can create another menu notification object
  // that should have the same properties as when it was initially created.
  auto new_notification = SafetyHubMenuNotification::FromDictValue<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>(
      std::move(dict));
  EXPECT_TRUE(new_notification->is_currently_active_);
  EXPECT_EQ(42, new_notification->impression_count_);
  EXPECT_EQ(first, new_notification->first_impression_time_);
  EXPECT_EQ(last, new_notification->last_impression_time_);
  EXPECT_NE(nullptr, new_notification->result_);
  // Similarly, the result should contain the same properties as the one that
  // was transformed into a Dict.
  auto* new_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          new_notification->result_.get());
  EXPECT_EQ(1U, new_result->GetRevokedPermissions().size());
  EXPECT_EQ(origin, *new_result->GetRevokedOrigins().begin());
}
