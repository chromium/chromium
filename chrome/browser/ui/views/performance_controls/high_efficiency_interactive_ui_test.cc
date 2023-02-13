// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAudioIsAudible);

constexpr base::TimeDelta kShortDelay = base::Seconds(1);
}  // namespace

class HighEfficiencyDiscardPolicyInteractiveTest
    : public InteractiveBrowserTest {
 public:
  HighEfficiencyDiscardPolicyInteractiveTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "1h"}}}},
        {});

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* GetWebContentsAt(int index) {
    return browser()->tab_strip_model()->GetWebContentsAt(index);
  }

  auto TryDiscardTab(int tab_index) {
    return Do(base::BindLambdaForTesting([=]() {
      performance_manager::user_tuning::UserPerformanceTuningManager::
          GetInstance()
              ->DiscardPageForTesting(GetWebContentsAt(tab_index));
    }));
  }

  auto CheckTabIsNotDiscarded(int tab_index) {
    return Check(base::BindLambdaForTesting(
        [=]() { return !GetWebContentsAt(tab_index)->WasDiscarded(); }));
  }

  auto PressKeyboard() {
    return Do(base::BindLambdaForTesting([=]() {
      ui_controls::SendKeyPress(browser()->window()->GetNativeWindow(),
                                ui::VKEY_A, false, false, false, false);
    }));
  }

  GURL GetURL(base::StringPiece path) {
    return embedded_test_server()->GetURL("example.com", path);
  }

  void OnRecentlyAudibleCallback(const ui::ElementIdentifier& contents_id,
                                 bool recently_audible) {
    if (recently_audible) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              contents_id, browser()->window()->GetElementContext()),
          kAudioIsAudible);
    }
  }

 private:
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that a tab playing a video in the background won't be discarded
IN_PROC_BROWSER_TEST_F(HighEfficiencyDiscardPolicyInteractiveTest,
                       TabWithVideoNotDiscarded) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kVideoIsPlaying);
  const char kPlayVideo[] = "(el) => { el.play(); }";
  const DeepQuery video = {"#video"};
  constexpr char kMediaIsPlaying[] =
      "(el) => { return el.currentTime > 0.1 && !el.paused && !el.ended && "
      "el.readyState > 2; }";

  StateChange video_is_playing;
  video_is_playing.event = kVideoIsPlaying;
  video_is_playing.where = video;
  video_is_playing.type = StateChange::Type::kConditionTrue;
  video_is_playing.test_function = kMediaIsPlaying;

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          GetURL("/media/bigbuck-player.html")),
      ExecuteJsAt(kFirstTabContents, video, kPlayVideo),
      WaitForStateChange(kFirstTabContents, std::move(video_is_playing)),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsNotDiscarded(0));
}

// Check that a tab playing audio in the background won't be discarded
IN_PROC_BROWSER_TEST_F(HighEfficiencyDiscardPolicyInteractiveTest,
                       TabWithAudioNotDiscarded) {
  const DeepQuery audio = {"audio"};

  base::CallbackListSubscription subscription =
      RecentlyAudibleHelper::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0))
          ->RegisterCallbackForTesting(
              base::BindRepeating(&HighEfficiencyDiscardPolicyInteractiveTest::
                                      OnRecentlyAudibleCallback,
                                  base::Unretained(this), kFirstTabContents));

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/autoplay_audio.html")),
      ExecuteJsAt(kFirstTabContents, audio, "(el) => { el.play(); }"),
      WaitForEvent(kFirstTabContents, kAudioIsAudible),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsNotDiscarded(0));
}

// Check that a form in the background but was interacted by the user
// won't be discarded
IN_PROC_BROWSER_TEST_F(HighEfficiencyDiscardPolicyInteractiveTest,
                       TabWithFormNotDiscarded) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputIsFocused);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputValueIsUpated);
  const DeepQuery input_text_box = {"#value"};

  StateChange input_is_focused;
  input_is_focused.event = kInputIsFocused;
  input_is_focused.where = input_text_box;
  input_is_focused.type = StateChange::Type::kConditionTrue;
  input_is_focused.test_function =
      "(el) => { return el === document.activeElement; }";

  StateChange input_value_updated;
  input_value_updated.event = kInputValueIsUpated;
  input_value_updated.where = input_text_box;
  input_value_updated.type = StateChange::Type::kConditionTrue;
  input_value_updated.test_function = "(el) => { return el.value === 'a'; }";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/form_search.html")),

      // Move focus off of the omnibox
      MoveMouseTo(kFirstTabContents, input_text_box), ClickMouse(),

      // Wait until the input text box is focused and simulate typing a letter
      ExecuteJsAt(kFirstTabContents, input_text_box,
                  "(el) => { el.select(); }"),
      WaitForStateChange(kFirstTabContents, std::move(input_is_focused)),
      PressKeyboard(),
      WaitForStateChange(kFirstTabContents, std::move(input_value_updated)),

      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsNotDiscarded(0));
}

// Check that tabs with enabled notifications won't be discarded
IN_PROC_BROWSER_TEST_F(HighEfficiencyDiscardPolicyInteractiveTest,
                       TabWithNotificationNotDiscarded) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 ContentSetting::CONTENT_SETTING_ALLOW);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          GetURL("/notifications/notification_tester.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsNotDiscarded(0));
}
