// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"

namespace ash {
namespace {

class FocusModePolicyTest : public policy::PolicyTest {
 public:
  FocusModePolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFocusMode, features::kFocusModeYTM},
        /*disabled_features=*/{});
  }
  ~FocusModePolicyTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();
    FocusModeController::Get()
        ->focus_mode_sounds_controller()
        ->SetIsMinorUserForTesting(false);
  }

  void SetPolicyValue(std::string_view value) {
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kFocusModeSoundsEnabled,
              base::Value(value));
    provider_.UpdateChromePolicy(policies);
  }

  FocusModeSoundsView* GetSoundsView(QuickSettingsView* quick_settings) {
    views::View* detailed_view = quick_settings->detailed_view_container();
    return reinterpret_cast<FocusModeSoundsView*>(
        detailed_view->GetViewByID(FocusModeDetailedView::ViewId::kSoundView));
  }

  void ClickOnFocusTile(views::View* panel) {
    views::Button* tile = static_cast<views::Button*>(
        panel->GetViewByID(VIEW_ID_FEATURE_TILE_FOCUS_MODE));
    tile->button_controller()->NotifyClick();
    base::RunLoop().RunUntilIdle();
  }

  QuickSettingsView* OpenQuickSettings() {
    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    return system_tray->bubble()->quick_settings_view();
  }

 private:
  Shelf* GetPrimaryShelf() {
    return Shell::GetPrimaryRootWindowController()->shelf();
  }

  UnifiedSystemTray* GetPrimaryUnifiedSystemTray() {
    return GetPrimaryShelf()->GetStatusAreaWidget()->unified_system_tray();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FocusModePolicyTest, FocusModeSounds_Enabled) {
  SetPolicyValue("enabled");
  auto* quick_settings = OpenQuickSettings();
  ClickOnFocusTile(quick_settings);
  FocusModeSoundsView* sounds_view = GetSoundsView(quick_settings);
  EXPECT_THAT(sounds_view->GetVisible(), testing::Eq(true));
  EXPECT_THAT(sounds_view->soundscape_views(),
              testing::Pair(testing::NotNull(), testing::NotNull()));
  EXPECT_THAT(sounds_view->youtube_music_views(),
              testing::Pair(testing::NotNull(), testing::NotNull()));
}

IN_PROC_BROWSER_TEST_F(FocusModePolicyTest, FocusModeSounds_FocusSoundsOnly) {
  SetPolicyValue("focus-sounds");
  auto* quick_settings = OpenQuickSettings();
  ClickOnFocusTile(quick_settings);
  FocusModeSoundsView* sounds_view = GetSoundsView(quick_settings);
  EXPECT_THAT(sounds_view->GetVisible(), testing::Eq(true));
  // For this case, we will show a label for the soundscape playlists instead of
  // a `TabSliderButton`.
  EXPECT_THAT(sounds_view->soundscape_views(),
              testing::Pair(testing::IsNull(), testing::NotNull()));
  EXPECT_THAT(sounds_view->youtube_music_views(),
              testing::Pair(testing::IsNull(), testing::IsNull()));
}

IN_PROC_BROWSER_TEST_F(FocusModePolicyTest, FocusModeSounds_Disabled) {
  SetPolicyValue("disabled");
  auto* quick_settings = OpenQuickSettings();
  ClickOnFocusTile(quick_settings);
  EXPECT_THAT(GetSoundsView(quick_settings)->GetVisible(), testing::Eq(false));
}

}  // namespace
}  // namespace ash
