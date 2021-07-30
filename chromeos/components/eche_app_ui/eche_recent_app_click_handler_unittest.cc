// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/eche_recent_app_click_handler.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/eche_app_ui/fake_feature_status_provider.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace eche_app {

class EcheRecentAppClickHandlerTest : public testing::Test {
 protected:
  EcheRecentAppClickHandlerTest() = default;
  EcheRecentAppClickHandlerTest(const EcheRecentAppClickHandlerTest&) = delete;
  EcheRecentAppClickHandlerTest& operator=(
      const EcheRecentAppClickHandlerTest&) = delete;
  ~EcheRecentAppClickHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_phone_hub_manager_.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    fake_feature_status_provider_.SetStatus(FeatureStatus::kIneligible);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kPhoneHubRecentApps},
        /*disabled_features=*/{});
    handler_ = std::make_unique<EcheRecentAppClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)));
  }

  void FakeLaunchEcheAppFunction(const std::string& package_name) {
    package_name_ = package_name;
  }

  void SetStatus(FeatureStatus status) {
    fake_feature_status_provider_.SetStatus(status);
  }

  size_t GetNumberOfRecentAppsInteractionHandlers() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->recent_app_click_observer_count();
  }

  void RecentAppClicked(const std::string& package_name) {
    handler_->OnRecentAppClicked(package_name);
  }

  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->HandleNotificationClick(notification_id, app_metadata);
  }

  std::vector<phonehub::Notification::AppMetadata>
  FetchRecentAppMetadataList() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->FetchRecentAppMetadataList();
  }

  const std::string& package_name() { return package_name_; }

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  eche_app::FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<EcheRecentAppClickHandler> handler_;
  std::string package_name_;
};

TEST_F(EcheRecentAppClickHandlerTest, StatusChangeTransitions) {
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
}

TEST_F(EcheRecentAppClickHandlerTest, LaunchEcheAppFunction) {
  const char expected_package_name[] = "com.fakeapp";

  RecentAppClicked(expected_package_name);

  EXPECT_EQ(expected_package_name, package_name());
}

TEST_F(EcheRecentAppClickHandlerTest, HandleNotificationClick) {
  const int64_t notification_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image());

  HandleNotificationClick(notification_id, fake_app_metadata);
  std::vector<phonehub::Notification::AppMetadata> app_metadata =
      FetchRecentAppMetadataList();

  EXPECT_EQ(fake_app_metadata.visible_app_name,
            app_metadata[0].visible_app_name);
  EXPECT_EQ(fake_app_metadata.package_name, app_metadata[0].package_name);
}

}  // namespace eche_app
}  // namespace chromeos
