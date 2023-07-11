// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "components/user_education/views/help_bubble_view.h"
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
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAudioIsAudible);

constexpr base::TimeDelta kShortDelay = base::Seconds(1);
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";

class QuitRunLoopOnMemoryMetricsRefreshObserver
    : public performance_manager::user_tuning::UserPerformanceTuningManager::
          Observer {
 public:
  explicit QuitRunLoopOnMemoryMetricsRefreshObserver(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~QuitRunLoopOnMemoryMetricsRefreshObserver() override = default;

  void OnMemoryMetricsRefreshed() override { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class HighEfficiencyInteractiveTest : public InteractiveBrowserTest {
 public:
  HighEfficiencyInteractiveTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_clock_.Advance(kShortDelay);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(true);

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* GetWebContentsAt(int index) {
    return browser()->tab_strip_model()->GetWebContentsAt(index);
  }

  bool IsTabDiscarded(int tab_index) {
    return GetWebContentsAt(tab_index)->WasDiscarded();
  }

  auto CheckTabIsDiscarded(int tab_index) {
    return Check(base::BindLambdaForTesting(
        [=]() { return IsTabDiscarded(tab_index); }));
  }

  auto CheckTabIsNotDiscarded(int tab_index) {
    return Check(base::BindLambdaForTesting(
        [=]() { return !IsTabDiscarded(tab_index); }));
  }

  auto TryDiscardTab(int tab_index) {
    return Do(base::BindLambdaForTesting([=]() {
      performance_manager::user_tuning::UserPerformanceTuningManager::
          GetInstance()
              ->DiscardPageForTesting(GetWebContentsAt(tab_index));
    }));
  }

  auto ForceRefreshMemoryMetrics() {
    return Do(base::BindLambdaForTesting([]() {
      performance_manager::user_tuning::UserPerformanceTuningManager* manager =
          performance_manager::user_tuning::UserPerformanceTuningManager::
              GetInstance();

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      QuitRunLoopOnMemoryMetricsRefreshObserver observer(
          run_loop.QuitClosure());
      base::ScopedObservation<
          performance_manager::user_tuning::UserPerformanceTuningManager,
          QuitRunLoopOnMemoryMetricsRefreshObserver>
          memory_metrics_observer(&observer);
      memory_metrics_observer.Observe(manager);

      performance_manager::PerformanceManager::CallOnGraph(
          FROM_HERE,
          base::BindLambdaForTesting([](performance_manager::Graph* graph) {
            auto* metrics_decorator = graph->GetRegisteredObjectAs<
                performance_manager::ProcessMetricsDecorator>();
            metrics_decorator->RequestImmediateMetrics();
          }));

      run_loop.Run();
    }));
  }

  // Attempts to discard the tab at discard_tab_index and navigates to that
  // tab and waits for it to reload
  auto DiscardAndSelectTab(int discard_tab_index,
                           const ui::ElementIdentifier& contents_id) {
    return Steps(FlushEvents(),
                 // This has to be done on a fresh message loop to prevent
                 // a tab being discarded while it is notifying its observers
                 TryDiscardTab(discard_tab_index), WaitForHide(contents_id),
                 SelectTab(kTabStripElementId, discard_tab_index),
                 WaitForShow(contents_id));
  }

  GURL GetURL(base::StringPiece path) {
    return embedded_test_server()->GetURL("example.com", path);
  }

  GURL GetURL(base::StringPiece hostname, base::StringPiece path) {
    return embedded_test_server()->GetURL(hostname, path);
  }

 private:
  base::SimpleTestTickClock test_clock_;
  resource_coordinator::ScopedSetTickClockForTesting
      scoped_set_tick_clock_for_testing_;
};

// Tests Discarding on pages with various types of content
class HighEfficiencyDiscardPolicyInteractiveTest
    : public HighEfficiencyInteractiveTest {
 public:
  HighEfficiencyDiscardPolicyInteractiveTest() = default;
  ~HighEfficiencyDiscardPolicyInteractiveTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HighEfficiencyInteractiveTest::SetUpCommandLine(command_line);
    // Some builders are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  auto PressKeyboard() {
    return Do(base::BindLambdaForTesting([=]() {
      ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_A, false,
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
      WaitForStateChange(kFirstTabContents, video_is_playing),
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
// TODO(crbug.com/1415833): Re-enable this test
IN_PROC_BROWSER_TEST_F(HighEfficiencyDiscardPolicyInteractiveTest,
                       DISABLED_TabWithFormNotDiscarded) {
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
                  "(el) => { el.focus(); el.select(); }"),
      WaitForStateChange(kFirstTabContents, input_is_focused), PressKeyboard(),
      WaitForStateChange(kFirstTabContents, input_value_updated),

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

// Tests the functionality of the High Efficiency page action chip
class HighEfficiencyChipInteractiveTest : public HighEfficiencyInteractiveTest {
 public:
  HighEfficiencyChipInteractiveTest() = default;
  ~HighEfficiencyChipInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kDiscardExceptionsImprovements);

    HighEfficiencyInteractiveTest::SetUp();
  }

  void SetUpOnMainThread() override {
    HighEfficiencyInteractiveTest::SetUpOnMainThread();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(browser()->tab_strip_model());
  }

  PageActionIconView* GetPageActionIconView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

  auto CheckChipIsExpandedState() {
    return CheckViewProperty(kHighEfficiencyChipElementId,
                             &PageActionIconView::ShouldShowLabel, true);
  }

  auto CheckChipIsCollapsedState() {
    return CheckViewProperty(kHighEfficiencyChipElementId,
                             &PageActionIconView::ShouldShowLabel, false);
  }

  // Discard and reload the tab at discard_tab_index the number of times the
  // high efficiency page action chip can expand so subsequent discards
  // will result in the chip staying in its collapsed state
  auto DiscardTabUntilChipStopsExpanding(
      size_t discard_tab_index,
      size_t non_discard_tab_index,
      const ui::ElementIdentifier& contents_id) {
    MultiStep result;
    for (int i = 0; i < HighEfficiencyChipTabHelper::kChipAnimationCount; i++) {
      MultiStep temp = std::move(result);
      result = Steps(std::move(temp),
                     SelectTab(kTabStripElementId, non_discard_tab_index),
                     DiscardAndSelectTab(discard_tab_index, contents_id),
                     CheckChipIsExpandedState());
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
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest, PageActionChipShows) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kHighEfficiencyChipElementId),
      DiscardTabUntilChipStopsExpanding(0, 1, kFirstTabContents),
      SelectTab(kTabStripElementId, 1),
      DiscardAndSelectTab(0, kFirstTabContents), CheckChipIsCollapsedState());
}

// Page Action chip should collapses after navigating to a tab without a chip
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       PageActionChipCollapseOnTabSwitch) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GetURL("/title1.html")),
      EnsureNotPresent(kHighEfficiencyChipElementId),
      DiscardAndSelectTab(0, kFirstTabContents), CheckChipIsExpandedState(),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kHighEfficiencyChipElementId),
      SelectTab(kTabStripElementId, 0), CheckChipIsCollapsedState(),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kHighEfficiencyChipElementId));
}

// Page Action chip should stay collapsed when navigating between two
// discarded tabs
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       ChipCollapseRemainCollapse) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GetURL("/title1.html")),
      EnsureNotPresent(kHighEfficiencyChipElementId),
      DiscardAndSelectTab(0, kFirstTabContents), CheckChipIsExpandedState(),
      DiscardAndSelectTab(1, kSecondTabContents), CheckChipIsExpandedState(),
      SelectTab(kTabStripElementId, 0), CheckChipIsCollapsedState(),
      SelectTab(kTabStripElementId, 1), CheckChipIsCollapsedState());
}

// Page Action chip should only show on discarded non-chrome pages
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       ChipShowsOnNonChromeSites) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      // Discards tab on non-chrome page
      DiscardAndSelectTab(0, kFirstTabContents),
      WaitForShow(kHighEfficiencyChipElementId),

      // Discards tab on chrome://newtab page
      TryDiscardTab(1), WaitForHide(kSecondTabContents), CheckTabIsDiscarded(1),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(kHighEfficiencyChipElementId));
}

// Clicking on the settings link in high efficiency dialog bubble should open
// a new tab and navigate to the performance settings page
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       BubbleSettingsLinkNavigates) {
  constexpr char kPerformanceSettingsLinkViewName[] = "performance_link";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      SelectTab(kTabStripElementId, 1), SelectTab(kTabStripElementId, 0),
      CheckChipIsCollapsedState(), PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      InAnyContext(NameViewRelative(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId,
          kPerformanceSettingsLinkViewName,
          base::BindOnce([](views::StyledLabel* label) -> views::View* {
            return label->GetFirstLinkForTesting();
          }))),
      MoveMouseTo(kPerformanceSettingsLinkViewName), ClickMouse(),
      Check(base::BindLambdaForTesting(
          [&]() { return browser()->tab_strip_model()->GetTabCount() == 3; })),
      InstrumentTab(kPerformanceSettingsTab, 2),
      WaitForWebContentsReady(kPerformanceSettingsTab,
                              GURL(chrome::kChromeUIPerformanceSettingsURL)));
}

// High Efficiency Dialog bubble should close after clicking the "OK" button
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnOkButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      PressButton(HighEfficiencyBubbleView::kHighEfficiencyDialogOkButton),
      WaitForHide(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency dialog bubble should close after clicking on the "X"
// close button
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnCloseButtonClick) {
  constexpr char kDialogCloseButton[] = "dialog_close_button";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      NameView(kDialogCloseButton, base::BindLambdaForTesting([&]() {
                 return static_cast<views::View*>(GetPageActionIconView()
                                                      ->GetBubble()
                                                      ->GetBubbleFrameView()
                                                      ->close_button());
               })),
      PressButton(kDialogCloseButton),
      EnsureNotPresent(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency Dialog bubble should close after clicking on
// the page action chip again
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnChipClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      PressButton(kHighEfficiencyChipElementId),
      EnsureNotPresent(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// High Efficiency dialog bubble should close when clicking to navigate to
// another tab
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CloseBubbleOnTabSwitch) {
  constexpr char kSecondTab[] = "second_tab";

  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      NameTab(1, kSecondTab), MoveMouseTo(kSecondTab), ClickMouse(),
      WaitForHide(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId));
}

// TODO(crbug.com/1416372): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_BubbleCorrectlyReportingMemorySaved \
  DISABLED_BubbleCorrectlyReportingMemorySaved
#else
#define MAYBE_BubbleCorrectlyReportingMemorySaved \
  BubbleCorrectlyReportingMemorySaved
#endif
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       MAYBE_BubbleCorrectlyReportingMemorySaved) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      ForceRefreshMemoryMetrics(), DiscardAndSelectTab(0, kFirstTabContents),
      WaitForShow(kHighEfficiencyChipElementId),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckView(
          HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId,
          base::BindOnce(
              [](Browser* browser, views::StyledLabel* label) {
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

// High Efficiency Dialog bubble should add the site it is currently on
// to the exceptions list if the cancel button of the dialog bubble is clicked.
// Opening the dialog button again will cause the cancel button to give users
// the option to go to settings instead.
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       ModifyExceptionsListOnCancelButtonClick) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      DiscardAndSelectTab(0, kFirstTabContents),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckViewProperty(
          HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(
              IDS_HIGH_EFFICIENCY_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST)),
      // Clicking the dialog's cancel button should add the site to the
      // exception list
      PressButton(HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton),
      WaitForHide(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      Do(base::BindLambdaForTesting([=]() {
        PrefService* const pref_service = browser()->profile()->GetPrefs();
        const base::Value::List& discard_exception = pref_service->GetList(
            performance_manager::user_tuning::prefs::kTabDiscardingExceptions);
        EXPECT_EQ(1u, discard_exception.size());
        std::string current_site_host = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetURL()
                                            .host();
        std::string added_exception = discard_exception.front().GetString();
        EXPECT_EQ(current_site_host, added_exception);
      })),
      FlushEvents(),
      // Dialog's cancel button should now allow users to navigate to the
      // performance settings page
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckViewProperty(
          HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT)),
      PressButton(HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton),
      WaitForHide(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      Check(base::BindLambdaForTesting(
          [&]() { return browser()->tab_strip_model()->GetTabCount() == 3; })),
      InstrumentTab(kPerformanceSettingsTab, 2),
      WaitForWebContentsReady(kPerformanceSettingsTab,
                              GURL(chrome::kChromeUIPerformanceSettingsURL)));
}

// High Efficiency Dialog bubble's cancel button's state should be preserved
// for that tab even when navigating to another tab.
IN_PROC_BROWSER_TEST_F(HighEfficiencyChipInteractiveTest,
                       CancelButtonStatePreseveredWhenSwitchingTabs) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("a.test", "/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GetURL("b.test", "/title1.html")),
      DiscardAndSelectTab(0, kFirstTabContents), TryDiscardTab(1),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      // Add site to the exceptions list
      PressButton(HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton),
      WaitForHide(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      FlushEvents(),
      // Check that the cancel button can go to settings page
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckViewProperty(
          HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT)),
      PressButton(kHighEfficiencyChipElementId),
      WaitForHide(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      // Second tab's cancel button should allow users to exclude the site
      // since this tab's site wasn't excluded yet
      SelectTab(kTabStripElementId, 1),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckViewProperty(
          HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(
              IDS_HIGH_EFFICIENCY_DIALOG_BUTTON_ADD_TO_EXCLUSION_LIST)),
      PressButton(kHighEfficiencyChipElementId),
      WaitForHide(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      // Ensure that the first tab's cancel button continues to allow users
      // to navigate to the settings page even after we selected another tab
      SelectTab(kTabStripElementId, 0),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(HighEfficiencyBubbleView::kHighEfficiencyDialogBodyElementId),
      CheckViewProperty(
          HighEfficiencyBubbleView::kHighEfficiencyDialogCancelButton,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(
              IDS_HIGH_EFFICIENCY_DIALOG_BODY_LINK_TEXT)));
}

struct FaviconScreenShotTestConfig {
  performance_manager::features::DiscardTabTreatmentOptions treatment_option;
  std::string screenshot_name;
  std::string cl_number;
};

class HighEfficiencyFaviconTreatmentTest
    : public HighEfficiencyInteractiveTest,
      public testing::WithParamInterface<FaviconScreenShotTestConfig> {
 public:
  HighEfficiencyFaviconTreatmentTest() = default;
  ~HighEfficiencyFaviconTreatmentTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        performance_manager::features::kDiscardedTabTreatment,
        {{"discard_tab_treatment_option", base::NumberToString(static_cast<int>(
                                              GetParam().treatment_option))}});

    HighEfficiencyInteractiveTest::SetUp();
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

IN_PROC_BROWSER_TEST_P(HighEfficiencyFaviconTreatmentTest,
                       FaviconTreatmentOnDiscard) {
  constexpr char kFirstTabFavicon[] = "first_tab_favicon";

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      Do(base::BindLambdaForTesting(
          [=]() { GetTabStrip()->StopAnimating(true); })),
      TryDiscardTab(0), CheckTabIsDiscarded(0),
      NameView(kFirstTabFavicon, base::BindLambdaForTesting([&]() {
                 return views::AsViewClass<views::View>(GetTabIcon(0));
               })),
      Check(base::BindLambdaForTesting([&]() {
        return GetTabIcon(0)
            ->GetTabDiscardAnimationForTesting()
            ->is_animating();
      })),
      Do(base::BindLambdaForTesting([&]() {
        // Force animation to end as it may not have finished progressing
        // before taking a screenshot
        GetTabIcon(0)->GetTabDiscardAnimationForTesting()->End();
      })),
      Screenshot(kFirstTabFavicon, GetParam().screenshot_name,
                 GetParam().cl_number));
}

std::vector<FaviconScreenShotTestConfig> HighEfficiencyTestConfig() {
  return {{performance_manager::features::DiscardTabTreatmentOptions::
               kFadeFullsizedFavicon,
           "FadeFullSizedFaviconOnDiscard", "4492205"},
          {performance_manager::features::DiscardTabTreatmentOptions::
               kFadeSmallFaviconWithRing,
           "FadeSmallFaviconOnDiscard", "4633624"}};
}

INSTANTIATE_TEST_SUITE_P(All,
                         HighEfficiencyFaviconTreatmentTest,
                         testing::ValuesIn(HighEfficiencyTestConfig()));

// Tests the new memory savings reporting improvements on the high efficiency
// dialog.
class HighEfficiencyMemorySavingsReportingImprovementsTest
    : public HighEfficiencyInteractiveTest {
 public:
  HighEfficiencyMemorySavingsReportingImprovementsTest() = default;
  ~HighEfficiencyMemorySavingsReportingImprovementsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kMemorySavingsReportingImprovements);

    HighEfficiencyInteractiveTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The high efficiency chip dialog renders a gauge style visualization that
// must be rendered correctly.
IN_PROC_BROWSER_TEST_F(HighEfficiencyMemorySavingsReportingImprovementsTest,
                       RenderVisualizationInDialog) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL("/title1.html")),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      ForceRefreshMemoryMetrics(), DiscardAndSelectTab(0, kFirstTabContents),
      Do(base::BindLambdaForTesting([&]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* pre_discard_resource_usage =
            performance_manager::user_tuning::UserPerformanceTuningManager::
                PreDiscardResourceUsage::FromWebContents(web_contents);
        pre_discard_resource_usage->SetMemoryFootprintEstimateKbForTesting(
            135 * 1024);
      })),
      PressButton(kHighEfficiencyChipElementId),
      WaitForShow(
          HighEfficiencyBubbleView::kHighEfficiencyDialogResourceViewElementId),
      Screenshot(
          HighEfficiencyBubbleView::kHighEfficiencyDialogResourceViewElementId,
          "HighEfficiencyResourceView", "4546555"));
}
