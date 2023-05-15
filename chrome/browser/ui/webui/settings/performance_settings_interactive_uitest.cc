// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_state_sampler.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using performance_manager::user_tuning::prefs::BatterySaverModeState;
using performance_manager::user_tuning::prefs::HighEfficiencyModeState;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPerformanceSettingsPage);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonWasClicked);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRenders);

constexpr char kCheckJsElementIsChecked[] = "(el) => { return el.checked; }";
constexpr char kCheckJsElementIsNotChecked[] =
    "(el) => { return !el.checked; }";

const WebContentsInteractionTestUtil::DeepQuery kHighEfficiencyToggleQuery = {
    "settings-ui",
    "settings-main",
    "settings-basic-page",
    "settings-performance-page",
    "settings-toggle-button",
    "cr-toggle#control"};

class PerformanceSettingsInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SetUpFakeBatterySampler();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(true);
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

  auto ClickElement(const ui::ElementIdentifier& contents_id,
                    const DeepQuery& element) {
    return Steps(MoveMouseTo(contents_id, element), ClickMouse());
  }

  auto CheckTabCount(int expected_tab_count) {
    auto get_tab_count = base::BindLambdaForTesting(
        [this]() { return browser()->tab_strip_model()->GetTabCount(); });

    return CheckResult(get_tab_count, expected_tab_count);
  }

  auto CheckHighEfficiencyModePrefState(HighEfficiencyModeState state) {
    return CheckResult(base::BindLambdaForTesting([]() {
                         return performance_manager::user_tuning::prefs::
                             GetCurrentHighEfficiencyModeState(
                                 g_browser_process->local_state());
                       }),
                       state);
  }

  auto CheckHighEfficiencyModeLogged(
      HighEfficiencyModeState state,
      int expected_count,
      const base::HistogramTester& histogram_tester) {
    return Do(base::BindLambdaForTesting([=, &histogram_tester]() {
      histogram_tester.ExpectBucketCount(
          "PerformanceControls.HighEfficiency.SettingsChangeMode2",
          static_cast<int>(state), expected_count);
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

  auto WaitForButtonStateChange(const ui::ElementIdentifier& contents_id,
                                DeepQuery element,
                                bool is_checked) {
    StateChange toggle_selection_change;
    toggle_selection_change.event = kButtonWasClicked;
    toggle_selection_change.where = element;
    toggle_selection_change.type = StateChange::Type::kExistsAndConditionTrue;
    toggle_selection_change.test_function =
        is_checked ? kCheckJsElementIsChecked : kCheckJsElementIsNotChecked;

    return WaitForStateChange(contents_id, toggle_selection_change);
  }

  auto WaitForElementToRender(const ui::ElementIdentifier& contents_id,
                              const DeepQuery& element) {
    StateChange element_renders;
    element_renders.event = kElementRenders;
    element_renders.where = element;
    element_renders.type = StateChange::Type::kExistsAndConditionTrue;
    element_renders.test_function =
        "(el) => { return el.clientWidth > 0 && el.clientHeight > 0; }";

    return WaitForStateChange(contents_id, element_renders);
  }

 private:
  raw_ptr<base::test::TestSamplingEventSource, DanglingUntriaged>
      sampling_source_;
  raw_ptr<base::test::TestBatteryLevelProvider, DanglingUntriaged>
      battery_level_provider_;
  std::unique_ptr<base::BatteryStateSampler> battery_state_sampler_;
};

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencyPrefChanged) {
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      WaitForElementToRender(kPerformanceSettingsPage,
                             kHighEfficiencyToggleQuery),
      CheckJsResultAt(kPerformanceSettingsPage, kHighEfficiencyToggleQuery,
                      kCheckJsElementIsChecked),

      // Turn Off High Efficiency Mode
      ClickElement(kPerformanceSettingsPage, kHighEfficiencyToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kHighEfficiencyToggleQuery, false),
      CheckHighEfficiencyModePrefState(HighEfficiencyModeState::kDisabled),

      // Turn High Efficiency Mode back on
      ClickElement(kPerformanceSettingsPage, kHighEfficiencyToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kHighEfficiencyToggleQuery, true),
      CheckHighEfficiencyModePrefState(
          HighEfficiencyModeState::kEnabledOnTimer));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencyLearnMoreLinkNavigates) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery high_efficiency_learn_more = {"settings-ui",
                                                "settings-main",
                                                "settings-basic-page",
                                                "settings-performance-page",
                                                "settings-toggle-button",
                                                "a#learn-more"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      InstrumentNextTab(kLearnMorePage),
      ClickElement(kPerformanceSettingsPage, high_efficiency_learn_more),
      WaitForShow(kLearnMorePage), CheckTabCount(2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kHighEfficiencyModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       BatterySaverLearnMoreLink) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kLearnMorePage);
  const DeepQuery battery_saver_learn_more = {
      "settings-ui",           "settings-main",          "settings-basic-page",
      "settings-battery-page", "settings-toggle-button", "a#learn-more"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      InstrumentNextTab(kLearnMorePage),
      ClickElement(kPerformanceSettingsPage, battery_saver_learn_more),
      WaitForShow(kLearnMorePage), CheckTabCount(2),
      WaitForWebContentsReady(kLearnMorePage,
                              GURL(chrome::kBatterySaverModeLearnMoreUrl)));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       HighEfficiencyMetricsShouldLogOnToggle) {
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      WaitForElementToRender(kPerformanceSettingsPage,
                             kHighEfficiencyToggleQuery),
      CheckJsResultAt(kPerformanceSettingsPage, kHighEfficiencyToggleQuery,
                      kCheckJsElementIsChecked),

      // Turn Off High Efficiency Mode
      ClickElement(kPerformanceSettingsPage, kHighEfficiencyToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kHighEfficiencyToggleQuery, false),
      CheckHighEfficiencyModeLogged(HighEfficiencyModeState::kDisabled, 1,
                                    histogram_tester),

      // Turn High Efficiency Mode back on
      ClickElement(kPerformanceSettingsPage, kHighEfficiencyToggleQuery),
      WaitForButtonStateChange(kPerformanceSettingsPage,
                               kHighEfficiencyToggleQuery, true),
      CheckHighEfficiencyModeLogged(HighEfficiencyModeState::kEnabledOnTimer, 1,
                                    histogram_tester));
}

IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       BatterySaverMetricsShouldLogOnToggle) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kIronCollapseContentShows);

  const DeepQuery battery_saver_toggle = {
      "settings-ui",           "settings-main",          "settings-basic-page",
      "settings-battery-page", "settings-toggle-button", "cr-toggle#control"};

  const DeepQuery iron_collapse = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page", "iron-collapse#radioGroupCollapse"};

  const DeepQuery turn_on_at_threshold_button = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page", "controlled-radio-button"};

  const DeepQuery turn_on_when_unplugged_button = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-battery-page",
      "controlled-radio-button#enabledOnBatteryButton"};

  StateChange iron_collapse_finish_animating;
  iron_collapse_finish_animating.event = kIronCollapseContentShows;
  iron_collapse_finish_animating.where = iron_collapse;
  iron_collapse_finish_animating.type =
      StateChange::Type::kExistsAndConditionTrue;
  iron_collapse_finish_animating.test_function =
      "(el) => { return !el.transitioning; }";

  base::HistogramTester histogram_tester;
  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      WaitForElementToRender(kPerformanceSettingsPage, battery_saver_toggle),
      CheckJsResultAt(kPerformanceSettingsPage, battery_saver_toggle,
                      kCheckJsElementIsChecked),

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

      // Wait for the iron-collapse animation to finish so that the battery
      // saver radio buttons will show on screen
      WaitForStateChange(kPerformanceSettingsPage,
                         iron_collapse_finish_animating),

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
// TODO(http://b/281528238): reenable the test.
IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       DISABLED_HighEfficiencySendFeedbackDialogOpens) {
  const DeepQuery high_efficiency_feedback = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-section#performanceSettingsSection", "cr-icon-button#feedback"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      ClickElement(kPerformanceSettingsPage, high_efficiency_feedback),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

// TODO(http://b/281528238): reenable the test.
IN_PROC_BROWSER_TEST_F(PerformanceSettingsInteractiveTest,
                       DISABLED_BatterySaverSendFeedbackDialogOpens) {
  const DeepQuery battery_saver_feedback = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-section#batterySettingsSection", "cr-icon-button#feedback"};

  RunTestSequence(
      InstrumentTab(kPerformanceSettingsPage),
      NavigateWebContents(kPerformanceSettingsPage,
                          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      ClickElement(kPerformanceSettingsPage, battery_saver_feedback),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace
