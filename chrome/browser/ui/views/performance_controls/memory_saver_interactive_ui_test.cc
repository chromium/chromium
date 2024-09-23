// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_interactive_test_mixin.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_chip_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAudioIsAudible);

constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

}  // namespace


// Tests Discarding on pages with various types of content
class MemorySaverDiscardPolicyInteractiveTest
    : public MemorySaverInteractiveTestMixin<InteractiveBrowserTest>,
      public ::testing::WithParamInterface<bool> {
 public:
  MemorySaverDiscardPolicyInteractiveTest() {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }
  ~MemorySaverDiscardPolicyInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MemorySaverInteractiveTestMixin<InteractiveBrowserTest>::SetUpCommandLine(
        command_line);
    // Some builders are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  auto PressKeyboard() {
    return Do(base::BindLambdaForTesting([=, this]() {
      // Send multiple key presses to reduce flakiness.
      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
                                                  false, false, false));
      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_B, false,
                                                  false, false, false));
    }));
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Check that a tab playing a video in the background won't be discarded
IN_PROC_BROWSER_TEST_P(MemorySaverDiscardPolicyInteractiveTest,
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
  video_is_playing.test_function = kMediaIsPlaying;

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          GetURL("example.com", "/media/bigbuck-player.html")),
      ExecuteJsAt(kFirstTabContents, video, kPlayVideo),
      WaitForStateChange(kFirstTabContents, video_is_playing),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsDiscarded(0, false));
}

// Check that a tab playing audio in the background won't be discarded
IN_PROC_BROWSER_TEST_P(MemorySaverDiscardPolicyInteractiveTest,
                       TabWithAudioNotDiscarded) {
  const DeepQuery audio = {"audio"};

  base::CallbackListSubscription subscription =
      RecentlyAudibleHelper::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0))
          ->RegisterCallbackForTesting(
              base::BindRepeating(&MemorySaverDiscardPolicyInteractiveTest::
                                      OnRecentlyAudibleCallback,
                                  base::Unretained(this), kFirstTabContents));

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          GetURL("example.com", "/autoplay_audio.html")),
      ExecuteJsAt(kFirstTabContents, audio, "(el) => { el.play(); }"),
      WaitForEvent(kFirstTabContents, kAudioIsAudible),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsDiscarded(0, false));
}

// Check that a form in the background but was interacted with by the user
// won't be discarded
// TODO(crbug.com/40893068): Consistently flakes, re-enable this test.
IN_PROC_BROWSER_TEST_P(MemorySaverDiscardPolicyInteractiveTest,
                       DISABLED_TabWithFormNotDiscarded) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputIsFocused);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputValueIsUpated);
  const DeepQuery input_text_box = {"#value"};

  StateChange input_is_focused;
  input_is_focused.event = kInputIsFocused;
  input_is_focused.where = input_text_box;
  input_is_focused.type = StateChange::Type::kExistsAndConditionTrue;
  input_is_focused.test_function =
      "(el) => { return el === document.activeElement; }";

  StateChange input_value_updated;
  input_value_updated.event = kInputValueIsUpated;
  input_value_updated.where = input_text_box;
  input_value_updated.type = StateChange::Type::kExistsAndConditionTrue;
  input_value_updated.test_function = "(el) => { return !!el.value; }";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          GetURL("example.com", "/form_search.html")),
      WaitForWebContentsReady(kFirstTabContents,
                              GetURL("example.com", "/form_search.html")),

      // Move focus off of the omnibox
      MoveMouseTo(kFirstTabContents, input_text_box), ClickMouse(),

      // Wait until the input text box is focused and simulate typing a letter
      ExecuteJsAt(kFirstTabContents, input_text_box,
                  "(el) => { el.focus(); el.select(); }"),
      WaitForStateChange(kFirstTabContents, input_is_focused), PressKeyboard(),
      WaitForStateChange(kFirstTabContents, input_value_updated),

      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsDiscarded(0, false));
}

// Check that tabs with enabled notifications won't be discarded
IN_PROC_BROWSER_TEST_P(MemorySaverDiscardPolicyInteractiveTest,
                       TabWithNotificationNotDiscarded) {
  // HTTPS because only secure origins can get the notification permission.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  // Grant notification permission by default (only works for secure origins).
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 ContentSetting::CONTENT_SETTING_ALLOW);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(
          kFirstTabContents,
          https_server.GetURL("a.test",
                              "/notifications/notification_tester.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsDiscarded(0, false));
}

// Tests the functionality of the Memory Saver page action chip
class MemorySaverChipInteractiveTest
    : public MemorySaverInteractiveTestMixin<InteractiveBrowserTest>,
      public ::testing::WithParamInterface<bool> {
 public:
  MemorySaverChipInteractiveTest() {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }
  ~MemorySaverChipInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    SetMemorySaverModeEnabled(true);
  }

  PageActionIconView* GetPageActionIconView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kMemorySaver);
  }

  auto CheckChipIsExpandedState(bool is_expanded) {
    return CheckViewProperty(kMemorySaverChipElementId,
                             &PageActionIconView::ShouldShowLabel, is_expanded);
  }

  // Discard and reload the tab at discard_tab_index the number of times the
  // memory saver page action chip can expand so subsequent discards
  // will result in the chip staying in its collapsed state
  auto DiscardTabUntilChipStopsExpanding(
      size_t discard_tab_index,
      size_t non_discard_tab_index,
      const ui::ElementIdentifier& contents_id) {
    MultiStep result;
    for (int i = 0; i < MemorySaverChipTabHelper::kChipAnimationCount; i++) {
      MultiStep temp = std::move(result);
      result = Steps(std::move(temp),
                     SelectTab(kTabStripElementId, non_discard_tab_index),
                     DiscardAndReloadTab(discard_tab_index, contents_id),
                     CheckChipIsExpandedState(true));
    }

    return result;
  }

  auto NameTab(size_t index, std::string name) {
    return NameViewRelative(
        kTabStripElementId, name,
        base::BindOnce([](size_t index, TabStrip* tab_strip)
                           -> views::View* { return tab_strip->tab_at(index); },
                       index));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Page Action Chip should appear expanded the first three times a tab is
// discarded and collapse all subsequent times
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest, PageActionChipShows) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kMemorySaverChipElementId),
      DiscardTabUntilChipStopsExpanding(0, 1, kFirstTabContents),
      SelectTab(kTabStripElementId, 1),
      DiscardAndReloadTab(0, kFirstTabContents),
      CheckChipIsExpandedState(false));
}

// Page Action chip should collapses after navigating to a tab without a chip
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       PageActionChipCollapseOnTabSwitch) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GetURL()),
      EnsureNotPresent(kMemorySaverChipElementId),
      DiscardAndReloadTab(0, kFirstTabContents), CheckChipIsExpandedState(true),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kMemorySaverChipElementId),
      SelectTab(kTabStripElementId, 0), CheckChipIsExpandedState(false),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kMemorySaverChipElementId));
}

// Page Action chip should stay collapsed when navigating between two
// discarded tabs
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       ChipCollapseRemainCollapse) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GetURL()),
      EnsureNotPresent(kMemorySaverChipElementId),
      DiscardAndReloadTab(0, kFirstTabContents), CheckChipIsExpandedState(true),
      DiscardAndReloadTab(1, kSecondTabContents),
      CheckChipIsExpandedState(true), SelectTab(kTabStripElementId, 0),
      CheckChipIsExpandedState(false), SelectTab(kTabStripElementId, 1),
      CheckChipIsExpandedState(false));
}

// Page Action chip should only show on discarded non-chrome pages
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       ChipShowsOnNonChromeSites) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      // Discards tab on non-chrome page
      DiscardAndReloadTab(0, kFirstTabContents),
      WaitForShow(kMemorySaverChipElementId),

      // Discards tab on chrome://newtab page
      TryDiscardTab(1), CheckTabIsDiscarded(1, true),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kMemorySaverChipElementId));
}

// Memory Saver Dialog bubble should close after clicking the "OK" button
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       CloseBubbleOnOkButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      PressButton(MemorySaverBubbleView::kMemorySaverDialogOkButton),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId));
}

// Memory Saver dialog bubble should close after clicking on the "X"
// close button
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       CloseBubbleOnCloseButtonClick) {
  constexpr char kDialogCloseButton[] = "dialog_close_button";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      NameView(kDialogCloseButton, base::BindLambdaForTesting([&]() {
                 return static_cast<views::View*>(GetPageActionIconView()
                                                      ->GetBubble()
                                                      ->GetBubbleFrameView()
                                                      ->close_button());
               })),
      PressButton(kDialogCloseButton),
      EnsureNotPresent(MemorySaverBubbleView::kMemorySaverDialogBodyElementId));
}

// Memory Saver Dialog bubble should close after clicking on
// the page action chip again
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest, CloseBubbleOnChipClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      PressButton(kMemorySaverChipElementId),
      EnsureNotPresent(MemorySaverBubbleView::kMemorySaverDialogBodyElementId));
}

// Memory Saver dialog bubble should close when clicking to navigate to
// another tab
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest, CloseBubbleOnTabSwitch) {
  constexpr char kSecondTab[] = "second_tab";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      NameTab(1, kSecondTab), MoveMouseTo(kSecondTab), ClickMouse(),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId));
}

IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       BubbleCorrectlyReportingMemorySaved) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      ForceRefreshMemoryMetrics(), DiscardAndReloadTab(0, kFirstTabContents),
      WaitForShow(kMemorySaverChipElementId),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverResourceView::
                      kMemorySaverResourceViewMemorySavingsElementId),
      CheckView(
          MemorySaverResourceView::
              kMemorySaverResourceViewMemorySavingsElementId,
          base::BindOnce(
              [](Browser* browser, views::Label* label) {
                content::WebContents* web_contents =
                    browser->tab_strip_model()->GetWebContentsAt(0);
                auto* pre_discard_resource_usage = performance_manager::
                    user_tuning::UserPerformanceTuningManager::
                        PreDiscardResourceUsage::FromWebContents(web_contents);
                int memory_estimate =
                    pre_discard_resource_usage->memory_footprint_estimate_kb();
                return label->GetText().find(ui::FormatBytes(
                           memory_estimate * 1024)) != std::string::npos;
              },
              browser())));
}

// Memory Saver Dialog bubble should add the site it is currently on
// to the exceptions list if the cancel button of the dialog bubble is clicked.
// Opening the dialog button again will cause the cancel button to give users
// the option to go to settings instead.
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       ModifyExceptionsListOnCancelButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndReloadTab(0, kFirstTabContents),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      CheckViewProperty(
          MemorySaverBubbleView::kMemorySaverDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(
              IDS_MEMORY_SAVER_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST)),
      // Clicking the dialog's cancel button should add the site to the
      // exception list
      PressButton(MemorySaverBubbleView::kMemorySaverDialogCancelButton),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      Do(base::BindLambdaForTesting([=, this]() {
        PrefService* const pref_service = browser()->profile()->GetPrefs();
        const base::Value::Dict& discard_exception =
            pref_service->GetDict(performance_manager::user_tuning::prefs::
                                      kTabDiscardingExceptionsWithTime);
        EXPECT_EQ(1u, discard_exception.size());
        std::string current_site_host = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetURL()
                                            .host();
        EXPECT_TRUE(discard_exception.contains(current_site_host));
      })),

      // Dialog's cancel button should now allow users to navigate to the
      // performance settings page
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      CheckViewProperty(
          MemorySaverBubbleView::kMemorySaverDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_SETTINGS_BUTTON)),
      PressButton(MemorySaverBubbleView::kMemorySaverDialogCancelButton),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      Check(base::BindLambdaForTesting(
          [&]() { return browser()->tab_strip_model()->GetTabCount() == 3; })),
      InstrumentTab(kPerformanceSettingsTab, 2),
      WaitForWebContentsReady(
          kPerformanceSettingsTab,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))));
}

// Memory Saver Dialog bubble's cancel button's state should be preserved
// for that tab even when navigating to another tab.
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       CancelButtonStatePreseveredWhenSwitchingTabs) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("a.test", "/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GetURL("b.test", "/title1.html")),
      DiscardAndReloadTab(0, kFirstTabContents), TryDiscardTab(1),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      // Add site to the exceptions list
      PressButton(MemorySaverBubbleView::kMemorySaverDialogCancelButton),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),

      // Check that the cancel button can go to settings page
      PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      CheckViewProperty(
          MemorySaverBubbleView::kMemorySaverDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_SETTINGS_BUTTON)),
      PressButton(kMemorySaverChipElementId),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      // Second tab's cancel button should allow users to exclude the site
      // since this tab's site wasn't excluded yet
      SelectTab(kTabStripElementId, 1), PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      CheckViewProperty(
          MemorySaverBubbleView::kMemorySaverDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(
              IDS_MEMORY_SAVER_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST)),
      PressButton(kMemorySaverChipElementId),
      WaitForHide(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      // Ensure that the first tab's cancel button continues to allow users
      // to navigate to the settings page even after we selected another tab
      SelectTab(kTabStripElementId, 0), PressButton(kMemorySaverChipElementId),
      WaitForShow(MemorySaverBubbleView::kMemorySaverDialogBodyElementId),
      CheckViewProperty(
          MemorySaverBubbleView::kMemorySaverDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_DIALOG_SETTINGS_BUTTON)));
}

// The memory saver chip dialog renders a gauge style visualization that
// must be rendered correctly.
IN_PROC_BROWSER_TEST_P(MemorySaverChipInteractiveTest,
                       RenderVisualizationInDialog) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      ForceRefreshMemoryMetrics(), DiscardAndReloadTab(0, kFirstTabContents),
      Do(base::BindLambdaForTesting([&]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* pre_discard_resource_usage =
            performance_manager::user_tuning::UserPerformanceTuningManager::
                PreDiscardResourceUsage::FromWebContents(web_contents);
        pre_discard_resource_usage->UpdateDiscardInfo(
            135 * 1024, LifecycleUnitDiscardReason::PROACTIVE);
      })),
      PressButton(kMemorySaverChipElementId),
      WaitForShow(
          MemorySaverBubbleView::kMemorySaverDialogResourceViewElementId),
      Screenshot(MemorySaverBubbleView::kMemorySaverDialogResourceViewElementId,
                 /*screenshot_name=*/"MemorySaverResourceView",
                 /*baseline_cl=*/"5280502"));
}

class MemorySaverDiscardIndicatorIPHTest
    : public MemorySaverInteractiveTestMixin<InteractiveFeaturePromoTest>,
      public ::testing::WithParamInterface<bool> {
 public:
  MemorySaverDiscardIndicatorIPHTest()
      : MemorySaverInteractiveTestMixin(
            InteractiveFeaturePromoTestApi::UseDefaultTrackerAllowingPromos(
                {feature_engagement::kIPHDiscardRingFeature})) {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }
  ~MemorySaverDiscardIndicatorIPHTest() override = default;

  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    SetMemorySaverModeEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(MemorySaverDiscardIndicatorIPHTest,
                       IPHAppearsWhenTabIsDiscarded) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      TryDiscardTab(0), CheckTabIsDiscarded(0, true),
      WaitForPromo(feature_engagement::kIPHDiscardRingFeature),
      PressNonDefaultPromoButton(), InstrumentTab(kThirdTabContents, 2),
      WaitForWebContentsReady(
          kThirdTabContents,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))));
}

class MemorySaverImprovedFaviconTreatmentTest
    : public WebUiInteractiveTestMixin<
          MemorySaverInteractiveTestMixin<InteractiveBrowserTest>>,
      public ::testing::WithParamInterface<bool> {
 public:
  static auto IsShowingDiscardIndicator(bool showing) {
    return [showing](TabIcon* tab_icon) {
      return showing == tab_icon->GetShowingDiscardIndicator();
    };
  }

  MemorySaverImprovedFaviconTreatmentTest() {
    scoped_feature_list_.InitWithFeatureState(features::kWebContentsDiscard,
                                              GetParam());
  }
  ~MemorySaverImprovedFaviconTreatmentTest() override = default;

  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    SetMemorySaverModeEnabled(true);
  }
  TabStrip* GetTabStrip() {
    return BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  }

  TabIcon* GetTabIcon(int tab_index) {
    return GetTabStrip()->tab_at(tab_index)->GetTabIconForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(MemorySaverImprovedFaviconTreatmentTest,
                       FaviconTreatmentOnDiscard) {
  constexpr char kFirstTabFavicon[] = "first_tab_favicon";

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      Do(base::BindLambdaForTesting(
          [=, this]() { GetTabStrip()->StopAnimating(true); })),
      TryDiscardTab(0), CheckTabIsDiscarded(0, true),
      NameView(kFirstTabFavicon, base::BindLambdaForTesting([&]() {
                 return views::AsViewClass<views::View>(GetTabIcon(0));
               })),
      WaitForEvent(kFirstTabFavicon, kDiscardAnimationFinishes),
      Screenshot(kFirstTabFavicon,
                 /*screenshot_name=*/"NoFadeSlightlySmallerFaviconOnDiscard",
                 /*baseline_cl=*/"5493847"));
}

IN_PROC_BROWSER_TEST_P(MemorySaverImprovedFaviconTreatmentTest,
                       DiscardRingTreatmentSetting) {
  constexpr char kFirstTabFavicon[] = "first_tab_favicon";
  const WebContentsInteractionTestUtil::DeepQuery
      discard_ring_treatment_setting = {
          "settings-ui",
          "settings-main",
          "settings-basic-page",
          "settings-performance-page",
          "settings-toggle-button#discardRingTreatmentToggleButton",
          "cr-toggle#control"};
  g_browser_process->local_state()->SetBoolean(
      performance_manager::user_tuning::prefs::kDiscardRingTreatmentEnabled,
      true);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      AddInstrumentedTab(
          kPerformanceSettingsTab,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      Do(base::BindLambdaForTesting(
          [=, this]() { GetTabStrip()->StopAnimating(true); })),
      TryDiscardTab(0), CheckTabIsDiscarded(0, true),
      NameView(kFirstTabFavicon, base::BindLambdaForTesting([&]() {
                 return views::AsViewClass<views::View>(GetTabIcon(0));
               })),
      CheckView(kFirstTabFavicon, IsShowingDiscardIndicator(true)),
      ClickElement(kPerformanceSettingsTab, discard_ring_treatment_setting),
      WaitForButtonStateChange(kPerformanceSettingsTab,
                               discard_ring_treatment_setting, false),
      CheckView(kFirstTabFavicon, IsShowingDiscardIndicator(false)),
      ClickElement(kPerformanceSettingsTab, discard_ring_treatment_setting),
      WaitForButtonStateChange(kPerformanceSettingsTab,
                               discard_ring_treatment_setting, true),
      CheckView(kFirstTabFavicon, IsShowingDiscardIndicator(true)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MemorySaverDiscardPolicyInteractiveTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        MemorySaverDiscardPolicyInteractiveTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

INSTANTIATE_TEST_SUITE_P(,
                         MemorySaverChipInteractiveTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<
                             MemorySaverChipInteractiveTest::ParamType>& info) {
                           return info.param ? "RetainedWebContents"
                                             : "UnretainedWebContents";
                         });

INSTANTIATE_TEST_SUITE_P(
    ,
    MemorySaverDiscardIndicatorIPHTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        MemorySaverDiscardIndicatorIPHTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });

INSTANTIATE_TEST_SUITE_P(
    ,
    MemorySaverImprovedFaviconTreatmentTest,
    ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<
        MemorySaverImprovedFaviconTreatmentTest::ParamType>& info) {
      return info.param ? "RetainedWebContents" : "UnretainedWebContents";
    });
