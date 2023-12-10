// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include <memory>
#include <vector>

#include "chromeos/ash/components/phonehub/notification.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {

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

  void AddAppToList(const Notification::AppMetadata& app_to_add) {
    app_stream_launcher_data_launcher_->AddAppToList(app_to_add);
  }

  void RemoveAppFromList(const proto::App app_to_remove) {
    app_stream_launcher_data_launcher_->RemoveAppFromList(app_to_remove);
  }

  const std::vector<Notification::AppMetadata>* GetAppsList() {
    return app_stream_launcher_data_launcher_->GetAppsList();
  }

  const std::vector<Notification::AppMetadata>* GetAppsListSortedByName() {
    return app_stream_launcher_data_launcher_->GetAppsListSortedByName();
  }

  void SetLauncherSize(int height, int width) {
    app_stream_launcher_data_launcher_->SetLauncherSize(height, width);
  }

  int GetLauncherHeight() {
    return app_stream_launcher_data_launcher_->launcher_height();
  }

  int GetLauncherWidth() {
    return app_stream_launcher_data_launcher_->launcher_width();
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
  apps_list.emplace_back(u"GPay", "com.fakeapp1", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  apps_list.emplace_back(u"Gboard", "com.fakeapp2", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  SetAppList(apps_list);
  EXPECT_TRUE(IsObserverAppListChanged());
  EXPECT_EQ(GetAppsList()->size(), 2u);
  EXPECT_EQ(GetAppsList()->at(0).visible_app_name, u"GPay");
  EXPECT_EQ(GetAppsList()->at(1).visible_app_name, u"Gboard");
  EXPECT_EQ(GetAppsListSortedByName()->size(), 2u);
  EXPECT_EQ(GetAppsListSortedByName()->at(0).visible_app_name, u"Gboard");
  EXPECT_EQ(GetAppsListSortedByName()->at(1).visible_app_name, u"GPay");
}

TEST_F(AppStreamLauncherDataModelTest, AddAppToList) {
  std::vector<Notification::AppMetadata> apps_list;
  apps_list.emplace_back(u"GPay", "com.fakeapp1", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  apps_list.emplace_back(u"Gboard", "com.fakeapp2", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  SetAppList(apps_list);
  AddAppToList(Notification::AppMetadata(
      u"added_app", "com.fakeapp3", /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt, /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, /*user_id=*/1,
      proto::AppStreamabilityStatus::STREAMABLE));
  AddAppToList(Notification::AppMetadata(
      u"a_added_app", "com.fakeapp3", /*color_icon=*/gfx::Image(),
      /*monochrome_icon_mask=*/std::nullopt, /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true, /*user_id=*/1,
      proto::AppStreamabilityStatus::STREAMABLE));
  EXPECT_TRUE(IsObserverAppListChanged());
  EXPECT_EQ(GetAppsList()->size(), 4u);
  EXPECT_EQ(GetAppsList()->at(0).visible_app_name, u"GPay");
  EXPECT_EQ(GetAppsList()->at(1).visible_app_name, u"Gboard");
  EXPECT_EQ(GetAppsList()->at(2).visible_app_name, u"added_app");
  EXPECT_EQ(GetAppsList()->at(3).visible_app_name, u"a_added_app");
  EXPECT_EQ(GetAppsListSortedByName()->size(), 4u);
  EXPECT_EQ(GetAppsListSortedByName()->at(0).visible_app_name, u"a_added_app");
  EXPECT_EQ(GetAppsListSortedByName()->at(1).visible_app_name, u"added_app");
  EXPECT_EQ(GetAppsListSortedByName()->at(2).visible_app_name, u"Gboard");
  EXPECT_EQ(GetAppsListSortedByName()->at(3).visible_app_name, u"GPay");
}

TEST_F(AppStreamLauncherDataModelTest, RemoveAppFromList) {
  std::vector<Notification::AppMetadata> apps_list;
  apps_list.emplace_back(u"GPay", "com.fakeapp1", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  apps_list.emplace_back(u"Gboard", "com.fakeapp2", /*color_icon=*/gfx::Image(),
                         /*monochrome_icon_mask=*/std::nullopt,
                         /*icon_color=*/std::nullopt,
                         /*icon_is_monochrome=*/true, /*user_id=*/1,
                         proto::AppStreamabilityStatus::STREAMABLE);
  SetAppList(apps_list);
  auto app_to_remove = proto::App();
  app_to_remove.set_package_name("com.fakeapp1");
  app_to_remove.set_visible_name("GPay");
  RemoveAppFromList(app_to_remove);
  EXPECT_TRUE(IsObserverAppListChanged());
  EXPECT_EQ(GetAppsList()->size(), 1u);
  EXPECT_EQ(GetAppsList()->at(0).visible_app_name, u"Gboard");
  EXPECT_EQ(GetAppsListSortedByName()->size(), 1u);
  EXPECT_EQ(GetAppsListSortedByName()->at(0).visible_app_name, u"Gboard");
}

TEST_F(AppStreamLauncherDataModelTest, SetLauncherSize) {
  SetLauncherSize(/*height=*/400, /*width=*/300);
  EXPECT_EQ(GetLauncherHeight(), 400);
  EXPECT_EQ(GetLauncherWidth(), 300);
}

}  // namespace ash::phonehub
