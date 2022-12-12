// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_state_sampler.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using performance_manager::user_tuning::prefs::BatterySaverModeState;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsPage);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonWasClicked);

constexpr char kCheckJsElementIsChecked[] = "(el) => { return el.checked; }";
constexpr char kCheckJsElementIsNotChecked[] =
    "(el) => { return !el.checked; }";

class PerformanceSettingsInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{performance_manager::features::kHighEfficiencyModeAvailable,
          {{"default_state", "true"}, {"time_before_discard", "30s"}}},
         {performance_manager::features::kBatterySaverModeAvailable, {}}},
        {});

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SetUpFakeBatterySampler();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  void SetUpFakeBatterySampler() {
    auto test_sampling_event_source =
        std::make_unique<base::test::TestSamplingEventSource>();
    auto test_battery_level_provider =
        std::make_unique<base::test::TestBatteryLevelProvider>();

    sampling_source_ = test_sampling_event_source.get();
    battery_level_provider_ = test_battery_level_provider.get();
    test_battery_level_provider->SetBatteryState(
        base::test::TestBatteryLevelProvider::CreateBatteryState(1, true, 100));

    battery_state_sampler_ =
        base::BatteryStateSampler::CreateInstanceForTesting(
            std::move(test_sampling_event_source),
            std::move(test_battery_level_provider));
  }

  auto CheckTabCount(int expected_tab_count) {
    auto get_tab_count = base::BindLambdaForTesting(
        [this]() { return browser()->tab_strip_model()->GetTabCount(); });

    return CheckResult(get_tab_count, expected_tab_count);
  }

  auto CheckHighEffiencyModeLogged(
      bool high_efficiency_enabled,
      int expected_count,
      const base::HistogramTester& histogram_tester) {
    return Do(base::BindLambdaForTesting([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.HighEfficiency.SettingsChangeMode",
          high_efficiency_enabled, expected_count);
    }));
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

  auto WaitForButtonStateChange(DeepQuery element, bool is_checked) {
    StateChange toggle_selection_change;
    toggle_selection_change.event = kButtonWasClicked;
    toggle_selection_change.where = element;
    toggle_selection_change.type = StateChange::Type::kExistsAndConditionTrue;
    toggle_selection_change.test_function =
        is_checked ? kCheckJsElementIsChecked : kCheckJsElementIsNotChecked;

    return WaitForStateChange(kPerformanceSettingsPage,
                              std::move(toggle_selection_change));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<base::test::TestSamplingEventSource> sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider> battery_level_provider_;
  std::unique_ptr<base::BatteryStateSampler> battery_state_sampler_;
};

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencyLearnMoreLinkNavigates) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery high_efficiency_learn_more = {"settings-ui",
                                                "settings-main",
                                                "settings-basic-page",
                                                "settings-performance-page",
                                                "settings-toggle-button",
                                                "a#highEfficiencyLearnMore"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      InstrumentNextTab(kLearnMorePage),
      MoveMouseTo(kPerformanceSettingsPage, high_efficiency_learn_more),
      ClickMouse(), WaitForShow(kLearnMorePage), CheckTabCount(2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kHighEfficiencyModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       BatterySaverLearnMoreLink) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery battery_saver_learn_more = {"settings-ui",
                                              "settings-main",
                                              "settings-basic-page",
                                              "settings-battery-page",
                                              "settings-toggle-button",
                                              "a#batterySaverLearnMore"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      InstrumentNextTab(kLearnMorePage),
      MoveMouseTo(kPerformanceSettingsPage, battery_saver_learn_more),
      ClickMouse(), WaitForShow(kLearnMorePage), CheckTabCount(2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kBatterySaverModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencyMetricsShouldLogOnToggle) {
  const DeepQuery high_efficiency_toggle = {"settings-ui",
                                            "settings-main",
                                            "settings-basic-page",
                                            "settings-performance-page",
                                            "settings-toggle-button",
                                            "cr-toggle#control"};

  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      CheckJsResultAt(kPerformanceSettingsPage, high_efficiency_toggle,
                      kCheckJsElementIsChecked),

      // Turn Off High Efficiency Mode
      MoveMouseTo(kPerformanceSettingsPage, high_efficiency_toggle),
      ClickMouse(), WaitForButtonStateChange(high_efficiency_toggle, false),
      CheckHighEffiencyModeLogged(false, 1, histogram_tester),

      // Turn High Efficiency Mode back on
      MoveMouseTo(kPerformanceSettingsPage, high_efficiency_toggle),
      ClickMouse(), WaitForButtonStateChange(high_efficiency_toggle, true),
      CheckHighEffiencyModeLogged(true, 1, histogram_tester));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       BatterySaverMetricsShouldLogOnToggle) {
  const DeepQuery battery_saver_toggle = {
      "settings-ui",           "settings-main",          "settings-basic-page",
      "settings-battery-page", "settings-toggle-button", "cr-toggle#control"};

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
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      CheckJsResultAt(kPerformanceSettingsPage, battery_saver_toggle,
                      kCheckJsElementIsChecked),

      // Turn off Battery Saver Mode
      MoveMouseTo(kPerformanceSettingsPage, battery_saver_toggle), ClickMouse(),
      WaitForButtonStateChange(battery_saver_toggle, false),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kDisabled, 1),

      // Turn Battery Saver Mode back on
      MoveMouseTo(kPerformanceSettingsPage, battery_saver_toggle), ClickMouse(),
      WaitForButtonStateChange(battery_saver_toggle, true),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kEnabledBelowThreshold, 1),

      // Change Battery Saver Setting to turn on when unplugged
      MoveMouseTo(kPerformanceSettingsPage, turn_on_when_unplugged_button),
      ClickMouse(),
      WaitForButtonStateChange(turn_on_when_unplugged_button, true),
      CheckBatteryStateLogged(histogram_tester,
                              BatterySaverModeState::kEnabledOnBattery, 1),

      // Change Battery Saver Setting to turn on when battery is at 20%
      MoveMouseTo(kPerformanceSettingsPage, turn_on_at_threshold_button),
      ClickMouse(), WaitForButtonStateChange(turn_on_at_threshold_button, true),
      CheckBatteryStateLogged(
          histogram_tester, BatterySaverModeState::kEnabledBelowThreshold, 2));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencySendFeedbackDialogOpens) {
  const DeepQuery high_efficiency_feedback = {"settings-ui",
                                              "settings-main",
                                              "settings-basic-page",
                                              "settings-performance-page",
                                              "settings-toggle-button",
                                              "a#highEfficiencySendFeedback"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      MoveMouseTo(kPerformanceSettingsPage, high_efficiency_feedback),
      ClickMouse(),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       BatterySaverSendFeedbackDialogOpens) {
  const DeepQuery battery_saver_feedback = {"settings-ui",
                                            "settings-main",
                                            "settings-basic-page",
                                            "settings-battery-page",
                                            "settings-toggle-button",
                                            "a#batterySaverSendFeedback"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      MoveMouseTo(kPerformanceSettingsPage, battery_saver_feedback),
      ClickMouse(),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace
