// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/microphone_mute_notification_delegate_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestMicrophoneMuteNotificationDelegateImpl
    : public MicrophoneMuteNotificationDelegateImpl {
 public:
  TestMicrophoneMuteNotificationDelegateImpl() = default;
  TestMicrophoneMuteNotificationDelegateImpl(
      const MicrophoneMuteNotificationDelegateImpl&) = delete;
  TestMicrophoneMuteNotificationDelegateImpl& operator=(
      const MicrophoneMuteNotificationDelegateImpl&) = delete;
  ~TestMicrophoneMuteNotificationDelegateImpl() override = default;

  void SetFakeActiveUserAccountId(AccountId id) {
    user_account_id_ = id;
    CheckActiveUserChanged();
  }

  AccountId GetActiveUserAccountId() override { return user_account_id_; }

 private:
  AccountId user_account_id_ = EmptyAccountId();
};

class MicrophoneMuteNotificationDelegateTest : public testing::Test {
 public:
  MicrophoneMuteNotificationDelegateTest() = default;
  MicrophoneMuteNotificationDelegateTest(
      const MicrophoneMuteNotificationDelegateTest&) = delete;
  MicrophoneMuteNotificationDelegateTest& operator=(
      const MicrophoneMuteNotificationDelegateTest&) = delete;
  ~MicrophoneMuteNotificationDelegateTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    microphone_mute_notification_delegate_ =
        std::make_unique<TestMicrophoneMuteNotificationDelegateImpl>();

    SetupPrimaryUser();
  }

  void TearDown() override {
    microphone_mute_notification_delegate_.reset();
    testing::Test::TearDown();
  }

  void SetupPrimaryUser() {
    registry_cache_primary_user_.SetAccountId(account_id_primary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_primary_user_, &registry_cache_primary_user_);
    capability_access_cache_primary_user_.SetAccountId(
        account_id_primary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_primary_user_, &capability_access_cache_primary_user_);
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(
        account_id_primary_user_);
  }

  void SetupSecondaryUser() {
    registry_cache_secondary_user_.SetAccountId(account_id_secondary_user_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(
        account_id_secondary_user_, &registry_cache_secondary_user_);
    capability_access_cache_secondary_user_.SetAccountId(
        account_id_secondary_user_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_secondary_user_, &capability_access_cache_secondary_user_);
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(
        account_id_secondary_user_);
  }

  absl::optional<std::u16string> GetAppAccessingMicrophone() {
    apps::AppCapabilityAccessCache* cap_cache =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_)
            ? &capability_access_cache_primary_user_
            : &capability_access_cache_secondary_user_;
    apps::AppRegistryCache* reg_cache =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_)
            ? &registry_cache_primary_user_
            : &registry_cache_secondary_user_;
    return microphone_mute_notification_delegate_->GetAppAccessingMicrophone(
        cap_cache, reg_cache);
  }

  static apps::AppPtr MakeApp(const char* app_id, const char* name) {
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
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

  void LaunchAppUsingMicrophone(const char* id,
                                const char* name,
                                bool use_microphone) {
    bool is_primary_user =
        (microphone_mute_notification_delegate_->GetActiveUserAccountId() ==
         account_id_primary_user_);
    apps::AppRegistryCache* reg_cache = is_primary_user
                                            ? &registry_cache_primary_user_
                                            : &registry_cache_secondary_user_;
    apps::AppCapabilityAccessCache* cap_cache =
        is_primary_user ? &capability_access_cache_primary_user_
                        : &capability_access_cache_secondary_user_;

    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    if (base::FeatureList::IsEnabled(
            apps::kAppServiceOnAppUpdateWithoutMojom)) {
      reg_cache->OnApps(std::move(registry_deltas), apps::AppType::kUnknown,
                        /*should_notify_initialized=*/false);
    } else {
      std::vector<apps::mojom::AppPtr> mojom_deltas;
      mojom_deltas.push_back(apps::ConvertAppToMojomApp(registry_deltas[0]));
      reg_cache->OnApps(std::move(mojom_deltas), apps::mojom::AppType::kUnknown,
                        /*should_notify_initialized=*/false);
    }

    std::vector<apps::mojom::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(MakeCapabilityAccess(
        id, use_microphone ? apps::mojom::OptionalBool::kTrue
                           : apps::mojom::OptionalBool::kFalse));
    cap_cache->OnCapabilityAccesses(std::move(capability_access_deltas));
  }

  void SetActiveUserAccountId(AccountId id) {
    microphone_mute_notification_delegate_->SetFakeActiveUserAccountId(id);
  }

  const std::string kPrimaryProfileName = "primary_profile";
  const AccountId account_id_primary_user_ =
      AccountId::FromUserEmail(kPrimaryProfileName);
  const std::string kSecondaryProfileName = "secondary_profile";
  const AccountId account_id_secondary_user_ =
      AccountId::FromUserEmail(kSecondaryProfileName);

  std::unique_ptr<TestMicrophoneMuteNotificationDelegateImpl>
      microphone_mute_notification_delegate_;

  apps::AppRegistryCache registry_cache_primary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_primary_user_;
  apps::AppRegistryCache registry_cache_secondary_user_;
  apps::AppCapabilityAccessCache capability_access_cache_secondary_user_;

  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(MicrophoneMuteNotificationDelegateTest, NoAppsLaunched) {
  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_F(MicrophoneMuteNotificationDelegateTest, AppLaunchedNotUsingMicrophone) {
  LaunchAppUsingMicrophone("id_rose", "name_rose", false);

  // Should return a completely value-free app_name.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_F(MicrophoneMuteNotificationDelegateTest, AppLaunchedUsingMicrophone) {
  LaunchAppUsingMicrophone("id_rose", "name_rose", true);

  // Should return the name of our app.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_rose");
}

TEST_F(MicrophoneMuteNotificationDelegateTest,
       MultipleAppsLaunchedUsingMicrophone) {
  LaunchAppUsingMicrophone("id_rose", "name_rose", true);
  LaunchAppUsingMicrophone("id_mars", "name_mars", true);
  LaunchAppUsingMicrophone("id_zara", "name_zara", true);
  LaunchAppUsingMicrophone("id_oscar", "name_oscar", false);

  // Most recently launched mic-using app should be the one we use for the
  // notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_zara");

  // Oscar starts using the mic, Oscar shows up in the notification.
  LaunchAppUsingMicrophone("id_oscar", "name_oscar", true);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_oscar");

  // If we "kill" Oscar (set to no longer be using the mic or camera),
  // the notification shows Zara again.
  LaunchAppUsingMicrophone("id_oscar", "name_oscar", false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_zara");
}

TEST_F(MicrophoneMuteNotificationDelegateTest, MultipleUsers) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(account_id_primary_user_);

  // Primary user launches a mic-using app.
  LaunchAppUsingMicrophone("id_primary_user", "name_primary_user", true);

  // App we just launched should show up in the notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user");

  // Secondary user is now the primary user.
  SetActiveUserAccountId(account_id_secondary_user_);

  // Secondary user launches a mic-using app.
  LaunchAppUsingMicrophone("id_secondary_user", "name_secondary_user", true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Switch back to the primary user and "kill" the app it was running, no app
  // name to show.
  SetActiveUserAccountId(account_id_primary_user_);
  LaunchAppUsingMicrophone("id_primary_user", "name_primary_user", false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());

  // Now switch back to the secondary user, verify that the same app as before
  // shows up in the notification.
  SetActiveUserAccountId(account_id_secondary_user_);
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Now "kill" our secondary user's app and verify that there's no name to
  // show.
  LaunchAppUsingMicrophone("id_secondary_user", "name_secondary_user", false);
  app_name = GetAppAccessingMicrophone();
  EXPECT_FALSE(app_name.has_value());
}

TEST_F(MicrophoneMuteNotificationDelegateTest, MultipleUsersMultipleApps) {
  // Prepare the secondary user.
  SetupSecondaryUser();

  // Primary user is the active user.
  SetActiveUserAccountId(account_id_primary_user_);

  // Primary user launches a mic-using app.
  LaunchAppUsingMicrophone("id_primary_user", "name_primary_user", true);

  // App we just launched should show up in the notification.
  absl::optional<std::u16string> app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user");

  // Primary user launches a second mic-using app.
  LaunchAppUsingMicrophone("id_primary_user", "name_primary_user_another_app",
                           true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user_another_app");

  // Secondary user is now the primary user.
  SetActiveUserAccountId(account_id_secondary_user_);

  // Secondary user launches a mic-using app.
  LaunchAppUsingMicrophone("id_secondary_user", "name_secondary_user", true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user");

  // Secondary user launches a second mic-using app.
  LaunchAppUsingMicrophone("id_secondary_user",
                           "name_secondary_user_another_app", true);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_secondary_user_another_app");

  // Switch back to the primary user.
  SetActiveUserAccountId(account_id_primary_user_);

  // App we just launched should show up in the notification.
  app_name = GetAppAccessingMicrophone();
  EXPECT_TRUE(app_name.has_value());
  EXPECT_EQ(app_name, u"name_primary_user_another_app");
}
