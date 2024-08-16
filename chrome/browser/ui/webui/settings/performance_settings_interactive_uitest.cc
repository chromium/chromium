// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/values_util.h"
#include "base/power_monitor/battery_state_sampler.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/performance_controls/test_support/battery_saver_browser_test_mixin.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_saver_interactive_test_mixin.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using performance_manager::user_tuning::prefs::BatterySaverModeState;
using performance_manager::user_tuning::prefs::MemorySaverModeAggressiveness;
using performance_manager::user_tuning::prefs::MemorySaverModeState;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsPage);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHides);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kExceptionDialogShows);

const WebContentsInteractionTestUtil::DeepQuery kMemorySaverToggleQuery = {
    "settings-ui",          "settings-main",          "settings-basic-page",
    "settings-memory-page", "settings-toggle-button", "cr-toggle#control"};

const WebContentsInteractionTestUtil::DeepQuery kMediumQuery = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-memory-page", "controlled-radio-button#mediumButton"};

const WebContentsInteractionTestUtil::DeepQuery kAggressiveQuery = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-memory-page", "controlled-radio-button#aggressiveButton"};

const WebContentsInteractionTestUtil::DeepQuery kConservativeQuery = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-memory-page", "controlled-radio-button#conservativeButton"};

const WebContentsInteractionTestUtil::DeepQuery kExceptionDialogEntry = {
    "settings-ui",
    "settings-main",
    "settings-basic-page",
    "settings-performance-page",
    "tab-discard-exception-list",
    "tab-discard-exception-tabbed-add-dialog",
    "tab-discard-exception-current-sites-list#list",
    "settings-checkbox-list-entry"};

const WebContentsInteractionTestUtil::DeepQuery kExceptionDialogAddButton = {
    "settings-ui",
    "settings-main",
    "settings-basic-page",
    "settings-performance-page",
    "tab-discard-exception-list",
    "tab-discard-exception-tabbed-add-dialog",
    "cr-button#actionButton"};

const WebContentsInteractionTestUtil::DeepQuery kPerformanceFeedbackButton = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-section#performanceSettingsSection", "cr-icon-button#feedback"};

const WebContentsInteractionTestUtil::DeepQuery kMemorySaverFeedbackButton = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-section#memorySettingsSection", "cr-icon-button#feedback"};

const WebContentsInteractionTestUtil::DeepQuery kBatterySaverFeedbackButton = {
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-section#batterySettingsSection", "cr-icon-button#feedback"};

}  // namespace

class PerformanceSettingsInteractiveTest
    : public MemorySaverInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  auto CheckDiscardRingTreatmentLogged(
      bool enabled,
      int expected_count,
      const base::HistogramTester& histogram_tester) {
    return Do(base::BindLambdaForTesting([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.MemorySaver.DiscardRingTreatment",
          static_cast<int>(enabled), expected_count);
    }));
  }
};

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       DiscardRingTreatmentMetricsShouldLogOnToggle) {
  base::HistogramTester histogram_tester;
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
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage,
                             discard_ring_treatment_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               discard_ring_treatment_setting, true),

      // Turn Off Discard Ring Treatment
      ClickElement(kPerformanceSettingsPage, discard_ring_treatment_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               discard_ring_treatment_setting, false),
      CheckDiscardRingTreatmentLogged(false, 1, histogram_tester),

      // Turn On Discard Ring Treatment
      ClickElement(kPerformanceSettingsPage, discard_ring_treatment_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               discard_ring_treatment_setting, true),
      CheckDiscardRingTreatmentLogged(true, 1, histogram_tester));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       PerformanceSendFeedbackDialogOpens) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      ClickElement(kPerformanceSettingsPage, kPerformanceFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

#elif BUILDFLAG(IS_CHROMEOS_ASH)
class PerformanceSettingsCrosInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveAshTest> {};

IN_PROC_BROWSER_TEST_F(PerformanceSettingsCrosInteractiveTest,
                       PerformanceSendFeedbackDialogOpens) {
  SetupContextWidget();
  InstallSystemApps();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackDialogElementId);
  CreateBrowserWindow(
      GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)));
  Browser* const browser = chrome::FindLastActive();
  ASSERT_NE(browser, nullptr);

  RunTestSequence(
      InContext(browser->window()->GetElementContext(),
                InstrumentTab(kPerformanceSettingsPage)),
      WaitForElementToRender(kPerformanceSettingsPage,
                             kPerformanceFeedbackButton),
      InstrumentNextTab(kOsFeedbackDialogElementId, AnyBrowser()),
      ClickElement(kPerformanceSettingsPage, kPerformanceFeedbackButton),
      WaitForShow(kOsFeedbackDialogElementId));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class MemorySettingsInteractiveTest
    : public MemorySaverInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  void SetUpOnMainThread() override {
    MemorySaverInteractiveTestMixin::SetUpOnMainThread();
    SetMemorySaverModeEnabled(true);
  }

  auto CheckMemorySaverModePrefState(MemorySaverModeState state) {
    return CheckResult(
        base::BindLambdaForTesting([]() {
          return performance_manager::user_tuning::prefs::
              GetCurrentMemorySaverModeState(g_browser_process->local_state());
        }),
        state);
  }

  auto CheckMemorySaverModeLogged(
      MemorySaverModeState state,
      int expected_count,
      const base::HistogramTester& histogram_tester) {
    return Do(base::BindLambdaForTesting([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.MemorySaver.SettingsChangeMode",
          static_cast<int>(state), expected_count);
    }));
  }
};

IN_PROC_BROWSER_TEST_F(MemorySettingsInteractiveTest, MemorySaverPrefChanged) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),

      // Turn Off Memory Saver Mode
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, false),
      CheckMemorySaverModePrefState(MemorySaverModeState::kDisabled),

      // Turn Memory Saver Mode back on
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),
      CheckMemorySaverModePrefState(MemorySaverModeState::kEnabled));
}

IN_PROC_BROWSER_TEST_F(MemorySettingsInteractiveTest,
                       MemorySaverLearnMoreLinkNavigates) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery memory_saver_learn_more = {
      "settings-ui",          "settings-main",          "settings-basic-page",
      "settings-memory-page", "settings-toggle-button", "a#learn-more"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      InstrumentNextTab(kLearnMorePage),
      ClickElement(kPerformanceSettingsPage, memory_saver_learn_more),
      WaitForShow(kLearnMorePage),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kMemorySaverModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(MemorySettingsInteractiveTest,
                       MemorySaverMetricsShouldLogOnToggle) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),

      // Turn Off Memory Saver Mode
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, false),
      CheckMemorySaverModeLogged(MemorySaverModeState::kDisabled, 1,
                                 histogram_tester),

      // Turn Memory Saver Mode back on
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),
      CheckMemorySaverModeLogged(MemorySaverModeState::kEnabled, 1,
                                 histogram_tester));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MemorySettingsInteractiveTest,
                       MemorySaverSendFeedbackDialogOpens) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      ClickElement(kPerformanceSettingsPage, kMemorySaverFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

#elif BUILDFLAG(IS_CHROMEOS_ASH)
class MemorySettingsCrosInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveAshTest> {};

IN_PROC_BROWSER_TEST_F(MemorySettingsCrosInteractiveTest,
                       MemorySaverSendFeedbackDialogOpens) {
  SetupContextWidget();
  InstallSystemApps();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackDialogElementId);
  CreateBrowserWindow(
      GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)));
  Browser* const browser = chrome::FindLastActive();
  ASSERT_NE(browser, nullptr);

  RunTestSequence(
      InContext(browser->window()->GetElementContext(),
                InstrumentTab(kPerformanceSettingsPage)),
      WaitForElementToRender(kPerformanceSettingsPage,
                             kMemorySaverFeedbackButton),
      InstrumentNextTab(kOsFeedbackDialogElementId, AnyBrowser()),
      ClickElement(kPerformanceSettingsPage, kMemorySaverFeedbackButton),
      WaitForShow(kOsFeedbackDialogElementId));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class MemorySaverAggressivenessSettingsInteractiveTest
    : public MemorySettingsInteractiveTest {
 public:
  auto CheckMemorySaverModeAggressivenessPrefState(
      MemorySaverModeAggressiveness aggressiveness) {
    return CheckResult(
        []() {
          return static_cast<MemorySaverModeAggressiveness>(
              g_browser_process->local_state()->GetInteger(
                  performance_manager::user_tuning::prefs::
                      kMemorySaverModeAggressiveness));
        },
        aggressiveness);
  }

  auto TestMemorySaverModeAggressivenessPrefState(
      const DeepQuery& element,
      MemorySaverModeAggressiveness aggressiveness) {
    return Steps(
        ScrollIntoView(kPerformanceSettingsPage, element),
        ClickElement(kPerformanceSettingsPage, element),
        WaitForButtonStateChange(kPerformanceSettingsPage, element, true),
        CheckMemorySaverModePrefState(MemorySaverModeState::kEnabled),
        CheckMemorySaverModeAggressivenessPrefState(aggressiveness));
  }

  auto CheckMemorySaverModeAggressivenessLogged(
      MemorySaverModeAggressiveness aggressiveness,
      int expected_count,
      const base::HistogramTester& histogram_tester) {
    return Do([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.MemorySaver.SettingsChangeAggressiveness",
          static_cast<int>(aggressiveness), expected_count);
    });
  }

  auto TestMemorySaverModeAggressivenessLogged(
      const DeepQuery& element,
      MemorySaverModeAggressiveness aggressiveness,
      const base::HistogramTester& histogram_tester) {
    return Steps(
        ScrollIntoView(kPerformanceSettingsPage, element),
        ClickElement(kPerformanceSettingsPage, element),
        WaitForButtonStateChange(kPerformanceSettingsPage, element, true),
        CheckMemorySaverModeAggressivenessLogged(aggressiveness, 1,
                                                 histogram_tester));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MemorySaverAggressivenessSettingsInteractiveTest,
                       MemorySaverPrefChanged) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),

      // Turn off memory saver mode
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, false),
      CheckMemorySaverModePrefState(MemorySaverModeState::kDisabled),

      // Turn memory saver mode back on
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),
      CheckMemorySaverModePrefState(MemorySaverModeState::kEnabled),

      // Test aggressiveness options
      WaitForElementToRender(kPerformanceSettingsPage, kMediumQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage, kMediumQuery, true),
      TestMemorySaverModeAggressivenessPrefState(
          kAggressiveQuery, MemorySaverModeAggressiveness::kAggressive),
      TestMemorySaverModeAggressivenessPrefState(
          kConservativeQuery, MemorySaverModeAggressiveness::kConservative),
      TestMemorySaverModeAggressivenessPrefState(
          kMediumQuery, MemorySaverModeAggressiveness::kMedium));
}

IN_PROC_BROWSER_TEST_F(MemorySaverAggressivenessSettingsInteractiveTest,
                       MemorySaverMetricsShouldLogOnToggle) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),

      // Turn Off Memory Saver Mode
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, false),
      CheckMemorySaverModeLogged(MemorySaverModeState::kDisabled, 1,
                                 histogram_tester),

      // Turn Memory Saver Mode back on
      ClickElement(kPerformanceSettingsPage, kMemorySaverToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kMemorySaverToggleQuery, true),
      CheckMemorySaverModeLogged(MemorySaverModeState::kEnabled, 1,
                                 histogram_tester),

      // Test aggressiveness options
      WaitForElementToRender(kPerformanceSettingsPage, kMediumQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage, kMediumQuery, true),
      TestMemorySaverModeAggressivenessLogged(
          kAggressiveQuery, MemorySaverModeAggressiveness::kAggressive,
          histogram_tester),
      TestMemorySaverModeAggressivenessLogged(
          kConservativeQuery, MemorySaverModeAggressiveness::kConservative,
          histogram_tester),
      TestMemorySaverModeAggressivenessLogged(
          kMediumQuery, MemorySaverModeAggressiveness::kMedium,
          histogram_tester));
}

#if !BUILDFLAG(IS_CHROMEOS)
class BatterySettingsInteractiveTest
    : public BatterySaverBrowserTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  base::BatteryLevelProvider::BatteryState GetFakeBatteryState() override {
    return base::test::TestBatteryLevelProvider::CreateBatteryState(1, true,
                                                                    100);
  }

  auto CheckBatteryStateLogged(const base::HistogramTester& histogram_tester,
                               BatterySaverModeState state,
                               int expected_count) {
    return Do(base::BindLambdaForTesting([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.BatterySaver.SettingsChangeMode",
          static_cast<int>(state), expected_count);
    }));
  }
};

IN_PROC_BROWSER_TEST_F(BatterySettingsInteractiveTest,
                       BatterySaverLearnMoreLink) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery battery_saver_learn_more = {
      "settings-ui",           "settings-main",          "settings-basic-page",
      "settings-battery-page", "settings-toggle-button", "a#learn-more"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      InstrumentNextTab(kLearnMorePage),
      ScrollIntoView(kPerformanceSettingsPage, battery_saver_learn_more),
      ClickElement(kPerformanceSettingsPage, battery_saver_learn_more),
      WaitForShow(kLearnMorePage),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kBatterySaverModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(BatterySettingsInteractiveTest,
                       BatterySaverMetricsShouldLogOnToggle) {
  const DeepQuery battery_saver_toggle = {
      "settings-ui",           "settings-main",          "settings-basic-page",
      "settings-battery-page", "settings-toggle-button", "cr-toggle#control"};

  const DeepQuery iron_collapse = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page", "cr-collapse#radioGroupCollapse"};

  const DeepQuery turn_on_at_threshold_button = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page", "controlled-radio-button"};

  const DeepQuery turn_on_when_unplugged_button = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page",
      "controlled-radio-button#enabledOnBatteryButton"};

  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage, battery_saver_toggle),
      WaitForButtonStateChange(kPerformanceSettingsPage, battery_saver_toggle,
                               true),
      ScrollIntoView(kPerformanceSettingsPage, turn_on_when_unplugged_button),

      // Turn off Battery Saver Mode
      ClickElement(kPerformanceSettingsPage, battery_saver_toggle),
      WaitForButtonStateChange(kPerformanceSettingsPage, battery_saver_toggle,
                               false),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kDisabled, 1),

      // Turn Battery Saver Mode back on
      ClickElement(kPerformanceSettingsPage, battery_saver_toggle),
      WaitForButtonStateChange(kPerformanceSettingsPage, battery_saver_toggle,
                               true),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kEnabledBelowThreshold, 1),

      // Wait for the cr-collapse animation to finish so that the battery
      // saver radio buttons will show on screen
      WaitForIronListCollapseStateChange(kPerformanceSettingsPage,
                                         iron_collapse),

      // Change Battery Saver Setting to turn on when unplugged
      ClickElement(kPerformanceSettingsPage, turn_on_when_unplugged_button),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               turn_on_when_unplugged_button, true),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kEnabledOnBattery, 1),

      // Change Battery Saver Setting to turn on when battery is at 20%
      ClickElement(kPerformanceSettingsPage, turn_on_at_threshold_button),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               turn_on_at_threshold_button, true),
      CheckBatteryStateLogged(
          histogram_tester, BatterySaverModeState::kEnabledBelowThreshold, 2));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(BatterySettingsInteractiveTest,
                       BatterySaverSendFeedbackDialogOpens) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      ClickElement(kPerformanceSettingsPage, kBatterySaverFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#elif BUILDFLAG(IS_CHROMEOS_ASH)
class BatterySettingsInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveAshTest> {
 public:
  BatterySettingsInteractiveTest()
      : scoped_feature_list_(ash::features::kBatterySaver) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        performance_manager::user_tuning::BatterySaverModeManager::
            kForceDeviceHasBatterySwitch);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BatterySettingsInteractiveTest,
                       BatterySaverSettingsLinksToOSSettings) {
  SetupContextWidget();
  InstallSystemApps();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsElementId);

  const DeepQuery battery_saver_link_row = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page", "cr-link-row#batterySaverOSSettingsLinkRow"};

  CreateBrowserWindow(
      GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)));
  Browser* const browser = chrome::FindLastActive();
  ASSERT_NE(browser, nullptr);

  RunTestSequence(
      InContext(browser->window()->GetElementContext(),
                InstrumentTab(kPerformanceSettingsPage)),
      WaitForElementToRender(kPerformanceSettingsPage, battery_saver_link_row),
      InstrumentNextTab(kOsSettingsElementId, AnyBrowser()),
      ClickElement(kPerformanceSettingsPage, battery_saver_link_row),
      WaitForShow(kOsSettingsElementId),
      WaitForWebContentsReady(kOsSettingsElementId,
                              GURL("chrome://os-settings/power")));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(BatterySettingsInteractiveTest,
                       BatterySaverSendFeedbackDialogOpens) {
  SetupContextWidget();
  InstallSystemApps();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackDialogElementId);
  CreateBrowserWindow(
      GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)));
  Browser* const browser = chrome::FindLastActive();
  ASSERT_NE(browser, nullptr);

  RunTestSequence(
      InContext(browser->window()->GetElementContext(),
                InstrumentTab(kPerformanceSettingsPage)),
      WaitForElementToRender(kPerformanceSettingsPage,
                             kBatterySaverFeedbackButton),
      InstrumentNextTab(kOsFeedbackDialogElementId, AnyBrowser()),
      ClickElement(kPerformanceSettingsPage, kBatterySaverFeedbackButton),
      WaitForShow(kOsFeedbackDialogElementId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class TabDiscardExceptionsSettingsInteractiveTest
    : public MemorySaverInteractiveTestMixin<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  auto WaitForElementToHide(const ui::ElementIdentifier& contents_id,
                            const DeepQuery& element) {
    StateChange element_renders;
    element_renders.event = kElementHides;
    element_renders.where = element;
    element_renders.test_function =
        "(el) => { let rect = el.getBoundingClientRect(); return rect.width "
        "=== 0 && rect.height === 0; }";

    return WaitForStateChange(contents_id, element_renders);
  }

  auto OpenAddExceptionDialog(const ui::ElementIdentifier& contents_id) {
    const WebContentsInteractionTestUtil::DeepQuery add_exceptions_button = {
        "settings-ui",
        "settings-main",
        "settings-basic-page",
        "settings-performance-page",
        "tab-discard-exception-list",
        "cr-button#addButton"};

    const WebContentsInteractionTestUtil::DeepQuery picker_dialog = {
        "settings-ui",
        "settings-main",
        "settings-basic-page",
        "settings-performance-page",
        "tab-discard-exception-list",
        "tab-discard-exception-tabbed-add-dialog"};

    const WebContentsInteractionTestUtil::DeepQuery tab_picker_tab = {
        "settings-ui",
        "settings-main",
        "settings-basic-page",
        "settings-performance-page",
        "tab-discard-exception-list",
        "tab-discard-exception-tabbed-add-dialog",
        "cr-tabs",
        "div.tab"};

    StateChange exceptions_dialog;
    exceptions_dialog.event = kExceptionDialogShows;
    exceptions_dialog.where = picker_dialog;
    return Steps(ClickElement(contents_id, add_exceptions_button),
                 WaitForStateChange(contents_id, exceptions_dialog),
                 ClickElement(contents_id, tab_picker_tab));
  }

  auto WaitForDisabledStateChange(const ui::ElementIdentifier& contents_id,
                                  const DeepQuery element,
                                  bool is_disabled) {
    StateChange toggle_selection_change;
    toggle_selection_change.event = kButtonWasClicked;
    toggle_selection_change.where = element;
    toggle_selection_change.type = StateChange::Type::kExistsAndConditionTrue;
    toggle_selection_change.test_function = base::StrCat(
        {"(el) => el.disabled === ", is_disabled ? "true" : "false"});
    return WaitForStateChange(contents_id, toggle_selection_change);
  }
};

IN_PROC_BROWSER_TEST_F(TabDiscardExceptionsSettingsInteractiveTest,
                       AddSiteToExceptionList) {
  const WebContentsInteractionTestUtil::DeepQuery exception_entry = {
      "settings-ui",
      "settings-main",
      "settings-basic-page",
      "settings-performance-page",
      "tab-discard-exception-list",
      "tab-discard-exception-entry"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForWebContentsReady(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      AddInstrumentedTab(kSecondTabContent, GetURL("example.com")),
      SelectTab(kTabStripElementId, 0), WaitForShow(kPerformanceSettingsPage),
      OpenAddExceptionDialog(kPerformanceSettingsPage),
      WaitForDisabledStateChange(kPerformanceSettingsPage,
                                 kExceptionDialogAddButton, true),
      ClickElement(kPerformanceSettingsPage, kExceptionDialogEntry),
      WaitForDisabledStateChange(kPerformanceSettingsPage,
                                 kExceptionDialogAddButton, false),
      ClickElement(kPerformanceSettingsPage, kExceptionDialogAddButton),
      WaitForElementToRender(kPerformanceSettingsPage, exception_entry));
}

// The high efficiency tab picker should live update when the user open or
// closes a tab that can be added to the exceptions list
IN_PROC_BROWSER_TEST_F(TabDiscardExceptionsSettingsInteractiveTest,
                       UpdatesEntryListLive) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      // Make sure there is no entry in the tab picker since there are no other
      // tabs open
      OpenAddExceptionDialog(kPerformanceSettingsPage),
      EnsureNotPresent(kPerformanceSettingsPage, kExceptionDialogEntry),

      // Dialog should show new entry when opening a new tab
      AddInstrumentedTab(kSecondTabContent, GetURL("example.com")),
      SelectTab(kTabStripElementId, 0), WaitForShow(kPerformanceSettingsPage),
      WaitForElementToRender(kPerformanceSettingsPage, kExceptionDialogEntry),

      // Dialog entry should hide when its corresponding tab is closed
      Do(base::BindLambdaForTesting([=, this]() {
        browser()->tab_strip_model()->CloseWebContentsAt(
            1, TabCloseTypes::CLOSE_NONE);
      })),
      WaitForElementToHide(kPerformanceSettingsPage, kExceptionDialogEntry));
}

// The high efficiency exceptions tab picker should only show sites that are
// non-chrome sites and have not been added to the exceptions list yet
IN_PROC_BROWSER_TEST_F(TabDiscardExceptionsSettingsInteractiveTest,
                       IgnoreIneligibleTabs) {
  SetTabDiscardExceptionsMap({"example.com"});

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      // Open a site that is already on the exclusion list
      AddInstrumentedTab(kSecondTabContent, GetURL("example.com")),
      SelectTab(kTabStripElementId, 0), WaitForShow(kPerformanceSettingsPage),

      // Verify entry not shown since this is an excluded site
      OpenAddExceptionDialog(kPerformanceSettingsPage),
      EnsureNotPresent(kPerformanceSettingsPage, kExceptionDialogEntry),

      // Verify entry shows when navigated to a non-excluded site
      NavigateWebContents(kSecondTabContent, GetURL("a.com")),
      WaitForElementToRender(kPerformanceSettingsPage, kExceptionDialogEntry),

      // Verify that the entry hides since the tab has navigated to a chrome
      // page
      NavigateWebContents(kSecondTabContent,
                          GURL(chrome::kChromeUINewTabPageURL)),
      WaitForElementToHide(kPerformanceSettingsPage, kExceptionDialogEntry));
}

class PerformanceInterventionSettingsInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {performance_manager::features::kPerformanceIntervention,
         performance_manager::features::kPerformanceInterventionUI},
        {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceInterventionSettingsInteractiveTest,
                       PerformanceInterventionMetricLogOnToggle) {
  base::HistogramTester histogram_tester;
  const WebContentsInteractionTestUtil::DeepQuery
      performance_intervention_setting = {
          "settings-ui",
          "settings-main",
          "settings-basic-page",
          "settings-performance-page",
          "settings-toggle-button#performanceInterventionToggleButton",
          "cr-toggle#control"};
  g_browser_process->local_state()->SetBoolean(
      performance_manager::user_tuning::prefs::kDiscardRingTreatmentEnabled,
      true);

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(
          kPerformanceSettingsPage,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))),
      WaitForElementToRender(kPerformanceSettingsPage,
                             performance_intervention_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               performance_intervention_setting, true),

      // Turn Off Performance Intervention notifications
      ClickElement(kPerformanceSettingsPage, performance_intervention_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               performance_intervention_setting, false),
      // Turn On performance Intervention notifications
      ClickElement(kPerformanceSettingsPage, performance_intervention_setting),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               performance_intervention_setting, true));
}
