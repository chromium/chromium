// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kUrl1[] = "https://example1.com:443";
const base::TimeDelta kLifetime = base::Days(60);
const base::Time kPastTime = base::Time::Now() - kLifetime;

// TODO(crbug.com/40267370): Use a mock result instead.
std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
CreateUnusedSitePermissionsResult(base::Value::List urls) {
  auto result = std::make_unique<
      UnusedSitePermissionsService::UnusedSitePermissionsResult>();
  PermissionsData permissions_data;
  for (base::Value& url_val : urls) {
    permissions_data.primary_pattern =
        ContentSettingsPattern::FromString(url_val.GetString());
    permissions_data.permission_types = {ContentSettingsType::GEOLOCATION};
    permissions_data.constraints =
        content_settings::ContentSettingConstraints(kPastTime);
    permissions_data.constraints.set_lifetime(kLifetime);
    result->AddRevokedPermission(permissions_data);
  }
  return result;
}
}  // namespace

class SafetyHubMenuNotificationTest : public testing::Test {
 public:
  SafetyHubMenuNotificationTest() {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        "user@example.com", {},
        /*is_main_profile=*/true);
    EXPECT_TRUE(profile_);
  }
  ~SafetyHubMenuNotificationTest() override = default;

  void SetUp() override {
    // Content settings registry needs to be reset to ensure that it has loaded
    // the correct permission types.
    auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
    registry->ResetForTest();
    service_ = std::make_unique<UnusedSitePermissionsService>(
        profile(), profile()->GetPrefs());
  }

 protected:
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  UnusedSitePermissionsService* service() { return service_.get(); }
  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<UnusedSitePermissionsService> service_;
};

// TODO(crbug.com/364523673): This test is flaking on android pie builder.
TEST_F(SafetyHubMenuNotificationTest, DISABLED_ToFromDictValue) {
  // Creating a mock menu notification.
  base::Time last = kPastTime + base::Days(30);
  auto notification = std::make_unique<SafetyHubMenuNotification>(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
  notification->is_currently_active_ = true;
  notification->impression_count_ = 42;
  notification->first_impression_time_ = kPastTime;
  notification->last_impression_time_ = last;
  notification->current_result_ =
      CreateUnusedSitePermissionsResult(base::Value::List().Append(kUrl1));

  // When transforming the notification to a Dict, the properties of the
  // notification should be correct.
  base::Value::Dict dict = notification->ToDictValue();
  EXPECT_TRUE(
      dict.FindBool(safety_hub::kSafetyHubMenuNotificationActiveKey).value());
  EXPECT_EQ(42, dict.FindInt(
                    safety_hub::kSafetyHubMenuNotificationImpressionCountKey));
  EXPECT_EQ(
      base::TimeToValue(kPastTime),
      *dict.Find(safety_hub::kSafetyHubMenuNotificationFirstImpressionKey));
  EXPECT_EQ(
      base::TimeToValue(last),
      *dict.Find(safety_hub::kSafetyHubMenuNotificationLastImpressionKey));
  EXPECT_TRUE(dict.contains(safety_hub::kSafetyHubMenuNotificationResultKey));
  // The properties of the contained result should also be correct.
  auto* result_dict =
      dict.FindDict(safety_hub::kSafetyHubMenuNotificationResultKey);
  EXPECT_TRUE(result_dict->contains(kUnusedSitePermissionsResultKey));
  EXPECT_EQ(1U, result_dict->FindList(kUnusedSitePermissionsResultKey)->size());
  base::Value::List& revoked_origins =
      *result_dict->FindList(kUnusedSitePermissionsResultKey);
  EXPECT_EQ(kUrl1, revoked_origins.front());

  // Using the dict from before, we can create another menu notification object
  // that should have the same properties as when it was initially created.
  auto new_notification = std::make_unique<SafetyHubMenuNotification>(
      std::move(dict),
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
  EXPECT_TRUE(new_notification->is_currently_active_);
  EXPECT_EQ(42, new_notification->impression_count_);
  EXPECT_EQ(kPastTime, new_notification->first_impression_time_);
  EXPECT_EQ(last, new_notification->last_impression_time_);
  EXPECT_FALSE(new_notification->prev_stored_result_.empty());
  EXPECT_EQ(kUrl1, new_notification->prev_stored_result_
                       .FindList(kUnusedSitePermissionsResultKey)
                       ->front()
                       .GetString());
}

// TODO(crbug.com/364523673): This test is flaking on android pie builder.
TEST_F(SafetyHubMenuNotificationTest, DISABLED_ShouldBeShown) {
  base::TimeDelta interval = base::Days(30);
  auto notification = std::make_unique<SafetyHubMenuNotification>(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);

  // No result associated with notification, so should not be shown.
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // The notification is updated with a new result that is a trigger. The
  // notification has never been shown, so should be shown.
  std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
      result =
          CreateUnusedSitePermissionsResult(base::Value::List().Append(kUrl1));
  notification->UpdateResult(std::move(result));
  ASSERT_TRUE(notification->ShouldBeShown(interval));

  // Showing the notification once should make it still be shown after.
  notification->Show();
  ASSERT_TRUE(notification->ShouldBeShown(interval));

  // Showing the notification the minimum number of times, but not the minimum
  // period of time, should make it still be shown.
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount; ++i) {
    notification->Show();
    FastForwardBy(base::Hours(1));
  }
  ASSERT_TRUE(notification->ShouldBeShown(interval));

  // Moving past the minimum duration should now make the notification no longer
  // be shown.
  FastForwardBy(kSafetyHubMenuNotificationMinNotificationDuration);
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // The notification can be dismissed now, and it should still not be shown.
  notification->Dismiss();
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // If the result does not change, the notification should not be shown.
  FastForwardBy(interval);
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // When updating to a similar result, the notification should still not be
  // shown.
  std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
      similar_result =
          CreateUnusedSitePermissionsResult(base::Value::List().Append(kUrl1));
  notification->UpdateResult(std::move(similar_result));
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // When updating to a new result, the notification should be shown.
  std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
      new_result = CreateUnusedSitePermissionsResult(
          base::Value::List().Append("https://other.com:443"));
  notification->UpdateResult(std::move(new_result));
  ASSERT_TRUE(notification->ShouldBeShown(interval));

  // Notification should still be shown after showing once.
  notification->Show();
  ASSERT_TRUE(notification->ShouldBeShown(interval));

  // Dimissing an active notification should make it not be shown. Note: showing
  // notification once is a prerequisite for dismissing it.
  notification->Dismiss();
  ASSERT_FALSE(notification->ShouldBeShown(interval));

  // Create new notification and new associated result to test passing time but
  // not the count.
  result = CreateUnusedSitePermissionsResult(base::Value::List().Append(kUrl1));
  auto other_notification = std::make_unique<SafetyHubMenuNotification>(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);
  other_notification->UpdateResult(std::move(result));
  other_notification->Show();

  // Go past the minimum duration that the notification should be shown. It
  // should still be shown because it was only shown once.
  FastForwardBy(kSafetyHubMenuNotificationMinNotificationDuration +
                base::Days(2));
  ASSERT_TRUE(other_notification->ShouldBeShown(interval));

  // The notification should no longer be shown if it has already been shown a
  // sufficient number of times.
  for (int i = 0; i < kSafetyHubMenuNotificationMinImpressionCount; ++i) {
    other_notification->Show();
    FastForwardBy(base::Hours(1));
  }
  ASSERT_FALSE(other_notification->ShouldBeShown(interval));
}

// TODO(crbug.com/364523673): This test is flaking on android pie builder.
TEST_F(SafetyHubMenuNotificationTest, DISABLED_IsCurrentlyActive) {
  auto notification = std::make_unique<SafetyHubMenuNotification>(
      safety_hub::SafetyHubModuleType::UNUSED_SITE_PERMISSIONS);

  // A notification is not active when it new or when it has not been shown yet.
  ASSERT_FALSE(notification->IsCurrentlyActive());
  std::unique_ptr<UnusedSitePermissionsService::UnusedSitePermissionsResult>
      result =
          CreateUnusedSitePermissionsResult(base::Value::List().Append(kUrl1));
  notification->UpdateResult(std::move(result));
  ASSERT_FALSE(notification->IsCurrentlyActive());

  // Showing a notification makes it active.
  notification->Show();
  ASSERT_TRUE(notification->IsCurrentlyActive());

  // Dismissing the nofication makes it not active any more.
  notification->Dismiss();
  ASSERT_FALSE(notification->IsCurrentlyActive());
}

// TODO(crbug.com/40267370): Add tests for other types of Safety Hub services
// and Safety Hub results.
