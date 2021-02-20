// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/restore_data.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/app_restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace full_restore {

namespace {

constexpr char kAppId1[] = "aaa";
constexpr char kAppId2[] = "bbb";

constexpr int32_t kWindowId1 = 100;
constexpr int32_t kWindowId2 = 200;
constexpr int32_t kWindowId3 = 300;

constexpr int64_t kDisplayId1 = 22000000;
constexpr int64_t kDisplayId2 = 11000000;

constexpr char kFilePath1[] = "path1";
constexpr char kFilePath2[] = "path2";

constexpr char kIntentActionView[] = "view";
constexpr char kIntentActionSend[] = "send";

constexpr char kMimeType[] = "text/plain";

constexpr char kShareText1[] = "text1";
constexpr char kShareText2[] = "text2";

constexpr int32_t kActivationIndex1 = 100;
constexpr int32_t kActivationIndex2 = 101;
constexpr int32_t kActivationIndex3 = 102;

constexpr int32_t kDeskId1 = 1;
constexpr int32_t kDeskId2 = 2;
constexpr int32_t kDeskId3 = 3;

constexpr bool kVisibleOnAllWorkspaces1 = false;
constexpr bool kVisibleOnAllWorkspaces2 = false;
constexpr bool kVisibleOnAllWorkspaces3 = true;

constexpr gfx::Rect kRestoreBounds1(10, 20, 110, 120);
constexpr gfx::Rect kRestoreBounds2(30, 40, 130, 140);
constexpr gfx::Rect kRestoreBounds3(50, 60, 150, 160);

constexpr gfx::Rect kCurrentBounds1(11, 21, 111, 121);
constexpr gfx::Rect kCurrentBounds2(31, 41, 131, 141);
constexpr gfx::Rect kCurrentBounds3(51, 61, 151, 161);

constexpr chromeos::WindowStateType kWindowStateType1 =
    chromeos::WindowStateType::kMaximized;
constexpr chromeos::WindowStateType kWindowStateType2 =
    chromeos::WindowStateType::kInactive;
constexpr chromeos::WindowStateType kWindowStateType3 =
    chromeos::WindowStateType::kFullscreen;

}  // namespace

// Unit tests for restore data.
class RestoreDataTest : public testing::Test {
 public:
  RestoreDataTest() = default;
  ~RestoreDataTest() override = default;

  RestoreDataTest(const RestoreDataTest&) = delete;
  RestoreDataTest& operator=(const RestoreDataTest&) = delete;

  apps::mojom::IntentPtr CreateIntent(const std::string& action,
                                      const std::string& mime_type,
                                      const std::string& share_text) {
    auto intent = apps::mojom::Intent::New();
    intent->action = action;
    intent->mime_type = mime_type;
    intent->share_text = share_text;
    return intent;
  }

  void AddAppLaunchInfos() {
    std::unique_ptr<AppLaunchInfo> app_launch_info1 =
        std::make_unique<AppLaunchInfo>(
            kAppId1, kWindowId1,
            apps::mojom::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
            std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                        base::FilePath(kFilePath2)},
            CreateIntent(kIntentActionSend, kMimeType, kShareText1));

    std::unique_ptr<AppLaunchInfo> app_launch_info2 =
        std::make_unique<AppLaunchInfo>(
            kAppId1, kWindowId2,
            apps::mojom::LaunchContainer::kLaunchContainerTab,
            WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
            std::vector<base::FilePath>{base::FilePath(kFilePath2)},
            CreateIntent(kIntentActionView, kMimeType, kShareText2));

    std::unique_ptr<AppLaunchInfo> app_launch_info3 =
        std::make_unique<AppLaunchInfo>(
            kAppId2, kWindowId3,
            apps::mojom::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_POPUP, kDisplayId2,
            std::vector<base::FilePath>{base::FilePath(kFilePath1)},
            CreateIntent(kIntentActionView, kMimeType, kShareText1));

    restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
    restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
    restore_data().AddAppLaunchInfo(std::move(app_launch_info3));
  }

  void ModifyWindowInfos() {
    WindowInfo window_info1;
    window_info1.activation_index = kActivationIndex1;
    window_info1.desk_id = kDeskId1;
    window_info1.restore_bounds = kRestoreBounds1;
    window_info1.current_bounds = kCurrentBounds1;
    window_info1.window_state_type = kWindowStateType1;
    window_info1.display_id = kDisplayId2;

    WindowInfo window_info2;
    window_info2.activation_index = kActivationIndex2;
    window_info2.desk_id = kDeskId2;
    window_info2.restore_bounds = kRestoreBounds2;
    window_info2.current_bounds = kCurrentBounds2;
    window_info2.window_state_type = kWindowStateType2;
    window_info2.display_id = kDisplayId1;

    WindowInfo window_info3;
    window_info3.activation_index = kActivationIndex3;
    window_info3.desk_id = kDeskId3;
    window_info3.visible_on_all_workspaces = kVisibleOnAllWorkspaces3;
    window_info3.restore_bounds = kRestoreBounds3;
    window_info3.current_bounds = kCurrentBounds3;
    window_info3.window_state_type = kWindowStateType3;
    window_info3.display_id = kDisplayId1;

    restore_data().ModifyWindowInfo(kAppId1, kWindowId1, window_info1);
    restore_data().ModifyWindowInfo(kAppId1, kWindowId2, window_info2);
    restore_data().ModifyWindowInfo(kAppId2, kWindowId3, window_info3);
  }

  void VerifyAppRestoreData(const std::unique_ptr<AppRestoreData>& data,
                            apps::mojom::LaunchContainer container,
                            WindowOpenDisposition disposition,
                            int64_t display_id,
                            std::vector<base::FilePath> file_paths,
                            apps::mojom::IntentPtr intent,
                            int32_t activation_index,
                            int32_t desk_id,
                            bool visible_on_all_workspaces,
                            const gfx::Rect& restore_bounds,
                            const gfx::Rect& current_bounds,
                            chromeos::WindowStateType window_state_type) {
    EXPECT_TRUE(data->container.has_value());
    EXPECT_EQ(static_cast<int>(container), data->container.value());

    EXPECT_TRUE(data->disposition.has_value());
    EXPECT_EQ(static_cast<int>(disposition), data->disposition.value());

    EXPECT_TRUE(data->display_id.has_value());
    EXPECT_EQ(display_id, data->display_id.value());

    EXPECT_TRUE(data->file_paths.has_value());
    EXPECT_EQ(file_paths.size(), data->file_paths.value().size());
    for (size_t i = 0; i < file_paths.size(); i++)
      EXPECT_EQ(file_paths[i], data->file_paths.value()[i]);

    EXPECT_TRUE(data->intent.has_value());
    EXPECT_EQ(intent->action, data->intent.value()->action);
    EXPECT_EQ(intent->mime_type, data->intent.value()->mime_type);
    EXPECT_EQ(intent->share_text, data->intent.value()->share_text);

    EXPECT_TRUE(data->activation_index.has_value());
    EXPECT_EQ(activation_index, data->activation_index.value());

    EXPECT_TRUE(data->desk_id.has_value());
    EXPECT_EQ(desk_id, data->desk_id.value());

    if (!visible_on_all_workspaces)
      // This field should only be written if it is true.
      EXPECT_FALSE(data->visible_on_all_workspaces.has_value());
    else {
      EXPECT_TRUE(data->visible_on_all_workspaces.has_value());
      EXPECT_EQ(visible_on_all_workspaces,
                data->visible_on_all_workspaces.value());
    }

    EXPECT_TRUE(data->restore_bounds.has_value());
    EXPECT_EQ(restore_bounds, data->restore_bounds.value());

    EXPECT_TRUE(data->current_bounds.has_value());
    EXPECT_EQ(current_bounds, data->current_bounds.value());

    EXPECT_TRUE(data->window_state_type.has_value());
    EXPECT_EQ(window_state_type, data->window_state_type.value());
  }

  void VerifyRestoreData(const RestoreData& restore_data) {
    EXPECT_EQ(2u, app_id_to_launch_list(restore_data).size());

    // Verify for |kAppId1|.
    const auto launch_list_it1 =
        app_id_to_launch_list(restore_data).find(kAppId1);
    EXPECT_TRUE(launch_list_it1 != app_id_to_launch_list(restore_data).end());
    EXPECT_EQ(2u, launch_list_it1->second.size());

    const auto app_restore_data_it1 = launch_list_it1->second.find(kWindowId1);
    EXPECT_TRUE(app_restore_data_it1 != launch_list_it1->second.end());

    VerifyAppRestoreData(
        app_restore_data_it1->second,
        apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, kDisplayId2,
        std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                    base::FilePath(kFilePath2)},
        CreateIntent(kIntentActionSend, kMimeType, kShareText1),
        kActivationIndex1, kDeskId1, kVisibleOnAllWorkspaces1, kRestoreBounds1,
        kCurrentBounds1, kWindowStateType1);

    const auto app_restore_data_it2 = launch_list_it1->second.find(kWindowId2);
    EXPECT_TRUE(app_restore_data_it2 != launch_list_it1->second.end());
    VerifyAppRestoreData(
        app_restore_data_it2->second,
        apps::mojom::LaunchContainer::kLaunchContainerTab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId1,
        std::vector<base::FilePath>{base::FilePath(kFilePath2)},
        CreateIntent(kIntentActionView, kMimeType, kShareText2),
        kActivationIndex2, kDeskId2, kVisibleOnAllWorkspaces2, kRestoreBounds2,
        kCurrentBounds2, kWindowStateType2);

    // Verify for |kAppId2|.
    const auto launch_list_it2 =
        app_id_to_launch_list(restore_data).find(kAppId2);
    EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list(restore_data).end());
    EXPECT_EQ(1u, launch_list_it2->second.size());

    EXPECT_EQ(kWindowId3, launch_list_it2->second.begin()->first);
    VerifyAppRestoreData(
        launch_list_it2->second.begin()->second,
        apps::mojom::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_POPUP, kDisplayId1,
        std::vector<base::FilePath>{base::FilePath(kFilePath1)},
        CreateIntent(kIntentActionView, kMimeType, kShareText1),
        kActivationIndex3, kDeskId3, kVisibleOnAllWorkspaces3, kRestoreBounds3,
        kCurrentBounds3, kWindowStateType3);
  }

  RestoreData& restore_data() { return restore_data_; }

  const RestoreData::AppIdToLaunchList& app_id_to_launch_list() const {
    return restore_data_.app_id_to_launch_list();
  }

  const RestoreData::AppIdToLaunchList& app_id_to_launch_list(
      const RestoreData& restore_data) const {
    return restore_data.app_id_to_launch_list();
  }

 private:
  RestoreData restore_data_;
};

TEST_F(RestoreDataTest, AddNullAppLaunchInfo) {
  restore_data().AddAppLaunchInfo(nullptr);
  EXPECT_TRUE(app_id_to_launch_list().empty());
}

TEST_F(RestoreDataTest, AddAppLaunchInfos) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  VerifyRestoreData(restore_data());
}

TEST_F(RestoreDataTest, RemoveAppRestoreData) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  VerifyRestoreData(restore_data());

  // Remove kAppId1's kWindowId1.
  restore_data().RemoveAppRestoreData(kAppId1, kWindowId1);

  EXPECT_EQ(2u, app_id_to_launch_list().size());

  // Verify for |kAppId1|.
  auto launch_list_it1 = app_id_to_launch_list().find(kAppId1);
  EXPECT_TRUE(launch_list_it1 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it1->second.size());

  EXPECT_FALSE(base::Contains(launch_list_it1->second, kWindowId1));
  EXPECT_TRUE(base::Contains(launch_list_it1->second, kWindowId2));

  // Verify for |kAppId2|.
  auto launch_list_it2 = app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it2->second.size());

  EXPECT_TRUE(base::Contains(launch_list_it2->second, kWindowId3));

  // Remove kAppId1's kWindowId2.
  restore_data().RemoveAppRestoreData(kAppId1, kWindowId2);

  EXPECT_EQ(1u, app_id_to_launch_list().size());

  // Verify for |kAppId1|.
  EXPECT_FALSE(base::Contains(app_id_to_launch_list(), kAppId1));

  // Verify for |kAppId2|.
  launch_list_it2 = app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it2->second.size());

  EXPECT_TRUE(base::Contains(launch_list_it2->second, kWindowId3));

  // Remove kAppId2's kWindowId3.
  restore_data().RemoveAppRestoreData(kAppId2, kWindowId3);

  EXPECT_EQ(0u, app_id_to_launch_list().size());
}

TEST_F(RestoreDataTest, RemoveApp) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  VerifyRestoreData(restore_data());

  // Remove kAppId1.
  restore_data().RemoveApp(kAppId1);

  EXPECT_EQ(1u, app_id_to_launch_list().size());

  // Verify for |kAppId2|
  auto launch_list_it2 = app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it2->second.size());

  EXPECT_TRUE(base::Contains(launch_list_it2->second, kWindowId3));

  // Remove kAppId2.
  restore_data().RemoveApp(kAppId2);

  EXPECT_EQ(0u, app_id_to_launch_list().size());
}

TEST_F(RestoreDataTest, Convert) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  std::unique_ptr<base::Value> value =
      std::make_unique<base::Value>(restore_data().ConvertToValue());
  std::unique_ptr<RestoreData> restore_data =
      std::make_unique<RestoreData>(std::move(value));
  VerifyRestoreData(*restore_data);
}

TEST_F(RestoreDataTest, ConvertNullData) {
  restore_data().AddAppLaunchInfo(nullptr);
  EXPECT_TRUE(app_id_to_launch_list().empty());

  std::unique_ptr<base::Value> value =
      std::make_unique<base::Value>(restore_data().ConvertToValue());
  std::unique_ptr<RestoreData> restore_data =
      std::make_unique<RestoreData>(std::move(value));
  EXPECT_TRUE(app_id_to_launch_list(*restore_data).empty());
}

TEST_F(RestoreDataTest, GetWindowInfo) {
  // The app id and window id doesn't exist;
  auto window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_FALSE(window_info);

  // Add the app launch info, but do not modify the window info.
  AddAppLaunchInfos();
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);
  EXPECT_FALSE(window_info->activation_index.has_value());
  EXPECT_FALSE(window_info->desk_id.has_value());
  EXPECT_FALSE(window_info->restore_bounds.has_value());
  EXPECT_FALSE(window_info->current_bounds.has_value());
  EXPECT_FALSE(window_info->window_state_type.has_value());

  // Modify the window info.
  ModifyWindowInfos();
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);

  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(kActivationIndex1, window_info->activation_index.value());

  EXPECT_TRUE(window_info->desk_id.has_value());
  EXPECT_EQ(kDeskId1, window_info->desk_id.value());

  EXPECT_TRUE(window_info->restore_bounds.has_value());
  EXPECT_EQ(kRestoreBounds1, window_info->restore_bounds.value());

  EXPECT_TRUE(window_info->current_bounds.has_value());
  EXPECT_EQ(kCurrentBounds1, window_info->current_bounds.value());

  EXPECT_TRUE(window_info->window_state_type.has_value());
  EXPECT_EQ(kWindowStateType1, window_info->window_state_type.value());

  EXPECT_FALSE(window_info->display_id.has_value());
}

TEST_F(RestoreDataTest, GetAppWindowInfo) {
  // Add the app launch info, but do not modify the window info.
  AddAppLaunchInfos();

  const auto it = restore_data().app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(it != restore_data().app_id_to_launch_list().end());
  EXPECT_FALSE(it->second.empty());

  auto data_it = it->second.find(kWindowId3);
  EXPECT_TRUE(data_it != it->second.end());

  auto app_window_info = data_it->second->GetAppWindowInfo();
  EXPECT_TRUE(app_window_info);
  EXPECT_EQ(-1, app_window_info->state);
  EXPECT_EQ(kDisplayId2, app_window_info->display_id);
  EXPECT_FALSE(app_window_info->bounds);

  // Modify the window info.
  ModifyWindowInfos();

  app_window_info = data_it->second->GetAppWindowInfo();
  EXPECT_EQ(static_cast<int32_t>(kWindowStateType3), app_window_info->state);
  EXPECT_EQ(kDisplayId1, app_window_info->display_id);
  EXPECT_TRUE(app_window_info->bounds);
  EXPECT_EQ(kCurrentBounds3,
            gfx::Rect(app_window_info->bounds->x, app_window_info->bounds->y,
                      app_window_info->bounds->width,
                      app_window_info->bounds->height));
}

TEST_F(RestoreDataTest, FetchRestoreWindowId) {
  // Add the app launch info, but do not modify the window info.
  AddAppLaunchInfos();

  // Modify the window info.
  ModifyWindowInfos();

  restore_data().SetNextRestoreWindowIdForChromeApp(kAppId2);

  EXPECT_EQ(kWindowId3, restore_data().FetchRestoreWindowId(kAppId2));

  // Verify that the activation index is not modified.
  auto window_info = restore_data().GetWindowInfo(kAppId2, kWindowId3);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(kActivationIndex3, window_info->activation_index.value());

  restore_data().SetNextRestoreWindowIdForChromeApp(kAppId1);

  // Verify that the activation index is modified as INT32_MIN.
  EXPECT_EQ(kWindowId1, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MIN, window_info->activation_index.value());

  // Verify that the activation index is modified as INT32_MIN.
  EXPECT_EQ(kWindowId2, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId2);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MIN, window_info->activation_index.value());

  EXPECT_EQ(0, restore_data().FetchRestoreWindowId(kAppId1));
}

}  // namespace full_restore
