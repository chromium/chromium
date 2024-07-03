// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/ash/ash_web_view_impl.h"
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

  // Select a playlist with a type and verify that a media widget is created.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
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

IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest,
                       CheckSoundsPlayedDuringSessionHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();

  // 1. No playlist playing during the session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(sounds_controller->selected_playlist().empty());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistTypesSelectedDuringSession,
      /*sample=*/
      focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType::
          kNone,
      /*expected_count=*/1);

  // 2. Only the type of soundscape playlist playing during the session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());
  EXPECT_TRUE(FindMediaWidget());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistTypesSelectedDuringSession,
      /*sample=*/
      focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType::
          kSoundscapes,
      /*expected_count=*/1);

  // 3. Only the type of YouTube Music playlist playing during the session.
  selected_playlist.id = "id1";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistTypesSelectedDuringSession,
      /*sample=*/
      focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType::
          kYouTubeMusic,
      /*expected_count=*/1);

  // 4. The two types of playlists playing during the session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());

  selected_playlist.id = "id3";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_EQ(sounds_controller->selected_playlist().type,
            focus_mode_util::SoundType::kSoundscape);

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kPlaylistTypesSelectedDuringSession,
      /*sample=*/
      focus_mode_histogram_names::PlaylistTypesSelectedDuringFocusSessionType::
          kYouTubeMusicAndSoundscapes,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest,
                       CheckPlaylistsPlayedDuringSessionHistogram) {
  base::HistogramTester histogram_tester;

  auto* controller = FocusModeController::Get();
  auto* sounds_controller = controller->focus_mode_sounds_controller();

  // 1. No playlist played during the session.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(sounds_controller->selected_playlist().empty());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kCountPlaylistsPlayedDuringSession,
      /*sample=*/0, /*expected_count=*/1);

  // 2. Two playlists played during the session.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());

  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());

  selected_playlist.id = "id1";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  sounds_controller->TogglePlaylist(selected_playlist);

  // De-select the playlist and the histogram will not record it.
  sounds_controller->TogglePlaylist(sounds_controller->selected_playlist());

  controller->ToggleFocusMode();
  EXPECT_FALSE(controller->in_focus_session());
  histogram_tester.ExpectBucketCount(
      /*name=*/focus_mode_histogram_names::kCountPlaylistsPlayedDuringSession,
      /*sample=*/2, /*expected_count=*/1);
}

// Tests that the source title shown in the media controls for the associated
// Focus Mode media widget is overridden and not empty.
IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest, MediaSourceTitle) {
  // Toggle on focus mode.
  auto* focus_mode_controller = FocusModeController::Get();
  focus_mode_controller->ToggleFocusMode();
  EXPECT_TRUE(focus_mode_controller->in_focus_session());

  // Select a playlist and verify that a media widget is created.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.title = "Playlist Title";
  selected_playlist.type = focus_mode_util::SoundType::kYouTubeMusic;
  auto* sounds_controller =
      focus_mode_controller->focus_mode_sounds_controller();
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_FALSE(sounds_controller->selected_playlist().empty());

  auto* widget = FindMediaWidget();
  EXPECT_TRUE(widget);

  // Verify that there is a source title.
  AshWebViewImpl* web_view_impl =
      static_cast<AshWebViewImpl*>(widget->GetContentsView());
  std::string source_title =
      web_view_impl->GetTitleForMediaControls(web_view_impl->web_contents());
  EXPECT_FALSE(source_title.empty());
}

}  // namespace ash
