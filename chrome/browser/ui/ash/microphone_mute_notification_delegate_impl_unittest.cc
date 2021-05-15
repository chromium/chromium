// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/microphone_mute_notification_delegate_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Gmock matchers and actions that are used below.
using ::testing::AnyOf;

class MicrophoneMuteNotificationDelegateTest : public testing::Test {
 public:
  MicrophoneMuteNotificationDelegateTest() = default;
  MicrophoneMuteNotificationDelegateTest(
      const MicrophoneMuteNotificationDelegateTest&) = delete;
  MicrophoneMuteNotificationDelegateTest& operator=(
      const MicrophoneMuteNotificationDelegateTest&) = delete;
  ~MicrophoneMuteNotificationDelegateTest() override = default;

  void SetUp() override {
    media_delegate_ =
        std::make_unique<MicrophoneMuteNotificationDelegateImpl>();

    registry_cache_.SetAccountId(account_id_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             &registry_cache_);
    capability_access_cache_.SetAccountId(account_id_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_, &capability_access_cache_);
  }

  absl::optional<std::u16string> GetAppAccessingMicrophone() {
    return media_delegate_->GetAppAccessingMicrophone(&capability_access_cache_,
                                                      &registry_cache_);
  }

  void TearDown() override { media_delegate_.reset(); }

  static apps::mojom::AppPtr MakeApp(const char* app_id, const char* name) {
    apps::mojom::AppPtr app = apps::mojom::App::New();
    app->app_id = app_id;
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::mojom::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      apps::mojom::OptionalBool microphone) {
    apps::mojom::CapabilityAccessPtr access =
        apps::mojom::CapabilityAccess::New();
    access->app_id = app_id;
    access->camera = apps::mojom::OptionalBool::kFalse;
    access->microphone = microphone;
    return access;
  }

  void LaunchApp(const char* id,
                 const char* name,
                 apps::mojom::OptionalBool use_microphone) {
    std::vector<apps::mojom::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    registry_cache_.OnApps(std::move(registry_deltas),
                           apps::mojom::AppType::kUnknown,
                           /* should_notify_initialized = */ false);

    std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(
        MakeCapabilityAccess(id, use_microphone));
    capability_access_cache_.OnCapabilityAccesses(
        std::move(capability_access_deltas));
  }

  const std::string kPrimaryProfileName = "primary_profile";
  const AccountId account_id_ = AccountId::FromUserEmail(kPrimaryProfileName);

  std::unique_ptr<MicrophoneMuteNotificationDelegateImpl> media_delegate_;

  apps::AppRegistryCache registry_cache_;
  apps::AppCapabilityAccessCache capability_access_cache_;
};

TEST_F(MicrophoneMuteNotificationDelegateTest, NoAppsLaunched) {
  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_F(MicrophoneMuteNotificationDelegateTest, AppLaunchedNotUsingMicrophone) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kFalse);

  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_F(MicrophoneMuteNotificationDelegateTest, AppLaunchedUsingMicrophone) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kTrue);

  // Should return the name of our app.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  std::string app_name_utf8 = base::UTF16ToUTF8(app_name.value());
  EXPECT_STREQ(app_name_utf8.c_str(), "name_rose");
}

TEST_F(MicrophoneMuteNotificationDelegateTest,
       MultipleAppsLaunchedUsingMicrophone) {
  LaunchApp("id_rose", "name_rose", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_mars", "name_mars", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_zara", "name_zara", apps::mojom::OptionalBool::kTrue);
  LaunchApp("id_oscar", "name_oscar", apps::mojom::OptionalBool::kFalse);

  // Because AppCapabilityAccessCache::GetAppsAccessingMicrophone (invoked by
  // GetAppAccessingMicrophone) returns a set, we have no guarantee of
  // which app will be found first.  So we verify that the app name is one of
  // our microphone-users.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  std::string app_name_utf8 = base::UTF16ToUTF8(app_name.value());
  EXPECT_THAT(app_name_utf8, AnyOf("name_rose", "name_mars", "name_zara"));
}
