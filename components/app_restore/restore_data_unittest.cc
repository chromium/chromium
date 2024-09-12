// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/restore_data.h"

#include <cstdint>
#include <memory>
#include <optional>
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
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

namespace app_restore {

namespace {

using testing::ElementsAre;
using testing::Pair;

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

constexpr ui::mojom::WindowShowState kPreMinimizedWindowStateType1 =
    ui::mojom::WindowShowState::kDefault;
constexpr ui::mojom::WindowShowState kPreMinimizedWindowStateType2 =
    ui::mojom::WindowShowState::kMaximized;
constexpr ui::mojom::WindowShowState kPreMinimizedWindowStateType3 =
    ui::mojom::WindowShowState::kDefault;

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
    auto app_launch_info1 = std::make_unique<AppLaunchInfo>(
        kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
        std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                    base::FilePath(kFilePath2)},
        MakeIntent(kIntentActionSend, kMimeType, kShareText1));

    auto app_launch_info2 = std::make_unique<AppLaunchInfo>(
        kAppId1, kWindowId2, apps::LaunchContainer::kLaunchContainerTab,
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
        std::vector<base::FilePath>{base::FilePath(kFilePath2)},
        MakeIntent(kIntentActionView, kMimeType, kShareText2));
    app_launch_info2->browser_extra_info.app_type_browser = kAppTypeBrower2;
    app_launch_info2->browser_extra_info.first_non_pinned_tab_index =
        kFirstNonPinnedTabIndex;
    PopulateTestTabgroups(app_launch_info2->browser_extra_info.tab_group_infos);

    auto app_launch_info3 = std::make_unique<AppLaunchInfo>(
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
    window_info1.arc_extra_info = {.maximum_size = kMaxSize1,
                                   .minimum_size = kMinSize1,
                                   .bounds_in_root = kBoundsInRoot1};

    WindowInfo window_info2;
    window_info2.activation_index = kActivationIndex2;
    window_info2.desk_id = kDeskId2;
    window_info2.desk_guid = kDeskGuid2;
    window_info2.current_bounds = kCurrentBounds2;
    window_info2.window_state_type = kWindowStateType2;
    window_info2.pre_minimized_show_state_type = kPreMinimizedWindowStateType2;
    window_info2.display_id = kDisplayId1;
    window_info2.app_title = kTitle2;
    window_info2.arc_extra_info = {.minimum_size = kMinSize2,
                                   .bounds_in_root = kBoundsInRoot2};

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
      ui::mojom::WindowShowState pre_minimized_show_state_type,
      uint32_t snap_percentage,
      std::optional<gfx::Size> max_size,
      std::optional<gfx::Size> min_size,
      std::optional<std::u16string> title,
      std::optional<gfx::Rect> bounds_in_root,
      uint32_t primary_color,
      uint32_t status_bar_color,
      std::vector<tab_groups::TabGroupInfo> expected_tab_group_infos,
      bool test_tab_group_infos = true) {
    EXPECT_THAT(data->container,
                testing::Optional(static_cast<int>(container)));
    EXPECT_THAT(data->disposition,
                testing::Optional(static_cast<int>(disposition)));
    EXPECT_THAT(data->display_id, testing::Optional(display_id));

    EXPECT_EQ(file_paths, data->file_paths);

    EXPECT_TRUE(data->intent);
    EXPECT_EQ(intent->action, data->intent->action);
    EXPECT_EQ(intent->mime_type, data->intent->mime_type);
    EXPECT_EQ(intent->share_text, data->intent->share_text);

    const BrowserExtraInfo browser_info = data->browser_extra_info;
    if (!app_type_browser) {
      // This field should only be written if it is true.
      EXPECT_FALSE(browser_info.app_type_browser.has_value());
    } else {
      EXPECT_THAT(browser_info.app_type_browser,
                  testing::Optional(app_type_browser));
      EXPECT_THAT(browser_info.first_non_pinned_tab_index,
                  testing::Optional(first_non_pinned_tab_index));
    }

    const WindowInfo window_info = data->window_info;
    EXPECT_THAT(window_info.activation_index,
                testing::Optional(activation_index));
    EXPECT_THAT(window_info.desk_id, testing::Optional(desk_id));
    EXPECT_EQ(desk_guid, window_info.desk_guid);
    EXPECT_THAT(window_info.current_bounds, testing::Optional(current_bounds));
    EXPECT_THAT(window_info.window_state_type,
                testing::Optional(window_state_type));

    // This field should only be written if we are in minimized window state.
    if (window_info.window_state_type.value() ==
        chromeos::WindowStateType::kMinimized) {
      EXPECT_THAT(window_info.pre_minimized_show_state_type,
                  testing::Optional(pre_minimized_show_state_type));
    }

    // This field should only be written if we are snapped.
    if (chromeos::IsSnappedWindowStateType(
            window_info.window_state_type.value())) {
      EXPECT_THAT(window_info.snap_percentage,
                  testing::Optional(snap_percentage));
    }

    EXPECT_EQ(title, window_info.app_title);

    // Extra ARC window's information.
    if (max_size || min_size || bounds_in_root) {
      ASSERT_TRUE(window_info.arc_extra_info.has_value());
      EXPECT_EQ(max_size, window_info.arc_extra_info->maximum_size);
      EXPECT_EQ(min_size, window_info.arc_extra_info->minimum_size);
      EXPECT_EQ(bounds_in_root, window_info.arc_extra_info->bounds_in_root);
    }

    if (primary_color) {
      EXPECT_THAT(data->primary_color, testing::Optional(primary_color));
    } else {
      EXPECT_FALSE(data->primary_color.has_value());
    }

    if (status_bar_color) {
      EXPECT_THAT(data->status_bar_color, testing::Optional(status_bar_color));
    } else {
      EXPECT_FALSE(data->status_bar_color.has_value());
    }

    // Only test tab group infos in tests that don't concern serialization
    // or deserialization as the logic for serializing tab group infos exists in
    // the desks_storage component. This is because tab group infos are only
    // utilized by save and recall and desk template features.
    if (expected_tab_group_infos.size() > 0 && test_tab_group_infos) {
      // If we're passing a non-empty expected vector then we expect the object
      // under test to have tab group infos.
      EXPECT_FALSE(browser_info.tab_group_infos.empty());
      EXPECT_THAT(browser_info.tab_group_infos,
                  testing::UnorderedElementsAreArray(expected_tab_group_infos));
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
        /*expected_tab_group_infos=*/{});

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
        /*snap_percentage=*/0, std::nullopt, kMinSize2, std::u16string(kTitle2),
        kBoundsInRoot2, kPrimaryColor2, kStatusBarColor2,
        std::move(expected_tab_group_infos), test_tab_group_infos);

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
        kSnapPercentage, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        0, 0,
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
  EXPECT_FALSE(base::Contains(launch_list_it1->second, kWindowId2));

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
      /*snap_percentage=*/0, std::nullopt, kMinSize2, std::u16string(kTitle2),
      kBoundsInRoot2, kPrimaryColor2, kStatusBarColor2,
      /*expected_tab_group_infos=*/{});

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

  // Remove `kAppId1`'s `kWindowId1`.
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
  ASSERT_TRUE(window_info);
  EXPECT_THAT(window_info->activation_index, testing::Optional(INT32_MAX));
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

  // Remove `kAppId1`.
  restore_data().RemoveApp(kAppId1);

  // Verify for `kAppId2`.
  EXPECT_THAT(
      app_id_to_launch_list(),
      ElementsAre(Pair(kAppId2, ElementsAre(Pair(kWindowId3, testing::_)))));

  // Remove kAppId2.
  restore_data().RemoveApp(kAppId2);
  EXPECT_TRUE(app_id_to_launch_list().empty());
}

TEST_F(RestoreDataTest, Convert) {
  AddAppLaunchInfos();
  ModifyWindowInfos();
  ModifyThemeColors();
  auto restore_data =
      std::make_unique<RestoreData>(this->restore_data().ConvertToValue());
  // Full restore is not responsible for serializing or deserializing
  // TabGroupInfos.
  VerifyRestoreData(*restore_data, /*test_tab_group_infos=*/false);
}

TEST_F(RestoreDataTest, ConvertNullData) {
  restore_data().AddAppLaunchInfo(nullptr);
  EXPECT_TRUE(app_id_to_launch_list().empty());

  auto restore_data =
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

  EXPECT_THAT(app_launch_info->window_id, testing::Optional(kWindowId1));
  EXPECT_FALSE(app_launch_info->event_flag.has_value());
  EXPECT_THAT(app_launch_info->container,
              testing::Optional(static_cast<int>(
                  apps::LaunchContainer::kLaunchContainerWindow)));
  EXPECT_THAT(
      app_launch_info->disposition,
      testing::Optional(static_cast<int>(WindowOpenDisposition::NEW_WINDOW)));
  EXPECT_FALSE(app_launch_info->arc_session_id.has_value());
  EXPECT_THAT(app_launch_info->display_id, testing::Optional(kDisplayId1));

  const std::vector<base::FilePath> expected_file_paths = {
      base::FilePath(kFilePath1), base::FilePath(kFilePath2)};
  EXPECT_EQ(expected_file_paths, app_launch_info->file_paths);

  EXPECT_TRUE(app_launch_info->intent);
  EXPECT_EQ(kIntentActionSend, app_launch_info->intent->action);
  EXPECT_EQ(kMimeType, app_launch_info->intent->mime_type);
  EXPECT_EQ(kShareText1, app_launch_info->intent->share_text);

  EXPECT_FALSE(
      app_launch_info->browser_extra_info.app_type_browser.has_value());
}

TEST_F(RestoreDataTest, GetWindowInfo) {
  // The app id and window id doesn't exist.
  auto window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  EXPECT_FALSE(window_info);

  // Add the app launch info, but do not modify the window info.
  AddAppLaunchInfos();
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  ASSERT_TRUE(window_info);
  EXPECT_FALSE(window_info->activation_index.has_value());
  EXPECT_FALSE(window_info->desk_id.has_value());
  EXPECT_FALSE(window_info->desk_guid.is_valid());
  EXPECT_FALSE(window_info->current_bounds.has_value());
  EXPECT_FALSE(window_info->window_state_type.has_value());

  // Modify the window info.
  ModifyWindowInfos();
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  ASSERT_TRUE(window_info);

  EXPECT_THAT(window_info->activation_index,
              testing::Optional(kActivationIndex1));
  EXPECT_THAT(window_info->desk_id, testing::Optional(kDeskId1));

  EXPECT_TRUE(window_info->desk_guid.is_valid());
  EXPECT_EQ(kDeskGuid1, window_info->desk_guid);

  EXPECT_THAT(window_info->current_bounds, testing::Optional(kCurrentBounds1));
  EXPECT_THAT(window_info->window_state_type,
              testing::Optional(kWindowStateType1));

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
  ASSERT_TRUE(app_window_info);
  EXPECT_EQ(0, app_window_info->state);
  EXPECT_EQ(kDisplayId2, app_window_info->display_id);
  EXPECT_FALSE(app_window_info->bounds);

  // Modify the window info.
  ModifyWindowInfos();

  app_window_info = data_it->second->GetAppWindowInfo();
  EXPECT_EQ(static_cast<int32_t>(kWindowStateType3), app_window_info->state);
  EXPECT_EQ(kDisplayId1, app_window_info->display_id);
  EXPECT_TRUE(app_window_info->bounds);
  EXPECT_EQ(kCurrentBounds3, app_window_info->bounds.value());
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
  ASSERT_TRUE(window_info);
  EXPECT_THAT(window_info->activation_index,
              testing::Optional(kActivationIndex3));

  restore_data().SetNextRestoreWindowIdForChromeApp(kAppId1);

  // Verify that the activation index is modified as INT32_MAX.
  EXPECT_EQ(kWindowId1, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId1);
  ASSERT_TRUE(window_info);
  EXPECT_THAT(window_info->activation_index, testing::Optional(INT32_MAX));

  // Verify that the activation index is modified as INT32_MAX.
  EXPECT_EQ(kWindowId2, restore_data().FetchRestoreWindowId(kAppId1));
  window_info = restore_data().GetWindowInfo(kAppId1, kWindowId2);
  ASSERT_TRUE(window_info);
  EXPECT_THAT(window_info->activation_index, testing::Optional(INT32_MAX));

  EXPECT_EQ(0, restore_data().FetchRestoreWindowId(kAppId1));
}

TEST_F(RestoreDataTest, HasAppTypeBrowser) {
  auto app_launch_info1 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId1);
  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  EXPECT_FALSE(restore_data().HasAppTypeBrowser());

  auto app_launch_info2 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId2);
  app_launch_info2->browser_extra_info.app_type_browser = true;
  restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
  EXPECT_TRUE(restore_data().HasAppTypeBrowser());
}

TEST_F(RestoreDataTest, HasBrowser) {
  auto app_launch_info1 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId1);
  app_launch_info1->browser_extra_info.app_type_browser = true;
  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  EXPECT_FALSE(restore_data().HasBrowser());

  auto app_launch_info2 =
      std::make_unique<AppLaunchInfo>(app_constants::kChromeAppId, kWindowId2);
  restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
  EXPECT_TRUE(restore_data().HasBrowser());
}

TEST_F(RestoreDataTest, UpdateAppIdToLacros) {
  auto app_launch_info1 =
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
  auto app_launch_info_1 = std::make_unique<AppLaunchInfo>(
      kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
      std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                  base::FilePath(kFilePath2)},
      MakeIntent(kIntentActionSend, kMimeType, kShareText1));

  app_launch_info_1->browser_extra_info.app_type_browser = kAppTypeBrower2;
  app_launch_info_1->browser_extra_info.first_non_pinned_tab_index =
      kFirstNonPinnedTabIndex;
  PopulateTestTabgroups(app_launch_info_1->browser_extra_info.tab_group_infos);

  // Same as `app_launch_info_1`.
  auto app_launch_info_2 = std::make_unique<AppLaunchInfo>(
      kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
      std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                  base::FilePath(kFilePath2)},
      MakeIntent(kIntentActionSend, kMimeType, kShareText1));

  app_launch_info_2->browser_extra_info.app_type_browser = kAppTypeBrower2;
  app_launch_info_2->browser_extra_info.first_non_pinned_tab_index =
      kFirstNonPinnedTabIndex;
  PopulateTestTabgroups(app_launch_info_2->browser_extra_info.tab_group_infos);

  auto app_launch_info_3 = std::make_unique<AppLaunchInfo>(
      kAppId1, kWindowId2, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
      std::vector<base::FilePath>{base::FilePath(kFilePath2)},
      MakeIntent(kIntentActionView, kMimeType, kShareText2));

  auto app_restore_data_1 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_1));

  auto app_restore_data_2 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_2));

  auto app_restore_data_3 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_3));

  EXPECT_TRUE(*app_restore_data_1 == *app_restore_data_2);
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_3);

  // Modify tab groups of app_restore_data_2.
  app_restore_data_2->browser_extra_info.tab_group_infos.push_back(
      MakeTestTabGroup(kTestTabGroupTitleThree, kTestTabGroupColorThree));
  EXPECT_TRUE(*app_restore_data_1 != *app_restore_data_2);
}

TEST_F(RestoreDataTest, CompareAppRestoreDataIntent) {
  // Intent is nullptr.
  auto app_launch_info_1 = std::make_unique<AppLaunchInfo>(
      kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
      std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                  base::FilePath(kFilePath2)},
      nullptr);

  // Same as `app_launch_info_1`.
  auto app_launch_info_2 = std::make_unique<AppLaunchInfo>(
      kAppId1, kWindowId1, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
      std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                  base::FilePath(kFilePath2)},
      nullptr);

  auto app_restore_data_1 =
      std::make_unique<AppRestoreData>(std::move(app_launch_info_1));

  auto app_restore_data_2 =
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
