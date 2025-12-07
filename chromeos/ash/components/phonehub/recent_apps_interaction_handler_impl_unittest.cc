// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "recent_apps_interaction_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::phonehub {

namespace {

using FeatureState = multidevice_setup::mojom::FeatureState;
using HostStatus = multidevice_setup::mojom::HostStatus;
using ConnectionStatus = eche_app::mojom::ConnectionStatus;

// Garbage color for the purpose of verification in these tests.
const SkColor kIconColor = SkColorSetRGB(0x12, 0x34, 0x56);
const char kIconColorR[] = "icon_color_r";
const char kIconColorG[] = "icon_color_g";
const char kIconColorB[] = "icon_color_b";
const char kIconIsMonochrome[] = "icon_is_monochrome";

class FakeClickHandler : public RecentAppClickObserver {
 public:
  FakeClickHandler() = default;
  ~FakeClickHandler() override = default;

  std::string get_package_name() { return package_name; }

  void OnRecentAppClicked(
      const Notification::AppMetadata& app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint entrypoint) override {
    package_name = app_metadata.package_name;
  }

 private:
  std::string package_name;
};

}  // namespace

class RecentAppsInteractionHandlerTest : public testing::Test {
 protected:
  RecentAppsInteractionHandlerTest() = default;
  RecentAppsInteractionHandlerTest(const RecentAppsInteractionHandlerTest&) =
      delete;
  RecentAppsInteractionHandlerTest& operator=(
      const RecentAppsInteractionHandlerTest&) = delete;
  ~RecentAppsInteractionHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    RecentAppsInteractionHandlerImpl::RegisterPrefs(pref_service_.registry());
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    interaction_handler_ = std::make_unique<RecentAppsInteractionHandlerImpl>(
        &pref_service_, fake_multidevice_setup_client_.get(),
        &fake_multidevice_feature_access_manager_);
    interaction_handler_->AddRecentAppClickObserver(&fake_click_handler_);

    eche_connection_status_handler_ =
        std::make_unique<eche_app::EcheConnectionStatusHandler>();

    interaction_handler_->SetConnectionStatusHandler(
        eche_connection_status_handler_.get());
  }

  void TearDown() override {
    interaction_handler_->RemoveRecentAppClickObserver(&fake_click_handler_);
  }

  void SaveRecentAppsToPref() {
    const char16_t app_visible_name1[] = u"Fake App";
    const char package_name1[] = "com.fakeapp";
    const int64_t expected_user_id1 = 1;
    auto app_metadata1 = Notification::AppMetadata(
        app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/std::nullopt,
        /*icon_color=*/kIconColor, /*icon_is_monochrome=*/true,
        expected_user_id1, proto::AppStreamabilityStatus::STREAMABLE);

    const char16_t app_visible_name2[] = u"Fake App2";
    const char package_name2[] = "com.fakeapp2";
    const int64_t expected_user_id2 = 2;
    auto app_metadata2 = Notification::AppMetadata(
        app_visible_name2, package_name2, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/gfx::Image(),
        /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/false,
        expected_user_id2, proto::AppStreamabilityStatus::STREAMABLE);

    base::Value::List app_metadata_value_list;
    app_metadata_value_list.Append(app_metadata1.ToValue());
    app_metadata_value_list.Append(app_metadata2.ToValue());

    pref_service_.SetList(prefs::kRecentAppsHistory,
                          std::move(app_metadata_value_list));
  }

  void SaveLegacyRecentAppToPref() {
    const char16_t app_visible_name1[] = u"Fake App";
    const char package_name1[] = "com.fakeapp";
    const int64_t expected_user_id1 = 1;
    base::Value::Dict app_metadata_value =
        Notification::AppMetadata(
            app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
            /*monochrome_icon_mask=*/std::nullopt,
            /*icon_color=*/kIconColor, /*icon_is_monochrome=*/false,
            expected_user_id1, proto::AppStreamabilityStatus::STREAMABLE)
            .ToValue();

    // Simulate an un-migrated preference without new fields.
    EXPECT_TRUE(app_metadata_value.Remove(kIconIsMonochrome));
    EXPECT_TRUE(app_metadata_value.Remove(kIconColorR));
    EXPECT_TRUE(app_metadata_value.Remove(kIconColorG));
    EXPECT_TRUE(app_metadata_value.Remove(kIconColorB));

    base::Value::List app_metadata_value_list;
    app_metadata_value_list.Append(std::move(app_metadata_value));

    pref_service_.SetList(prefs::kRecentAppsHistory,
                          std::move(app_metadata_value_list));
  }

  std::string GetPackageName() {
    return fake_click_handler_.get_package_name();
  }

  RecentAppsInteractionHandlerImpl& handler() { return *interaction_handler_; }

  void SetEcheFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kEche, feature_state);
  }

  void SetPhoneHubNotificationsFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kPhoneHubNotifications,
        feature_state);
  }

  void SetHostStatus(HostStatus host_status) {
    fake_multidevice_setup_client_->SetHostStatusWithDevice(
        std::make_pair(host_status, std::nullopt /* host_device */));
  }

  void SetNotificationAccess(bool enabled) {
    fake_multidevice_feature_access_manager_
        .SetNotificationAccessStatusInternal(
            enabled
                ? MultideviceFeatureAccessManager::AccessStatus::kAccessGranted
                : MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted,
            MultideviceFeatureAccessManager::AccessProhibitedReason::kUnknown);
  }

  void SetAppsAccessStatus(bool enabled) {
    fake_multidevice_feature_access_manager_.SetAppsAccessStatusInternal(
        enabled ? MultideviceFeatureAccessManager::AccessStatus::kAccessGranted
                : MultideviceFeatureAccessManager::AccessStatus::
                      kAvailableButNotGranted);
  }

  std::vector<RecentAppsInteractionHandler::UserState> GetDefaultUserStates() {
    RecentAppsInteractionHandler::UserState user_state1;
    user_state1.user_id = 1;
    user_state1.is_enabled = true;
    RecentAppsInteractionHandler::UserState user_state2;
    user_state2.user_id = 2;
    user_state2.is_enabled = true;
    std::vector<RecentAppsInteractionHandler::UserState> user_states;
    user_states.push_back(user_state1);
    user_states.push_back(user_state2);
    return user_states;
  }

  std::vector<RecentAppsInteractionHandler::UserState>
  GetWorkProfileTurnedOffUserStates() {
    RecentAppsInteractionHandler::UserState user_state1;
    user_state1.user_id = 1;
    user_state1.is_enabled = true;
    RecentAppsInteractionHandler::UserState user_state2;
    user_state2.user_id = 2;
    user_state2.is_enabled = false;
    std::vector<RecentAppsInteractionHandler::UserState> user_states;
    user_states.push_back(user_state1);
    user_states.push_back(user_state2);
    return user_states;
  }

  void GenerateDefaultAppMetadata() {
    const base::Time now = base::Time::Now();
    const char16_t app_visible_name1[] = u"Fake App1";
    const char package_name1[] = "com.fakeapp1";
    const int64_t expected_user_id1 = 1;
    auto app_metadata1 = Notification::AppMetadata(
        app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/std::nullopt,
        /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
        expected_user_id1, proto::AppStreamabilityStatus::STREAMABLE);
    const char16_t app_visible_name2[] = u"Fake App2";
    const char package_name2[] = "com.fakeapp2";
    const int64_t expected_user_id2 = 2;
    auto app_metadata2 = Notification::AppMetadata(
        app_visible_name2, package_name2, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/gfx::Image(),
        /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
        expected_user_id2, proto::AppStreamabilityStatus::STREAMABLE);
    handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
    handler().NotifyRecentAppAddedOrUpdated(app_metadata2, now);
  }

  void GenerateAppMetadataWithDuplicateUserId() {
    const base::Time now = base::Time::Now();
    const char16_t app_visible_name1[] = u"Fake App1";
    const char package_name1[] = "com.fakeapp1";
    const int64_t expected_user_id = 1;
    auto app_metadata1 = Notification::AppMetadata(
        app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/std::nullopt,
        /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
        expected_user_id, proto::AppStreamabilityStatus::STREAMABLE);
    const char16_t app_visible_name2[] = u"Fake App2";
    const char package_name2[] = "com.fakeapp2";
    auto app_metadata2 = Notification::AppMetadata(
        app_visible_name2, package_name2, /*color_icon=*/gfx::Image(),
        /*monochrome_icon_mask=*/gfx::Image(),
        /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
        expected_user_id, proto::AppStreamabilityStatus::STREAMABLE);
    handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
    handler().NotifyRecentAppAddedOrUpdated(app_metadata2, now);
  }

  void NotifyConnectionStatusChanged(ConnectionStatus connection_status) {
    eche_connection_status_handler_->OnConnectionStatusChanged(
        connection_status);
  }

  void SetConnectionStatus(ConnectionStatus connection_status) {
    interaction_handler_->set_connection_status_for_testing(connection_status);
  }

  RecentAppsInteractionHandler::RecentAppsUiState
  GetUiStateFromConnectionStatus() {
    return interaction_handler_->GetUiStateFromConnectionStatus();
  }

  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  base::test::ScopedFeatureList feature_list_;

 private:
  FakeClickHandler fake_click_handler_;
  std::unique_ptr<eche_app::EcheConnectionStatusHandler>
      eche_connection_status_handler_;
  TestingPrefServiceSimple pref_service_;
  FakeMultideviceFeatureAccessManager fake_multidevice_feature_access_manager_;
  std::unique_ptr<RecentAppsInteractionHandlerImpl> interaction_handler_;
};

TEST_F(RecentAppsInteractionHandlerTest, RecentAppsClicked) {
  const char16_t expected_app_visible_name[] = u"Fake App";
  const char expected_package_name[] = "com.fakeapp";
  const int64_t expected_user_id = 1;
  auto expected_app_metadata = Notification::AppMetadata(
      expected_app_visible_name, expected_package_name,
      /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
      expected_user_id, proto::AppStreamabilityStatus::STREAMABLE);

  handler().NotifyRecentAppClicked(
      expected_app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint::RECENT_APPS);

  EXPECT_EQ(expected_package_name, GetPackageName());
}

TEST_F(RecentAppsInteractionHandlerTest, RecentAppsClickedHasOriginalIcon) {
  const char16_t expected_app_visible_name[] = u"Fake App";
  const char expected_package_name[] = "com.fakeapp";
  const int64_t expected_user_id = 1;
  auto expected_app_metadata = Notification::AppMetadata(
      expected_app_visible_name, expected_package_name,
      /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/gfx::Image(),
      /*icon_color=*/std::nullopt, /*icon_is_monochrome=*/true,
      expected_user_id, proto::AppStreamabilityStatus::STREAMABLE);

  handler().NotifyRecentAppClicked(
      expected_app_metadata,
      eche_app::mojom::AppStreamLaunchEntryPoint::RECENT_APPS);

  EXPECT_EQ(expected_package_name, GetPackageName());
}

TEST_F(RecentAppsInteractionHandlerTest, RecentAppsUpdated) {
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);

  const char16_t app_visible_name2[] = u"Fake App2";
  const char package_name2[] = "com.fakeapp2";
  const int64_t expected_user_id2 = 2;
  auto app_metadata2 = Notification::AppMetadata(
      app_visible_name2, package_name2, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/gfx::Image(),
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id2,
      proto::AppStreamabilityStatus::STREAMABLE);
  const base::Time now = base::Time::Now();

  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
  EXPECT_EQ(1U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ(now,
            handler().recent_app_metadata_list_for_testing()->at(0).second);

  // The same package name only update last accessed timestamp.
  const base::Time next_minute = base::Time::Now() + base::Minutes(1);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, next_minute);
  EXPECT_EQ(1U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ(next_minute,
            handler().recent_app_metadata_list_for_testing()->at(0).second);

  const base::Time next_hour = base::Time::Now() + base::Hours(1);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata2, next_hour);
  EXPECT_EQ(2U, handler().recent_app_metadata_list_for_testing()->size());
}

TEST_F(RecentAppsInteractionHandlerTest, SetStreamableApps) {
  std::vector<Notification::AppMetadata> streamable_apps;
  streamable_apps.emplace_back(u"App1", "com.fakeapp1",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/std::nullopt,
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);
  streamable_apps.emplace_back(u"App2", "com.fakeapp2",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/gfx::Image(),
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);

  handler().SetStreamableApps(streamable_apps);

  EXPECT_EQ(2U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ("com.fakeapp1", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(0)
                                .first.package_name);
  EXPECT_EQ("com.fakeapp2", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(1)
                                .first.package_name);
}

TEST_F(RecentAppsInteractionHandlerTest,
       SetStreamableApps_ClearsPreviousState) {
  std::vector<Notification::AppMetadata> streamable_apps;
  streamable_apps.emplace_back(u"App1", "com.fakeapp1",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/std::nullopt,
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);
  streamable_apps.emplace_back(u"App2", "com.fakeapp2",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/gfx::Image(),
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);

  handler().SetStreamableApps(streamable_apps);

  EXPECT_EQ(2U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ("com.fakeapp1", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(0)
                                .first.package_name);
  EXPECT_EQ("com.fakeapp2", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(1)
                                .first.package_name);

  std::vector<Notification::AppMetadata> streamable_apps2;
  streamable_apps2.emplace_back(u"App3", "com.fakeapp3",
                                /*color_icon=*/gfx::Image(),
                                /*monochrome_icon_mask=*/std::nullopt,
                                /*icon_color=*/std::nullopt,
                                /*icon_is_monochrome=*/true, 1,
                                proto::AppStreamabilityStatus::STREAMABLE);

  handler().SetStreamableApps(streamable_apps2);

  EXPECT_EQ(1U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ("com.fakeapp3", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(0)
                                .first.package_name);
}

TEST_F(RecentAppsInteractionHandlerTest, SetStreamableApps_EmptyList) {
  std::vector<Notification::AppMetadata> streamable_apps;

  handler().SetStreamableApps(streamable_apps);

  EXPECT_TRUE(handler().recent_app_metadata_list_for_testing()->empty());
}

TEST_F(RecentAppsInteractionHandlerTest, RemoveStreamableApp) {
  std::vector<Notification::AppMetadata> streamable_apps;
  streamable_apps.emplace_back(u"App1", "com.fakeapp1",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/std::nullopt,
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);
  streamable_apps.emplace_back(u"App2", "com.fakeapp2",
                               /*color_icon=*/gfx::Image(),
                               /*monochrome_icon_mask=*/gfx::Image(),
                               /*icon_color=*/std::nullopt,
                               /*icon_is_monochrome=*/true, 1,
                               proto::AppStreamabilityStatus::STREAMABLE);

  handler().SetStreamableApps(streamable_apps);

  EXPECT_EQ(2U, handler().recent_app_metadata_list_for_testing()->size());

  auto app_to_remove = proto::App();
  app_to_remove.set_package_name("com.fakeapp1");
  app_to_remove.set_visible_name("App1");
  handler().RemoveStreamableApp(app_to_remove);
  EXPECT_EQ(1U, handler().recent_app_metadata_list_for_testing()->size());
  EXPECT_EQ("com.fakeapp2", handler()
                                .recent_app_metadata_list_for_testing()
                                ->at(0)
                                .first.package_name);
}

TEST_F(RecentAppsInteractionHandlerTest, FetchRecentAppMetadataList) {
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);

  const char16_t app_visible_name2[] = u"Fake App2";
  const char package_name2[] = "com.fakeapp2";
  const int64_t expected_user_id2 = 1;
  auto app_metadata2 = Notification::AppMetadata(
      app_visible_name2, package_name2, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/gfx::Image(),
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id2,
      proto::AppStreamabilityStatus::STREAMABLE);

  const char16_t app_visible_name3[] = u"Fake App3";
  const char package_name3[] = "com.fakeapp3";
  const int64_t expected_user_id3 = 1;
  auto app_metadata3 = Notification::AppMetadata(
      app_visible_name3, package_name3, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id3,
      proto::AppStreamabilityStatus::STREAMABLE);

  const base::Time now = base::Time::Now();
  const base::Time next_minute = base::Time::Now() + base::Minutes(1);
  const base::Time next_hour = base::Time::Now() + base::Hours(1);

  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata2, next_minute);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata3, next_hour);

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      GetDefaultUserStates();
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(3U, recent_apps_metadata_result.size());
  EXPECT_EQ(package_name3, recent_apps_metadata_result[0].package_name);
  EXPECT_EQ(package_name2, recent_apps_metadata_result[1].package_name);
  EXPECT_EQ(package_name1, recent_apps_metadata_result[2].package_name);

  const char16_t app_visible_name4[] = u"Fake App4";
  const char package_name4[] = "com.fakeapp4";
  const int64_t expected_user_id4 = 1;
  auto app_metadata4 = Notification::AppMetadata(
      app_visible_name4, package_name4, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id4,
      proto::AppStreamabilityStatus::STREAMABLE);

  const char16_t app_visible_name5[] = u"Fake App5";
  const char package_name5[] = "com.fakeapp5";
  const int64_t expected_user_id5 = 1;
  auto app_metadata5 = Notification::AppMetadata(
      app_visible_name5, package_name5, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id5,
      proto::AppStreamabilityStatus::STREAMABLE);

  const base::Time next_two_hour = base::Time::Now() + base::Hours(2);
  const base::Time next_three_hour = base::Time::Now() + base::Hours(3);

  handler().NotifyRecentAppAddedOrUpdated(app_metadata4, next_two_hour);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata5, next_three_hour);

  const size_t max_most_recent_apps = 5;
  recent_apps_metadata_result = handler().FetchRecentAppMetadataList();
  EXPECT_EQ(max_most_recent_apps, recent_apps_metadata_result.size());

  EXPECT_EQ(package_name5, recent_apps_metadata_result[0].package_name);
  EXPECT_EQ(package_name4, recent_apps_metadata_result[1].package_name);
  EXPECT_EQ(package_name3, recent_apps_metadata_result[2].package_name);
  EXPECT_EQ(package_name2, recent_apps_metadata_result[3].package_name);
  EXPECT_EQ(package_name1, recent_apps_metadata_result[4].package_name);
}

TEST_F(RecentAppsInteractionHandlerTest,
       FetchRecentAppMetadataListFromPreference) {
  SaveRecentAppsToPref();

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      GetDefaultUserStates();
  handler().set_user_states(user_states);

  const char package_name1[] = "com.fakeapp";
  const char package_name2[] = "com.fakeapp2";
  const size_t number_of_recent_apps_in_preference = 2;

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(number_of_recent_apps_in_preference,
            recent_apps_metadata_result.size());
  EXPECT_EQ(package_name1, recent_apps_metadata_result[0].package_name);
  EXPECT_EQ(package_name2, recent_apps_metadata_result[1].package_name);

  // Check de/serialization of icon metadata
  EXPECT_TRUE(recent_apps_metadata_result[0].icon_color.has_value());
  EXPECT_TRUE(*recent_apps_metadata_result[0].icon_color == kIconColor);
  EXPECT_TRUE(recent_apps_metadata_result[0].icon_is_monochrome);
  EXPECT_FALSE(recent_apps_metadata_result[1].icon_color.has_value());
  EXPECT_FALSE(recent_apps_metadata_result[1].icon_is_monochrome);
}

TEST_F(RecentAppsInteractionHandlerTest,
       FetchRecentAppMetadataListFromPreferenceBackwardsCompat) {
  SaveLegacyRecentAppToPref();

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      GetDefaultUserStates();
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();
  EXPECT_EQ(1u, recent_apps_metadata_result.size());

  // Check that new fields are appropriately filled in with safe defaults.
  EXPECT_FALSE(recent_apps_metadata_result[0].icon_color.has_value());
  EXPECT_FALSE(recent_apps_metadata_result[0].icon_is_monochrome);
}

TEST_F(RecentAppsInteractionHandlerTest,
       OnFeatureStatesChangedToDisabledWithEmptyRecentAppsList) {
  SetEcheFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       OnFeatureStatesChangedToDisabledWithNonEmptyRecentAppsList) {
  const base::Time now = base::Time::Now();
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);

  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
  SetEcheFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       OnFeatureStatesChangedToEnabledWithEmptyRecentAppsList) {
  SetEcheFeatureState(FeatureState::kEnabledByUser);
  SetPhoneHubNotificationsFeatureState(FeatureState::kEnabledByUser);
  SetAppsAccessStatus(true);
  SetNotificationAccess(true);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::LOADING,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       DisableNotificationAccessWithEmptyRecentAppsList) {
  SetEcheFeatureState(FeatureState::kEnabledByUser);
  SetPhoneHubNotificationsFeatureState(FeatureState::kEnabledByUser);
  SetAppsAccessStatus(true);
  SetNotificationAccess(true);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::LOADING,
            handler().ui_state());

  // Disable apps access permission on the host device.
  SetAppsAccessStatus(false);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Disable apps access permission on the local device.
  SetAppsAccessStatus(true);
  SetEcheFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Disable apps access permission on both devices.
  SetAppsAccessStatus(false);
  SetEcheFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Enable apps access permission back on both devices.
  SetAppsAccessStatus(true);
  SetEcheFeatureState(FeatureState::kEnabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::LOADING,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       OnFeatureStatesChangedToEnabledWithNonEmptyRecentAppsList) {
  const base::Time now = base::Time::Now();
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);
  SetConnectionStatus(ConnectionStatus::kConnectionStatusConnected);
  SetAppsAccessStatus(true);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
  SetEcheFeatureState(FeatureState::kEnabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       DisableNotificationAccessWithNonEmptyRecentAppsList) {
  const base::Time now = base::Time::Now();
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);

  SetConnectionStatus(ConnectionStatus::kConnectionStatusConnected);
  SetAppsAccessStatus(true);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);
  SetEcheFeatureState(FeatureState::kEnabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());

  // Disable notification access permission on the host device.
  SetNotificationAccess(false);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());

  // Disable notification access permission on the local device.
  SetNotificationAccess(true);
  SetPhoneHubNotificationsFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());

  // Disable notification access permission on both devices.
  SetNotificationAccess(false);
  SetPhoneHubNotificationsFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       UiStateChangedToVisibleWhenRecentAppBeAdded) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  SetEcheFeatureState(FeatureState::kEnabledByUser);
  SetPhoneHubNotificationsFeatureState(FeatureState::kEnabledByUser);
  SetConnectionStatus(ConnectionStatus::kConnectionStatusConnected);
  SetAppsAccessStatus(true);
  SetNotificationAccess(true);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::PLACEHOLDER_VIEW,
            handler().ui_state());

  const base::Time now = base::Time::Now();
  const char16_t app_visible_name1[] = u"Fake App";
  const char package_name1[] = "com.fakeapp";
  const int64_t expected_user_id1 = 1;
  auto app_metadata1 = Notification::AppMetadata(
      app_visible_name1, package_name1, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id1,
      proto::AppStreamabilityStatus::STREAMABLE);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata1, now);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest, DisableAppsAccess) {
  GenerateDefaultAppMetadata();

  // The apps access has not been granted yet so the UI state always HIDDEN.
  SetAppsAccessStatus(true);

  SetPhoneHubNotificationsFeatureState(FeatureState::kEnabledByUser);
  SetNotificationAccess(true);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  SetNotificationAccess(false);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Disable notification access permission on the local device.
  SetNotificationAccess(true);
  SetPhoneHubNotificationsFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Disable notification access permission on both devices.
  SetNotificationAccess(false);
  SetPhoneHubNotificationsFeatureState(FeatureState::kDisabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());

  // Enable notification access permission back on both devices.
  SetNotificationAccess(true);
  SetPhoneHubNotificationsFeatureState(FeatureState::kEnabledByUser);

  EXPECT_EQ(RecentAppsInteractionHandler::RecentAppsUiState::HIDDEN,
            handler().ui_state());
}

TEST_F(RecentAppsInteractionHandlerTest,
       PrefBeClearedWhenFeatureStatesChangedToUnavailableNoVerifiedHost) {
  SaveRecentAppsToPref();
  SetHostStatus(HostStatus::kHostSetButNotYetVerified);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 0u);
}

TEST_F(
    RecentAppsInteractionHandlerTest,
    RecentAppsListBeClearedWhenFeatureStatesChangedToUnavailableNoVerifiedHost) {
  const base::Time now = base::Time::Now();
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  const int64_t expected_user_id = 1;
  auto app_metadata = Notification::AppMetadata(
      app_visible_name, package_name, /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt,
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, expected_user_id,
      proto::AppStreamabilityStatus::STREAMABLE);
  handler().NotifyRecentAppAddedOrUpdated(app_metadata, now);
  SetHostStatus(HostStatus::kHostSetButNotYetVerified);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 0u);
}

TEST_F(RecentAppsInteractionHandlerTest,
       ShowAllRecentAppsOfAllUsersWithQuietModeOff) {
  GenerateDefaultAppMetadata();

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      GetDefaultUserStates();
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 2u);
  EXPECT_EQ(2, recent_apps_metadata_result[0].user_id);
  EXPECT_EQ("com.fakeapp2", recent_apps_metadata_result[0].package_name);
  EXPECT_EQ(1, recent_apps_metadata_result[1].user_id);
  EXPECT_EQ("com.fakeapp1", recent_apps_metadata_result[1].package_name);
}

TEST_F(RecentAppsInteractionHandlerTest, ShowRecentAppsOfUserWithQuietModeOn) {
  GenerateDefaultAppMetadata();

  std::vector<RecentAppsInteractionHandler::UserState> user_states =
      GetWorkProfileTurnedOffUserStates();
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 1u);
  EXPECT_EQ(1, recent_apps_metadata_result[0].user_id);
  EXPECT_EQ("com.fakeapp1", recent_apps_metadata_result[0].package_name);
}

TEST_F(RecentAppsInteractionHandlerTest, ShowRecentAppsWhenGetsEmptyUser) {
  GenerateDefaultAppMetadata();

  std::vector<RecentAppsInteractionHandler::UserState> user_states;
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 2u);
  // LIFO scheme is used, hence app 2 is first as it was pushed last.
  EXPECT_EQ(2, recent_apps_metadata_result[0].user_id);
  EXPECT_EQ(1, recent_apps_metadata_result[1].user_id);
}

TEST_F(RecentAppsInteractionHandlerTest, GetUserIdSet) {
  GenerateAppMetadataWithDuplicateUserId();

  std::vector<RecentAppsInteractionHandler::UserState> user_states;
  handler().set_user_states(user_states);

  std::vector<Notification::AppMetadata> recent_apps_metadata_result =
      handler().FetchRecentAppMetadataList();

  EXPECT_EQ(recent_apps_metadata_result.size(), 2u);
  EXPECT_EQ(1, recent_apps_metadata_result[0].user_id);
  EXPECT_EQ(1, recent_apps_metadata_result[1].user_id);
}

TEST_F(RecentAppsInteractionHandlerTest, OnConnectionStatusChanged) {
  // Start in the Disconnected state.
  // Handler will only change connection state for Connected and Failed.
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  // Any transition to "connected" should update ConnUX to connected.
  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusConnected);

  // Only background failures update the ConnUX state, app stream failures
  // should not.
  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusConnected);
  NotifyConnectionStatusChanged(
      ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusConnected);
}

TEST_F(RecentAppsInteractionHandlerTest,
       OnConnectionStatusChangedFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  // Start in the Disconnected state. When flag is disabled, the state should
  // never change.
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusChanged(ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);

  NotifyConnectionStatusChanged(
      ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);
}

TEST_F(RecentAppsInteractionHandlerTest, GetUiStateFromConnectionStatus) {
  RecentAppsInteractionHandler::RecentAppsUiState ui_state;

  SetConnectionStatus(ConnectionStatus::kConnectionStatusDisconnected);
  ui_state = GetUiStateFromConnectionStatus();
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(ui_state, RecentAppsInteractionHandler::RecentAppsUiState::LOADING);

  SetConnectionStatus(ConnectionStatus::kConnectionStatusConnecting);
  ui_state = GetUiStateFromConnectionStatus();
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(ui_state, RecentAppsInteractionHandler::RecentAppsUiState::LOADING);

  SetConnectionStatus(ConnectionStatus::kConnectionStatusConnected);
  ui_state = GetUiStateFromConnectionStatus();
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(ui_state,
            RecentAppsInteractionHandler::RecentAppsUiState::ITEMS_VISIBLE);

  SetConnectionStatus(ConnectionStatus::kConnectionStatusFailed);
  ui_state = GetUiStateFromConnectionStatus();
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(ui_state,
            RecentAppsInteractionHandler::RecentAppsUiState::CONNECTION_FAILED);

  SetConnectionStatus(ConnectionStatus::kConnectionStatusDisconnected);
  ui_state = GetUiStateFromConnectionStatus();
  EXPECT_EQ(handler().connection_status_for_testing(),
            ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(ui_state, RecentAppsInteractionHandler::RecentAppsUiState::LOADING);
}

}  // namespace ash::phonehub
