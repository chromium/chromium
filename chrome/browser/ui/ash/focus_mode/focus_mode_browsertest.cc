// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kFocusModeMediaWidgetName[] = "FocusModeMediaWidget";

views::Widget* FindMediaWidgetFromWindow(aura::Window* search_root) {
  if (views::Widget* const widget =
          views::Widget::GetWidgetForNativeWindow(search_root);
      widget && widget->GetName() == kFocusModeMediaWidgetName) {
    return widget;
  }

  // Keep searching in children.
  for (aura::Window* const child : search_root->children()) {
    if (views::Widget* const found = FindMediaWidgetFromWindow(child)) {
      return found;
    }
  }

  return nullptr;
}

views::Widget* FindMediaWidget() {
  return FindMediaWidgetFromWindow(Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_OverlayContainer));
}

}  // namespace

class FocusModeBrowserTest : public InProcessBrowserTest {
 public:
  FocusModeBrowserTest() {
    feature_list_.InitWithFeatures({features::kFocusMode}, {});
  }
  ~FocusModeBrowserTest() override = default;
  FocusModeBrowserTest(const FocusModeBrowserTest&) = delete;
  FocusModeBrowserTest& operator=(const FocusModeBrowserTest&) = delete;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Tests basic create/close media widget functionality.
IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest, MediaWidget) {
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Toggle on focus mode. Verify that there is no media widget since there is
  // no selected playlist.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  auto* sounds_controller = controller->focus_mode_sounds_controller();
  EXPECT_TRUE(sounds_controller->selected_playlist().empty());
  EXPECT_FALSE(FindMediaWidget());

  // Select a playlist and verify that a media widget is created.
  FocusModeSoundsController::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());
  EXPECT_TRUE(FindMediaWidget());

  // Swap playlists, then verify that the media widget still exists.
  selected_playlist.id = "id1";
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());
  EXPECT_TRUE(FindMediaWidget());

  // The media widget should be closed when the ending moment is triggered.
  controller->TriggerEndingMomentImmediately();
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_FALSE(FindMediaWidget());

  // If the user extends the time during the ending moment, the media widget
  // should be recreated.
  controller->ExtendSessionDuration();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(FindMediaWidget());

  // Toggling off focus mode should close the media widget.
  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  EXPECT_FALSE(FindMediaWidget());

  // Toggling on focus mode with a selected playlist should trigger creating a
  // media widget.
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(FindMediaWidget());
}

}  // namespace ash
