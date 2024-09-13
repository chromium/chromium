// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_textfield.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/unified/quick_settings_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/accessibility/spoken_feedback_browsertest.h"
#include "chrome/browser/ui/ash/web_view/ash_web_view_impl.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_utils.h"
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

void SimulatePlaybackState(bool is_playing) {
  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kActive;
  session_info->playback_state =
      is_playing ? media_session::mojom::MediaPlaybackState::kPlaying
                 : media_session::mojom::MediaPlaybackState::kPaused;

  FocusModeController::Get()
      ->focus_mode_sounds_controller()
      ->MediaSessionInfoChanged(std::move(session_info));
}

QuickSettingsView* OpenQuickSettings() {
  UnifiedSystemTray* system_tray = Shell::GetPrimaryRootWindowController()
                                       ->shelf()
                                       ->GetStatusAreaWidget()
                                       ->unified_system_tray();
  system_tray->ShowBubble();
  return system_tray->bubble()->quick_settings_view();
}

void ClickOnFocusTile(QuickSettingsView* panel) {
  views::Button* tile = views::AsViewClass<views::Button>(
      panel->GetViewByID(VIEW_ID_FEATURE_TILE_FOCUS_MODE));
  tile->button_controller()->NotifyClick();
  base::RunLoop().RunUntilIdle();
}

SystemTextfield* GetTimerTextfield(QuickSettingsView* quick_settings) {
  FocusModeDetailedView* detailed_view =
      quick_settings->GetDetailedViewForTest<FocusModeDetailedView>();
  EXPECT_TRUE(detailed_view);
  return views::AsViewClass<SystemTextfield>(detailed_view->GetViewByID(
      FocusModeDetailedView::ViewId::kTimerTextfield));
}

PillButton* GetToggleFocusButton(QuickSettingsView* quick_settings) {
  FocusModeDetailedView* detailed_view =
      quick_settings->GetDetailedViewForTest<FocusModeDetailedView>();
  EXPECT_TRUE(detailed_view);
  return views::AsViewClass<PillButton>(detailed_view->GetViewByID(
      FocusModeDetailedView::ViewId::kToggleFocusButton));
}

}  // namespace

class FocusModeBrowserTest : public InProcessBrowserTest {
 public:
  FocusModeBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kFocusMode, features::kFocusModeYTM}, {});
  }
  ~FocusModeBrowserTest() override = default;
  FocusModeBrowserTest(const FocusModeBrowserTest&) = delete;
  FocusModeBrowserTest& operator=(const FocusModeBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    FocusModeController::Get()
        ->focus_mode_sounds_controller()
        ->SetIsMinorUserForTesting(false);
  }

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

  // The media widget should still exist when the ending moment is triggered.
  controller->TriggerEndingMomentImmediately();
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(FindMediaWidget());

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

// Tests that the ending moment will pause the playlist without closing the
// media widget.
IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest, PauseMusicDuringEndingMoment) {
  auto* controller = FocusModeController::Get();
  EXPECT_FALSE(controller->in_focus_session());

  // Toggle on focus mode. Verify that there is no media widget since there is
  // no selected playlist.
  controller->ToggleFocusMode();
  EXPECT_TRUE(controller->in_focus_session());
  auto* sounds_controller = controller->focus_mode_sounds_controller();
  sounds_controller->set_simulate_playback_for_testing();

  // Case 1. If the music is paused by the ending moment, when extending the
  // session, it should be resumed. Select a playlist with a type and verify
  // that a media widget is created.
  focus_mode_util::SelectedPlaylist selected_playlist;
  selected_playlist.id = "id0";
  selected_playlist.type = focus_mode_util::SoundType::kSoundscape;
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_TRUE(FindMediaWidget());
  // Simulate the playlist is playing.
  SimulatePlaybackState(/*is_playing=*/true);
  EXPECT_EQ(focus_mode_util::SoundState::kPlaying,
            sounds_controller->selected_playlist().state);

  controller->TriggerEndingMomentImmediately();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(FindMediaWidget());
  EXPECT_EQ(focus_mode_util::SoundState::kPaused,
            sounds_controller->selected_playlist().state);

  // Extending the session will resume the playlist.
  controller->ExtendSessionDuration();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(focus_mode_util::SoundState::kPlaying,
            sounds_controller->selected_playlist().state);

  // Case 2. If the user paused the playlist before ending moment, after
  // extending the session, the playlist should still in paused state.
  SimulatePlaybackState(/*is_playing=*/false);
  EXPECT_EQ(focus_mode_util::SoundState::kPaused,
            sounds_controller->selected_playlist().state);

  controller->TriggerEndingMomentImmediately();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_ending_moment());
  EXPECT_TRUE(FindMediaWidget());

  controller->ExtendSessionDuration();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_EQ(focus_mode_util::SoundState::kPaused,
            sounds_controller->selected_playlist().state);

  // Case 3. If the user selected another playlist during the ending moment,
  // after extending the session, it should be the new playlist is playing.
  controller->TriggerEndingMomentImmediately();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_ending_moment());
  const auto old_playlist_id = sounds_controller->selected_playlist().id;

  selected_playlist.id = "id1";
  sounds_controller->TogglePlaylist(selected_playlist);
  EXPECT_EQ(focus_mode_util::SoundState::kSelected,
            sounds_controller->selected_playlist().state);

  controller->ExtendSessionDuration();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(controller->in_focus_session());
  EXPECT_TRUE(FindMediaWidget());
  EXPECT_NE(old_playlist_id, sounds_controller->selected_playlist().id);
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

// Tests that during the overvide mode, clicking on the focus panel will not end
// the overview mode.
IN_PROC_BROWSER_TEST_F(FocusModeBrowserTest, ClickOnFocusPanelInOverviewMode) {
  // Enter overview mode and open the focus panel.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  auto* quick_settings = OpenQuickSettings();
  ClickOnFocusTile(quick_settings);
  EXPECT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // 1. Click the timer textfield on the focus panel and stay in overview mode.
  auto* timer_textfield = GetTimerTextfield(quick_settings);
  EXPECT_TRUE(timer_textfield->GetVisible());

  test::Click(timer_textfield);
  EXPECT_TRUE(timer_textfield->HasFocus());
  EXPECT_TRUE(ash::OverviewController::Get()->InOverviewSession());

  // 2. Click the `Start` button to start a focus session and stay in overview
  // mode.
  auto* toggle_button = GetToggleFocusButton(quick_settings);
  EXPECT_TRUE(toggle_button->GetVisible());

  auto* focus_mode_controller = ash::FocusModeController::Get();
  EXPECT_FALSE(focus_mode_controller->in_focus_session());
  test::Click(toggle_button);
  EXPECT_TRUE(focus_mode_controller->in_focus_session());
  EXPECT_TRUE(ash::OverviewController::Get()->InOverviewSession());
}

class FocusModeSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 public:
  FocusModeSpokenFeedbackTest() = default;
  FocusModeSpokenFeedbackTest(const FocusModeSpokenFeedbackTest&) = delete;
  FocusModeSpokenFeedbackTest& operator=(const FocusModeSpokenFeedbackTest&) =
      delete;
  ~FocusModeSpokenFeedbackTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kFocusMode};
};

// Tests that when using `Search + Left/Right Arrow` key to navigate on the
// focus panel, the user update the timer texfield and start a focus session,
// which should also update the session duration for the controller.
IN_PROC_BROWSER_TEST_F(FocusModeSpokenFeedbackTest,
                       AfterA11yFocusRingOnTimerTextfield) {
  EnableChromeVox();

  // Set a session duration with 25 min and let the timer textfield gain the
  // focus.
  sm_.Call([] {
    auto* focus_mode_controller = FocusModeController::Get();
    focus_mode_controller->SetInactiveSessionDuration(base::Minutes(25));
    EXPECT_EQ(base::Minutes(25), focus_mode_controller->session_duration());

    // Open the focus panel.
    auto* quick_settings = OpenQuickSettings();
    ClickOnFocusTile(quick_settings);
    auto* timer_textfield = GetTimerTextfield(quick_settings);
    timer_textfield->RequestFocus();
  });
  sm_.ExpectSpeechPattern("Edit timer*");

  // Update the session duration from 25 min to 250 min by appending a `0` key
  // to the end of the text.
  sm_.Call([this] { SendKeyPress(ui::VKEY_0); });

  // Press `Search + Left Arrow` keys to the `Start Focus` button..
  sm_.Call([this] {
    SendKeyPressWithSearch(ui::VKEY_LEFT);
    SendKeyPressWithSearch(ui::VKEY_LEFT);
  });
  sm_.ExpectSpeechPattern("Start Focus*");

  // Press `Enter` key to start a focus session and Verify the session
  // duration..
  sm_.Call([this] {
    SendKeyPress(ui::VKEY_RETURN);
    EXPECT_EQ(base::Minutes(250),
              FocusModeController::Get()->session_duration());
  });
  sm_.Replay();
}

}  // namespace ash
