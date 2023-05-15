// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/restore_data.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/values.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace app_restore {

namespace {

constexpr char kAppId1[] = "aaa";
constexpr char kAppId2[] = "bbb";

constexpr int32_t kWindowId1 = 100;
constexpr int32_t kWindowId2 = 200;
constexpr int32_t kWindowId3 = 300;
constexpr int32_t kWindowId4 = 400;

constexpr int64_t kDisplayId1 = 22000000;
constexpr int64_t kDisplayId2 = 11000000;

constexpr char kFilePath1[] = "path1";
constexpr char kFilePath2[] = "path2";

constexpr char kIntentActionView[] = "view";
constexpr char kIntentActionSend[] = "send";

constexpr bool kAppTypeBrower1 = false;
constexpr bool kAppTypeBrower2 = true;
constexpr bool kAppTypeBrower3 = false;

constexpr char kMimeType[] = "text/plain";

constexpr char kShareText1[] = "text1";
constexpr char kShareText2[] = "text2";

constexpr int32_t kActivationIndex1 = 100;
constexpr int32_t kActivationIndex2 = 101;
constexpr int32_t kActivationIndex3 = 102;

constexpr int32_t kFirstNonPinnedTabIndex = 1;

constexpr int32_t kDeskId1 = 1;
constexpr int32_t kDeskId2 = 2;
constexpr int32_t kDeskId3 =
    aura::client::kWindowWorkspaceVisibleOnAllWorkspaces;

const base::Uuid kDeskGuid1 = base::Uuid::GenerateRandomV4();
const base::Uuid kDeskGuid2 = base::Uuid::GenerateRandomV4();
const base::Uuid kDeskGuid3 = base::Uuid();

constexpr gfx::Rect kCurrentBounds1(11, 21, 111, 121);
constexpr gfx::Rect kCurrentBounds2(31, 41, 131, 141);
constexpr gfx::Rect kCurrentBounds3(51, 61, 151, 161);

constexpr chromeos::WindowStateType kWindowStateType1 =
    chromeos::WindowStateType::kMaximized;
constexpr chromeos::WindowStateType kWindowStateType2 =
    chromeos::WindowStateType::kMinimized;
constexpr chromeos::WindowStateType kWindowStateType3 =
    chromeos::WindowStateType::kPrimarySnapped;

constexpr ui::WindowShowState kPreMinimizedWindowStateType1 =
    ui::SHOW_STATE_DEFAULT;
constexpr ui::WindowShowState kPreMinimizedWindowStateType2 =
    ui::SHOW_STATE_MAXIMIZED;
constexpr ui::WindowShowState kPreMinimizedWindowStateType3 =
    ui::SHOW_STATE_DEFAULT;

constexpr int32_t kSnapPercentage = 75;

constexpr gfx::Size kMaxSize1(600, 800);
constexpr gfx::Size kMinSize1(100, 50);
constexpr gfx::Size kMinSize2(88, 128);

constexpr uint32_t kPrimaryColor1(0xFFFFFFFF);
constexpr uint32_t kPrimaryColor2(0xFF000000);

constexpr uint32_t kStatusBarColor1(0xFF00FF00);
constexpr uint32_t kStatusBarColor2(0xFF000000);

constexpr char16_t kTitle1[] = u"test title1";
constexpr char16_t kTitle2[] = u"test title2";

constexpr gfx::Rect kBoundsInRoot1(11, 21, 111, 121);
constexpr gfx::Rect kBoundsInRoot2(31, 41, 131, 141);

constexpr char16_t kTestTabGroupTitleOne[] = u"sample_tab_group_1";
constexpr char16_t kTestTabGroupTitleTwo[] = u"sample_tab_group_2";
constexpr char16_t kTestTabGroupTitleThree[] = u"sample_tab_group_3";
const tab_groups::TabGroupColorId kTestTabGroupColorOne =
    tab_groups::TabGroupColorId::kGrey;
const tab_groups::TabGroupColorId kTestTabGroupColorTwo =
    tab_groups::TabGroupColorId::kBlue;
const tab_groups::TabGroupColorId kTestTabGroupColorThree =
    tab_groups::TabGroupColorId::kGreen;
const gfx::Range kTestTabGroupTabRange(1, 2);

tab_groups::TabGroupInfo MakeTestTabGroup(const char16_t* title,
                                          tab_groups::TabGroupColorId color) {
  return tab_groups::TabGroupInfo(kTestTabGroupTabRange,
                                  tab_groups::TabGroupVisualData(title, color));
}

void PopulateTestTabgroups(
    std::vector<tab_groups::TabGroupInfo>& out_tab_groups) {
  out_tab_groups.push_back(
      MakeTestTabGroup(kTestTabGroupTitleOne, kTestTabGroupColorOne));
  out_tab_groups.push_back(
      MakeTestTabGroup(kTestTabGroupTitleTwo, kTestTabGroupColorTwo));
  out_tab_groups.push_back(
      MakeTestTabGroup(kTestTabGroupTitleThree, kTestTabGroupColorThree));
}

}  // namespace

// Unit tests for restore data.
class RestoreDataTest : public testing::Test {
 public:
  RestoreDataTest() = default;
  RestoreDataTest(const RestoreDataTest&) = delete;
  RestoreDataTest& operator=(const RestoreDataTest&) = delete;
  ~RestoreDataTest() override = default;

  apps::IntentPtr MakeIntent(const std::string& action,
                             const std::string& mime_type,
                             const std::string& share_text) {
    auto intent = std::make_unique<apps::Intent>(action);
    intent->mime_type = mime_type;
    intent->share_text = share_text;
    return intent;
  }

  void AddAppLaunchInfos() {
    std::unique_ptr<AppLaunchInfo> app_launch_info1 =
        std::make_unique<AppLaunchInfo>(
            kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
            std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                        base::FilePath(kFilePath2)},
            MakeIntent(kIntentActionSend, kMimeType, kShareText1));

    std::unique_ptr<AppLaunchInfo> app_launch_info2 =
        std::make_unique<AppLaunchInfo>(
            kAppId1, kWindowId2, apps::LaunchContainer::kLaunchContainerTab,
            WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
            std::vector<base::FilePath>{base::FilePath(kFilePath2)},
            MakeIntent(kIntentActionView, kMimeType, kShareText2));
    app_launch_info2->app_type_browser = kAppTypeBrower2;
    app_launch_info2->first_non_pinned_tab_index = kFirstNonPinnedTabIndex;
    PopulateTestTabgroups(app_launch_info2->tab_group_infos);

    std::unique_ptr<AppLaunchInfo> app_launch_info3 =
        std::make_unique<AppLaunchInfo>(
            kAppId2, kWindowId3, apps::LaunchContainer::kLaunchContainerNone,
            WindowOpenDisposition::NEW_POPUP, kDisplayId2,
            std::vector<base::FilePath>{base::FilePath(kFilePath1)},
            MakeIntent(kIntentActionView, kMimeType, kShareText1));

    restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
    restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
    restore_data().AddAppLaunchInfo(std::move(app_launch_info3));
  }

  void ModifyWindowInfos() {
    WindowInfo window_info1;
    window_info1.activation_index = kActivationIndex1;
    window_info1.desk_id = kDeskId1;
    window_info1.desk_guid = kDeskGuid1;
    window_info1.current_bounds = kCurrentBounds1;
    window_info1.window_state_type = kWindowStateType1;
    window_info1.display_id = kDisplayId2;
    window_info1.app_title = kTitle1;
    window_info1.arc_extra_info = WindowInfo::ArcExtraInfo();
    window_info1.arc_extra_info->maximum_size = kMaxSize1;
    window_info1.arc_extra_info->minimum_size = kMinSize1;
    window_info1.arc_extra_info->bounds_in_root = kBoundsInRoot1;

    WindowInfo window_info2;
    window_info2.activation_index = kActivationIndex2;
    window_info2.desk_id = kDeskId2;
    window_info2.desk_guid = kDeskGuid2;
    window_info2.current_bounds = kCurrentBounds2;
    window_info2.window_state_type = kWindowStateType2;
    window_info2.pre_minimized_show_state_type = kPreMinimizedWindowStateType2;
    window_info2.display_id = kDisplayId1;
    window_info2.app_title = kTitle2;
    window_info2.arc_extra_info = WindowInfo::ArcExtraInfo();
    window_info2.arc_extra_info->minimum_size = kMinSize2;
    window_info2.arc_extra_info->bounds_in_root = kBoundsInRoot2;

    WindowInfo window_info3;
    window_info3.activation_index = kActivationIndex3;
    window_info3.desk_id = kDeskId3;
    window_info3.desk_guid = kDeskGuid3;
    window_info3.current_bounds = kCurrentBounds3;
    window_info3.window_state_type = kWindowStateType3;
    window_info3.snap_percentage = kSnapPercentage;
    window_info3.display_id = kDisplayId1;

    restore_data().ModifyWindowInfo(kAppId1, kWindowId1, window_info1);
    restore_data().ModifyWindowInfo(kAppId1, kWindowId2, window_info2);
    restore_data().ModifyWindowInfo(kAppId2, kWindowId3, window_info3);
  }

  void ModifyThemeColors() {
    restore_data().ModifyThemeColor(kAppId1, kWindowId1, kPrimaryColor1,
                                    kStatusBarColor1);
    restore_data().ModifyThemeColor(kAppId1, kWindowId2, kPrimaryColor2,
                                    kStatusBarColor2);
  }

  void VerifyAppRestoreData(
      const std::unique_ptr<AppRestoreData>& data,
      apps::LaunchContainer container,
      WindowOpenDisposition disposition,
      int64_t display_id,
      std::vector<base::FilePath> file_paths,
      apps::IntentPtr intent,
      bool app_type_browser,
      int32_t activation_index,
      int32_t first_non_pinned_tab_index,
      int32_t desk_id,
      const base::Uuid& desk_guid,
      const gfx::Rect& current_bounds,
      chromeos::WindowStateType window_state_type,
      ui::WindowShowState pre_minimized_show_state_type,
      uint32_t snap_percentage,
      absl::optional<gfx::Size> max_size,
      absl::optional<gfx::Size> min_size,
      absl::optional<std::u16string> title,
      absl::optional<gfx::Rect> bounds_in_root,
      uint32_t primary_color,
      uint32_t status_bar_color,
      std::vector<tab_groups::TabGroupInfo> expected_tab_group_infos,
      bool test_tab_group_infos = true) {
    EXPECT_TRUE(data->container.has_value());
    EXPECT_EQ(static_cast<int>(container), data->container.value());

    EXPECT_TRUE(data->disposition.has_value());
    EXPECT_EQ(static_cast<int>(disposition), data->disposition.value());

    EXPECT_TRUE(data->display_id.has_value());
    EXPECT_EQ(display_id, data->display_id.value());

    EXPECT_FALSE(data->file_paths.empty());
    EXPECT_EQ(file_paths.size(), data->file_paths.size());
    for (size_t i = 0; i < file_paths.size(); i++)
      EXPECT_EQ(file_paths[i], data->file_paths[i]);

    EXPECT_TRUE(data->intent);
    EXPECT_EQ(intent->action, data->intent->action);
    EXPECT_EQ(intent->mime_type, data->intent->mime_type);
    EXPECT_EQ(intent->share_text, data->intent->share_text);

    if (!app_type_browser)
      // This field should only be written if it is true.
      EXPECT_FALSE(data->app_type_browser.has_value());
    else {
      EXPECT_TRUE(data->app_type_browser.has_value());
      EXPECT_EQ(app_type_browser, data->app_type_browser.value());
      EXPECT_TRUE(data->first_non_pinned_tab_index.has_value());
      EXPECT_EQ(data->first_non_pinned_tab_index, first_non_pinned_tab_index);
    }

    EXPECT_TRUE(data->activation_index.has_value());
    EXPECT_EQ(activation_index, data->activation_index.value());

    EXPECT_TRUE(data->desk_id.has_value());
    EXPECT_EQ(desk_id, data->desk_id.value());

    EXPECT_EQ(desk_guid, data->desk_guid);

    EXPECT_TRUE(data->current_bounds.has_value());
    EXPECT_EQ(current_bounds, data->current_bounds.value());

    ASSERT_TRUE(data->window_state_type.has_value());
    EXPECT_EQ(window_state_type, data->window_state_type.value());

    // This field should only be written if we are in minimized window state.
    if (data->window_state_type.value() ==
        chromeos::WindowStateType::kMinimized) {
      EXPECT_TRUE(data->pre_minimized_show_state_type.has_value());
      EXPECT_EQ(pre_minimized_show_state_type,
                data->pre_minimized_show_state_type.value());
    }

    // This field should only be written if we are snapped.
    if (data->window_state_type.value() ==
            chromeos::WindowStateType::kPrimarySnapped ||
        data->window_state_type.value() ==
            chromeos::WindowStateType::kSecondarySnapped) {
      EXPECT_TRUE(data->snap_percentage.has_value());
      EXPECT_EQ(snap_percentage, data->snap_percentage.value());
    }

    if (max_size.has_value()) {
      EXPECT_TRUE(data->maximum_size.has_value());
      EXPECT_EQ(max_size.value(), data->maximum_size.value());
    } else {
      EXPECT_FALSE(data->maximum_size.has_value());
    }

    if (min_size.has_value()) {
      EXPECT_TRUE(data->minimum_size.has_value());
      EXPECT_EQ(min_size.value(), data->minimum_size.value());
    } else {
      EXPECT_FALSE(data->minimum_size.has_value());
    }

    if (title.has_value()) {
      EXPECT_TRUE(data->title.has_value());
      EXPECT_EQ(title.value(), data->title.value());
    } else {
      EXPECT_FALSE(data->title.has_value());
    }

    if (bounds_in_root.has_value()) {
      EXPECT_TRUE(data->bounds_in_root.has_value());
      EXPECT_EQ(bounds_in_root.value(), data->bounds_in_root.value());
    } else {
      EXPECT_FALSE(data->bounds_in_root.has_value());
    }

    if (primary_color) {
      EXPECT_TRUE(data->primary_color.has_value());
      EXPECT_EQ(primary_color, data->primary_color.value());
    } else {
      EXPECT_FALSE(data->primary_color.has_value());
    }

    if (status_bar_color) {
      EXPECT_TRUE(data->status_bar_color.has_value());
      EXPECT_EQ(status_bar_color, data->status_bar_color.value());
    } else {
      EXPECT_FALSE(data->status_bar_color.has_value());
    }

    // Only test tab group infos in tests that don't concern serialization
    // or deserialization as the logic for serializing tab group infos
    // exists in the desks_storage component.  This is because tab group
    // infos are only utilized by save and recall and desk template features.
    if (expected_tab_group_infos.size() > 0 && test_tab_group_infos) {
      // If we're passing a non-empty expceted vector then we expect the
      // object under test to have tab group infos.
      EXPECT_FALSE(data->tab_group_infos.empty());

      // Parameter vector and data vector should always have the same size
      // as they should be instantiated from the same function.
      EXPECT_EQ(expected_tab_group_infos.size(), data->tab_group_infos.size());

      EXPECT_THAT(expected_tab_group_infos,
                  testing::UnorderedElementsAreArray(data->tab_group_infos));
    }
  }

  void VerifyRestoreData(const RestoreData& restore_data,
                         bool test_tab_group_infos = true) {
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
        apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, kDisplayId2,
        std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                    base::FilePath(kFilePath2)},
        MakeIntent(kIntentActionSend, kMimeType, kShareText1), kAppTypeBrower1,
        kActivationIndex1, kFirstNonPinnedTabIndex, kDeskId1, kDeskGuid1,
        kCurrentBounds1, kWindowStateType1, kPreMinimizedWindowStateType1,
        /*snap_percentage=*/0, kMaxSize1, kMinSize1, std::u16string(kTitle1),
        kBoundsInRoot1, kPrimaryColor1, kStatusBarColor1,
        /*tab_group_infos=*/{});

    const auto app_restore_data_it2 = launch_list_it1->second.find(kWindowId2);
    std::vector<tab_groups::TabGroupInfo> expected_tab_group_infos;
    PopulateTestTabgroups(expected_tab_group_infos);
    EXPECT_TRUE(app_restore_data_it2 != launch_list_it1->second.end());
    VerifyAppRestoreData(
        app_restore_data_it2->second,
        apps::LaunchContainer::kLaunchContainerTab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId1,
        std::vector<base::FilePath>{base::FilePath(kFilePath2)},
        MakeIntent(kIntentActionView, kMimeType, kShareText2), kAppTypeBrower2,
        kActivationIndex2, kFirstNonPinnedTabIndex, kDeskId2, kDeskGuid2,
        kCurrentBounds2, kWindowStateType2, kPreMinimizedWindowStateType2,
        /*snap_percentage=*/0, absl::nullopt, kMinSize2,
        std::u16string(kTitle2), kBoundsInRoot2, kPrimaryColor2,
        kStatusBarColor2, std::move(expected_tab_group_infos),
        test_tab_group_infos);

    // Verify for |kAppId2|.
    const auto launch_list_it2 =
        app_id_to_launch_list(restore_data).find(kAppId2);
    EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list(restore_data).end());
    EXPECT_EQ(1u, launch_list_it2->second.size());

    EXPECT_EQ(kWindowId3, launch_list_it2->second.begin()->first);
    VerifyAppRestoreData(
        launch_list_it2->second.begin()->second,
        apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_POPUP, kDisplayId1,
        std::vector<base::FilePath>{base::FilePath(kFilePath1)},
        MakeIntent(kIntentActionView, kMimeType, kShareText1), kAppTypeBrower3,
        kActivationIndex3, kFirstNonPinnedTabIndex, kDeskId3, kDeskGuid3,
        kCurrentBounds3, kWindowStateType3, kPreMinimizedWindowStateType3,
        kSnapPercentage, absl::nullopt, absl::nullopt, absl::nullopt,
        absl::nullopt, 0, 0,
        /*expected_tab_group_infos=*/{});
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
  ModifyThemeColors();
  VerifyRestoreData(restore_data());
}

// Modify the window id from `kWindowId2` to `kWindowId4` for `kAppId1`. Verify
// the restore data is correctly updated.
TEST_F(RestoreDataTest, ModifyWindowId) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  ModifyThemeColors();
  VerifyRestoreData(restore_data());

  restore_data().ModifyWindowId(kAppId1, kWindowId2, kWindowId4);

  // Verify for |kAppId1|.
  const auto launch_list_it1 =
      app_id_to_launch_list(restore_data()).find(kAppId1);
  EXPECT_TRUE(launch_list_it1 != app_id_to_launch_list(restore_data()).end());
  EXPECT_EQ(2u, launch_list_it1->second.size());

  // Verify the restore data for |kAppId1| and |kWindowId1| still exists.
  EXPECT_TRUE(base::Contains(launch_list_it1->second, kWindowId1));

  // Verify the restore data for |kAppId1| and |kWindowId2| doesn't exist.
  EXPECT_TRUE(!base::Contains(launch_list_it1->second, kWindowId2));

  // Verify the restore data for |kWindowId2| is migrated to |kWindowId4|.
  const auto app_restore_data_it4 = launch_list_it1->second.find(kWindowId4);
  EXPECT_TRUE(app_restore_data_it4 != launch_list_it1->second.end());
  VerifyAppRestoreData(
      app_restore_data_it4->second, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId1,
      std::vector<base::FilePath>{base::FilePath(kFilePath2)},
      MakeIntent(kIntentActionView, kMimeType, kShareText2), kAppTypeBrower2,
      kActivationIndex2, kFirstNonPinnedTabIndex, kDeskId2, kDeskGuid2,
      kCurrentBounds2, kWindowStateType2, kPreMinimizedWindowStateType2,
      /*snap_percentage=*/0, absl::nullopt, kMinSize2, std::u16string(kTitle2),
      kBoundsInRoot2, kPrimaryColor2, kStatusBarColor2, /*tab_group_infos=*/{});

  // Verify the restore data for |kAppId2| still exists.
  const auto launch_list_it2 =
      app_id_to_launch_list(restore_data()).find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list(restore_data()).end());
  EXPECT_EQ(1u, launch_list_it2->second.size());
}

TEST_F(RestoreDataTest, RemoveAppRestoreData) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  ModifyThemeColors();
  VerifyRestoreData(restore_data());

  EXPECT_TRUE(restore_data().HasAppRestoreData(kAppId1, kWindowId1));

  // Remove kAppId1's kWindowId1.
  restore_data().RemoveAppRestoreData(kAppId1, kWindowId1);

  EXPECT_FALSE(restore_data().HasAppRestoreData(kAppId1, kWindowId1));

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

  EXPECT_TRUE(restore_data().HasAppRestoreData(kAppId1, kWindowId2));

  // Remove kAppId1's kWindowId2.
  restore_data().RemoveAppRestoreData(kAppId1, kWindowId2);

  EXPECT_FALSE(restore_data().HasAppRestoreData(kAppId1, kWindowId2));

  EXPECT_EQ(1u, app_id_to_launch_list().size());

  // Verify for |kAppId1|.
  EXPECT_FALSE(base::Contains(app_id_to_launch_list(), kAppId1));

  // Verify for |kAppId2|.
  launch_list_it2 = app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it2->second.size());

  EXPECT_TRUE(base::Contains(launch_list_it2->second, kWindowId3));

  EXPECT_TRUE(restore_data().HasAppRestoreData(kAppId2, kWindowId3));

  // Remove kAppId2's kWindowId3.
  restore_data().RemoveAppRestoreData(kAppId2, kWindowId3);

  EXPECT_FALSE(restore_data().HasAppRestoreData(kAppId2, kWindowId3));

  EXPECT_EQ(0u, app_id_to_launch_list().size());
}

TEST_F(RestoreDataTest, SendWindowToBackground) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  ModifyThemeColors();
  VerifyRestoreData(restore_data());

  restore_data().SendWindowToBackground(kAppId1, kWindowId1);

  auto window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MAX, window_info->activation_index.value());
  EXPECT_TRUE(window_info->desk_id.has_value());
  EXPECT_TRUE(window_info->desk_guid.is_valid());
  EXPECT_TRUE(window_info->current_bounds.has_value());
  EXPECT_TRUE(window_info->window_state_type.has_value());
  EXPECT_TRUE(window_info->arc_extra_info.has_value());
}

TEST_F(RestoreDataTest, RemoveApp) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  ModifyThemeColors();
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
  ModifyThemeColors();
  std::unique_ptr<RestoreData> restore_data =
      std::make_unique<RestoreData>(this->restore_data().ConvertToValue());
  // Full restore is not responsible for serializing or deseraizling
  // TabGroupInfos.
  VerifyRestoreData(*restore_data, /*test_tab_group_infos=*/false);
}

TEST_F(RestoreDataTest, ConvertNullData) {
  restore_data().AddAppLaunchInfo(nullptr);
  EXPECT_TRUE(app_id_to_launch_list().empty());

  std::unique_ptr<RestoreData> restore_data =
      std::make_unique<RestoreData>(this->restore_data().ConvertToValue());
  EXPECT_TRUE(app_id_to_launch_list(*restore_data).empty());
}

TEST_F(RestoreDataTest, GetAppLaunchInfo) {
  // The app id and window id doesn't exist.
  auto app_launch_info = restore_data().GetAppLaunchInfo(kAppId1, kWindowId1);
  EXPECT_FALSE(app_launch_info);

  // Add the app launch info.
  AddAppLaunchInfos();
  app_launch_info = restore_data().GetAppLaunchInfo(kAppId1, kWindowId1);

  // Verify the app launch info.
  EXPECT_TRUE(app_launch_info);

  EXPECT_EQ(kAppId1, app_launch_info->app_id);

  EXPECT_TRUE(app_launch_info->window_id.has_value());
  EXPECT_EQ(kWindowId1, app_launch_info->window_id.value());

  EXPECT_FALSE(app_launch_info->event_flag.has_value());

  EXPECT_TRUE(app_launch_info->container.has_value());
  EXPECT_EQ(static_cast<int>(apps::LaunchContainer::kLaunchContainerWindow),
            app_launch_info->container.value());

  EXPECT_TRUE(app_launch_info->disposition.has_value());
  EXPECT_EQ(static_cast<int>(WindowOpenDisposition::NEW_WINDOW),
            app_launch_info->disposition.value());

  EXPECT_FALSE(app_launch_info->arc_session_id.has_value());

  EXPECT_TRUE(app_launch_info->display_id.has_value());
  EXPECT_EQ(kDisplayId1, app_launch_info->display_id.value());

  EXPECT_FALSE(app_launch_info->file_paths.empty());
  ASSERT_EQ(2u, app_launch_info->file_paths.size());
  EXPECT_EQ(base::FilePath(kFilePath1), app_launch_info->file_paths[0]);
  EXPECT_EQ(base::FilePath(kFilePath2), app_launch_info->file_paths[1]);

  EXPECT_TRUE(app_launch_info->intent);
  EXPECT_EQ(kIntentActionSend, app_launch_info->intent->action);
  EXPECT_EQ(kMimeType, app_launch_info->intent->mime_type);
  EXPECT_EQ(kShareText1, app_launch_info->intent->share_text);

  EXPECT_FALSE(app_launch_info->app_type_browser.has_value());
}

TEST_F(RestoreDataTest, GetWindowInfo) {
  // The app id and window id doesn't exist.
  auto window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_FALSE(window_info);

  // Add the app launch info, but do not modify the window info.
  AddAppLaunchInfos();
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);
  EXPECT_FALSE(window_info->activation_index.has_value());
  EXPECT_FALSE(window_info->desk_id.has_value());
  EXPECT_FALSE(window_info->desk_guid.is_valid());
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

  EXPECT_TRUE(window_info->desk_guid.is_valid());
  EXPECT_EQ(kDeskGuid1, window_info->desk_guid);

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
  EXPECT_EQ(0, app_window_info->state);
  EXPECT_EQ(kDisplayId2, app_window_info->display_id);
  EXPECT_FALSE(app_window_info->bounds);

  // Modify the window info.
  ModifyWindowInfos();

  app_window_info = data_it->second->GetAppWindowInfo();
  EXPECT_EQ(static_cast<int32_t>(kWindowStateType3), app_window_info->state);
  EXPECT_EQ(kDisplayId1, app_window_info->display_id);
  EXPECT_TRUE(app_window_info->bounds);
  EXPECT_EQ(kCurrentBounds3, gfx::Rect(app_window_info->bounds->x(),
                                       app_window_info->bounds->y(),
                                       app_window_info->bounds->width(),
                                       app_window_info->bounds->height()));
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

  // Verify that the activation index is modified as INT32_MAX.
  EXPECT_EQ(kWindowId1, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MAX, window_info->activation_index.value());

  // Verify that the activation index is modified as INT32_MAX.
  EXPECT_EQ(kWindowId2, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId2);
  EXPECT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MAX, window_info->activation_index.value());

  EXPECT_EQ(0, restore_data().FetchRestoreWindowId(kAppId1));
}

TEST_F(RestoreDataTest, HasAppTypeBrowser) {
  std::unique_ptr<AppLaunchInfo> app_launch_info1 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId1);
  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  EXPECT_FALSE(restore_data().HasAppTypeBrowser());

  std::unique_ptr<AppLaunchInfo> app_launch_info2 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId2);
  app_launch_info2->app_type_browser = true;
  restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
  EXPECT_TRUE(restore_data().HasAppTypeBrowser());
}

TEST_F(RestoreDataTest, HasBrowser) {
  std::unique_ptr<AppLaunchInfo> app_launch_info1 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId1);
  app_launch_info1->app_type_browser = true;
  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  EXPECT_FALSE(restore_data().HasBrowser());

  std::unique_ptr<AppLaunchInfo> app_launch_info2 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId2);
  restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
  EXPECT_TRUE(restore_data().HasBrowser());
}

TEST_F(RestoreDataTest, UpdateAppIdToLacros) {
  std::unique_ptr<AppLaunchInfo> app_launch_info1 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId1);

  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  // Verify that ash chrome is added.
  const auto ash_chrome_it =
      restore_data().app_id_to_launch_list().find(app_constants::kChromeAppId);
  EXPECT_TRUE(ash_chrome_it != restore_data().app_id_to_launch_list().end());
  EXPECT_FALSE(ash_chrome_it->second.empty());

  restore_data().UpdateBrowserAppIdToLacros();
  // Verify that ash chrome app id is modified to lacros version.
  const auto lacros_chrome_it =
      restore_data().app_id_to_launch_list().find(app_constants::kLacrosAppId);
  const auto ash_chrome_after_update_it =
      restore_data().app_id_to_launch_list().find(app_constants::kChromeAppId);
  EXPECT_TRUE(lacros_chrome_it != restore_data().app_id_to_launch_list().end());
  EXPECT_FALSE(lacros_chrome_it->second.empty());
  EXPECT_TRUE(ash_chrome_after_update_it ==
              restore_data().app_id_to_launch_list().end());
  EXPECT_EQ(1u, restore_data().app_id_to_launch_list().size());
}

TEST_F(RestoreDataTest, CompareAppRestoreData) {
  std::unique_ptr<AppLaunchInfo> app_launch_info_1 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          MakeIntent(kIntentActionSend, kMimeType, kShareText1));

  app_launch_info_1->app_type_browser = kAppTypeBrower2;
  app_launch_info_1->first_non_pinned_tab_index = kFirstNonPinnedTabIndex;
  PopulateTestTabgroups(app_launch_info_1->tab_group_infos);

  // Same as app_launch_info_1.
  std::unique_ptr<AppLaunchInfo> app_launch_info_2 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          MakeIntent(kIntentActionSend, kMimeType, kShareText1));

  app_launch_info_2->app_type_browser = kAppTypeBrower2;
  app_launch_info_2->first_non_pinned_tab_index = kFirstNonPinnedTabIndex;
  PopulateTestTabgroups(app_launch_info_2->tab_group_infos);

  std::unique_ptr<AppLaunchInfo> app_launch_info_3 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kWindowId2, apps::LaunchContainer::kLaunchContainerTab,
          WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
          std::vector<base::FilePath>{base::FilePath(kFilePath2)},
          MakeIntent(kIntentActionView, kMimeType, kShareText2));

  std::unique_ptr<AppRestoreData> app_restore_data_1 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_1));

  std::unique_ptr<AppRestoreData> app_restore_data_2 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_2));

  std::unique_ptr<AppRestoreData> app_restore_data_3 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_3));

  EXPECT_TRUE(*app_restore_data_1 == *app_restore_data_2);
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_3);

  // Modify tab groups of app_restore_data_2.
  app_restore_data_2->tab_group_infos.push_back(
      MakeTestTabGroup(kTestTabGroupTitleThree, kTestTabGroupColorThree));
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_2);
}

TEST_F(RestoreDataTest, CompareAppRestoreDataIntent) {
  // Intent is nullptr.
  std::unique_ptr<AppLaunchInfo> app_launch_info_1 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          nullptr);

  // Same as app_launch_info_1.
  std::unique_ptr<AppLaunchInfo> app_launch_info_2 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          nullptr);

  std::unique_ptr<AppRestoreData> app_restore_data_1 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_1));

  std::unique_ptr<AppRestoreData> app_restore_data_2 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_2));

  // Intent both nullptr.
  EXPECT_TRUE(*app_restore_data_1 == *app_restore_data_2);

  // Add intent to app_restore_data_1.
  app_restore_data_1->intent =
      MakeIntent(kIntentActionView, kMimeType, kShareText1);
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_2);

  // Add intent to app_restore_data_2, different from app_restore_data_1.
  app_restore_data_2->intent =
      MakeIntent(kIntentActionView, kMimeType, kShareText2);
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_2);

  // Modify app_restore_data_2 to the same as app_restore_data_1.
  app_restore_data_2->intent =
      MakeIntent(kIntentActionView, kMimeType, kShareText1);
  EXPECT_TRUE(*app_restore_data_1 == *app_restore_data_2);
}

}  // namespace app_restore
