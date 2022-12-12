// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include <memory>
#include <vector>

#include "chromeos/ash/components/phonehub/notification.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

class FakeObserver : public AppStreamLauncherDataModel::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  // AppStreamLauncherDataModel::Observer
  void OnShouldShowMiniLauncherChanged() override {
    is_should_show_mini_launcher_changed_ = true;
  }

  void OnAppListChanged() override { is_app_list_changed_ = true; }

  bool IsShouldShowMiniLauncherChanged() const {
    return is_should_show_mini_launcher_changed_;
  }

  bool IsAppListChanged() const { return is_app_list_changed_; }

 private:
  bool is_should_show_mini_launcher_changed_ = false;
  bool is_app_list_changed_ = false;
};
}  // namespace

class AppStreamLauncherDataModelTest : public testing::Test {
 protected:
  AppStreamLauncherDataModelTest() = default;
  AppStreamLauncherDataModelTest(const AppStreamLauncherDataModelTest&) =
      delete;
  AppStreamLauncherDataModelTest& operator=(
      const AppStreamLauncherDataModelTest&) = delete;
  ~AppStreamLauncherDataModelTest() override = default;

  void SetUp() override {
    app_stream_launcher_data_launcher_ =
        std::make_unique<AppStreamLauncherDataModel>();
    app_stream_launcher_data_launcher_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    app_stream_launcher_data_launcher_->RemoveObserver(&fake_observer_);
  }

  bool IsObserverShouldShowMiniLauncherChanged() const {
    return fake_observer_.IsShouldShowMiniLauncherChanged();
  }

  bool IsObserverAppListChanged() const {
    return fake_observer_.IsAppListChanged();
  }

  void SetShouldShowMiniLauncher(bool should_show_mini_launcher) {
    app_stream_launcher_data_launcher_->SetShouldShowMiniLauncher(
        should_show_mini_launcher);
  }

  bool GetShouldShowMiniLauncher() const {
    return app_stream_launcher_data_launcher_->GetShouldShowMiniLauncher();
  }

  void ResetState() { app_stream_launcher_data_launcher_->ResetState(); }

  void SetAppList(
      const std::vector<Notification::AppMetadata>& streamable_apps) {
    app_stream_launcher_data_launcher_->SetAppList(streamable_apps);
  }

  const std::vector<Notification::AppMetadata>* GetAppsList() {
    return app_stream_launcher_data_launcher_->GetAppsList();
  }

  const std::vector<Notification::AppMetadata>* GetAppsListSortedByName() {
    return app_stream_launcher_data_launcher_->GetAppsListSortedByName();
  }

 private:
  std::unique_ptr<AppStreamLauncherDataModel>
      app_stream_launcher_data_launcher_;
  FakeObserver fake_observer_;
};

TEST_F(AppStreamLauncherDataModelTest, SetShouldShowMiniLauncher) {
  SetShouldShowMiniLauncher(true);
  EXPECT_TRUE(GetShouldShowMiniLauncher());
  EXPECT_TRUE(IsObserverShouldShowMiniLauncherChanged());
}

TEST_F(AppStreamLauncherDataModelTest, ResetState) {
  ResetState();
  EXPECT_FALSE(GetShouldShowMiniLauncher());
}

TEST_F(AppStreamLauncherDataModelTest, SetAppsList) {
  std::vector<Notification::AppMetadata> apps_list;
  apps_list.emplace_back(Notification::AppMetadata(
      u"b_app", "com.fakeapp1", gfx::Image(), absl::nullopt, true, 1,
      proto::AppStreamabilityStatus::STREAMABLE));
  apps_list.emplace_back(Notification::AppMetadata(
      u"a_app", "com.fakeapp2", gfx::Image(), absl::nullopt, true, 1,
      proto::AppStreamabilityStatus::STREAMABLE));
  SetAppList(apps_list);
  EXPECT_TRUE(IsObserverAppListChanged());
  EXPECT_EQ(GetAppsList()->size(), 2u);
  EXPECT_EQ(GetAppsList()->at(0).visible_app_name, u"b_app");
  EXPECT_EQ(GetAppsList()->at(1).visible_app_name, u"a_app");
  EXPECT_EQ(GetAppsListSortedByName()->size(), 2u);
  EXPECT_EQ(GetAppsListSortedByName()->at(0).visible_app_name, u"a_app");
  EXPECT_EQ(GetAppsListSortedByName()->at(1).visible_app_name, u"b_app");
}
}  // namespace phonehub
}  // namespace ash
