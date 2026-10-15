// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_hats.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/metrics.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::DoAll;
using ::testing::ExplainMatchResult;
using ::testing::Mock;
using ::testing::PrintToString;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StringMatchResultListener;
using ::testing::UnorderedElementsAreArray;

namespace scheduled_restart {

namespace {

base::DictValue MakeValidPendingSurveyDict(
    base::Time timestamp = base::Time::Now()) {
  return base::DictValue()
      .Set(kHatsScheduledRestartPendingTimestampKey,
           base::TimeToValue(timestamp))
      .Set(kHatsScheduledRestartPendingIncognitoKey, false)
      .Set(kHatsScheduledRestartPendingStringDataKey,
           base::DictValue()
               .Set(kHatsScheduledRestartClientChannel, "stable")
               .Set(kHatsScheduledRestartTimePromptVisible, "10")
               .Set(kHatsScheduledRestartUserChoice, "restart now")
               .Set(kHatsScheduledRestartTimeSinceUpdate, "1")
               .Set(kHatsScheduledRestartTabsAndWindows, "1 tabs, 1 windows")
               .Set(kHatsScheduledRestartTriggerType, "automatic")
               .Set(kHatsScheduledRestartFirstNudgeDelay, "14")
               .Set(kHatsScheduledRestartNudgeCooldown, "14"));
}

}  // namespace

class ScheduledRestartHatsBrowserTest : public InProcessBrowserTest {
 public:
  ScheduledRestartHatsBrowserTest()
      : ScheduledRestartHatsBrowserTest(base::FieldTrialParams(),
                                        /*enable_hats=*/true) {}

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockHatsService));
  }

  void TearDownOnMainThread() override {
    ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
        base::RepeatingClosure());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void ExpectSurveyLaunch() {
    EXPECT_CALL(*mock_hats_service(),
                LaunchDelayedSurveyForWebContents(
                    kHatsSurveyTriggerScheduledRestart, _,
                    kHatsScheduledRestartSurveyDelay.InMilliseconds(), _, _,
                    HatsService::NavigationBehavior::ALLOW_ANY, _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&captured_bits_),
                        SaveArg<4>(&captured_strings_),
                        Return(HatsService::LaunchError::kNone)));
  }

  // Shows a bubble with a non-null `on_close` callback, matching how
  // `ScheduledRestartBubbleController` creates the bubble in production (which
  // registers the callback via `Widget::MakeCloseSynchronous()`).
  std::unique_ptr<views::Widget> ShowBubbleLikeController(
      BrowserWindowInterface* target_browser = nullptr,
      bool show_active = false,
      views::Widget::ClosedCallback on_close = base::DoNothing()) {
    return ScheduledRestartBubbleView::ShowBubble(
        target_browser ? target_browser : browser(), std::move(on_close),
        show_active);
  }

  [[nodiscard]] AssertionResult ShowAndDismissBubble(
      views::Widget::ClosedReason reason =
          views::Widget::ClosedReason::kCloseButtonClicked,
      bool show_active = false,
      BrowserWindowInterface* target_browser = nullptr) {
    bool on_close_called = false;
    auto widget = ShowBubbleLikeController(
        target_browser, show_active,
        base::BindLambdaForTesting(
            [&](views::Widget::ClosedReason) { on_close_called = true; }));
    if (!widget) {
      return AssertionFailure() << "Failed to show bubble widget";
    }
    widget->CloseWithReason(reason);
    if (!on_close_called) {
      return AssertionFailure() << "on_close callback was not invoked";
    }
    return AssertionSuccess();
  }

  [[nodiscard]] AssertionResult ShowAndAcceptBubble(
      BrowserWindowInterface* target_browser = nullptr,
      bool show_active = false) {
    bool relaunch_called = false;
    ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
        base::BindLambdaForTesting([&]() { relaunch_called = true; }));
    auto widget = ShowBubbleLikeController(target_browser, show_active);
    if (!widget) {
      return AssertionFailure() << "Failed to show bubble widget";
    }
    auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
    if (!dialog_delegate) {
      return AssertionFailure() << "Widget delegate is not a DialogDelegate";
    }
    dialog_delegate->AcceptDialog();
    if (!relaunch_called) {
      return AssertionFailure() << "relaunch callback was not invoked";
    }
    return AssertionSuccess();
  }

  [[nodiscard]] AssertionResult VerifyCapturedPsdMatchesConfig() {
    hats::SurveyConfigs configs;
    hats::GetActiveSurveyConfigs(configs);
    auto it = configs.find(kHatsSurveyTriggerScheduledRestart);
    if (it == configs.end()) {
      return AssertionFailure() << "No survey config found for trigger: "
                                << kHatsSurveyTriggerScheduledRestart;
    }

    std::vector<std::string> actual_bits;
    for (const auto& [key, val] : captured_bits_) {
      actual_bits.push_back(key);
    }
    std::vector<std::string> actual_strings;
    for (const auto& [key, val] : captured_strings_) {
      actual_strings.push_back(key);
    }

    StringMatchResultListener bits_listener;
    if (!ExplainMatchResult(UnorderedElementsAreArray(
                                it->second.product_specific_bits_data_fields),
                            actual_bits, &bits_listener)) {
      return AssertionFailure()
             << "PSD bits mismatch: " << PrintToString(actual_bits) << " "
             << bits_listener.str();
    }
    StringMatchResultListener strings_listener;
    if (!ExplainMatchResult(UnorderedElementsAreArray(
                                it->second.product_specific_string_data_fields),
                            actual_strings, &strings_listener)) {
      return AssertionFailure()
             << "PSD strings mismatch: " << PrintToString(actual_strings) << " "
             << strings_listener.str();
    }

    return AssertionSuccess();
  }

  const base::DictValue& pending_survey_dict() const {
    return g_browser_process->local_state()->GetDict(
        prefs::kScheduledRestartHatsPendingSurvey);
  }

  void SetPendingSurveyDict(base::DictValue dict) {
    g_browser_process->local_state()->SetDict(
        prefs::kScheduledRestartHatsPendingSurvey, std::move(dict));
  }

  const SurveyBitsData& captured_bits() const { return captured_bits_; }
  const SurveyStringData& captured_strings() const { return captured_strings_; }

 protected:
  ScheduledRestartHatsBrowserTest(const base::FieldTrialParams& nudge_params,
                                  bool enable_hats) {
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kScheduledRestart, nudge_params}};
    std::vector<base::test::FeatureRef> disabled;
    if (enable_hats) {
      enabled.push_back(
          {features::kHappinessTrackingSurveysForDesktopScheduledRestart, {}});
    } else {
      disabled.push_back(
          features::kHappinessTrackingSurveysForDesktopScheduledRestart);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  ScheduledRestartManager* manager() {
    return g_browser_process->GetFeatures()->scheduled_restart_manager();
  }

  MockHatsService* mock_hats_service() {
    return static_cast<MockHatsService*>(
        HatsServiceFactory::GetForProfile(browser()->GetProfile(),
                                          /*create_if_necessary=*/true));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  SurveyBitsData captured_bits_;
  SurveyStringData captured_strings_;
};

class ScheduledRestartHatsDisabledBrowserTest
    : public ScheduledRestartHatsBrowserTest {
 public:
  ScheduledRestartHatsDisabledBrowserTest()
      : ScheduledRestartHatsBrowserTest(base::FieldTrialParams(),
                                        /*enable_hats=*/false) {}
};

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsDisabledBrowserTest,
                       HatsSurveyNotTriggeredWhenFeatureDisabled) {
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  ASSERT_TRUE(ShowAndAcceptBubble());
  EXPECT_TRUE(pending_survey_dict().empty());

  SetPendingSurveyDict(MakeValidPendingSurveyDict());
  EXPECT_FALSE(pending_survey_dict().empty());

  MaybeLaunchPendingHatsSurvey(browser()->GetProfile());
  EXPECT_TRUE(pending_survey_dict().empty());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyTriggeredOnRestartNow) {
  ASSERT_TRUE(ShowAndAcceptBubble());

  const base::DictValue& pending = pending_survey_dict();
  EXPECT_FALSE(pending.empty());
  EXPECT_TRUE(pending.Find(kHatsScheduledRestartPendingTimestampKey));
  const base::DictValue* string_dict =
      pending.FindDict(kHatsScheduledRestartPendingStringDataKey);
  ASSERT_TRUE(string_dict);
  EXPECT_EQ(*string_dict->FindString(kHatsScheduledRestartUserChoice),
            "restart now");
  EXPECT_EQ(*string_dict->FindString(kHatsScheduledRestartTriggerType),
            "automatic");
  EXPECT_EQ(*string_dict->FindString(kHatsScheduledRestartFirstNudgeDelay),
            "14");
  EXPECT_EQ(*string_dict->FindString(kHatsScheduledRestartNudgeCooldown), "14");

  // Trigger a synchronous session restore to exercise the controller's
  // SessionRestore::RegisterOnSessionRestoredCallback subscription end-to-end.
  ExpectSurveyLaunch();
  SessionRestore::RestoreSession(
      browser()->GetProfile(), browser(),
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_BROWSER,
      StartupTabs());
  EXPECT_TRUE(pending_survey_dict().empty());

  ASSERT_TRUE(VerifyCapturedPsdMatchesConfig());
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartUserChoice),
            "restart now");
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartTriggerType),
            "automatic");
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyTriggeredOnScheduledOnIdle) {
  auto widget = ShowBubbleLikeController();
  ASSERT_TRUE(widget);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  auto* idle_button =
      views::AsViewClass<views::MdTextButton>(dialog_delegate->GetExtraView());
  ASSERT_TRUE(idle_button);

  ExpectSurveyLaunch();
  views::test::ButtonTestApi(idle_button)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
          ui::EventTimeForNow() + 2 * views::GetDoubleClickInterval(),
          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_TRUE(manager()->is_scheduled());
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartUserChoice),
            "restart when idle");
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyTriggeredOnDismiss) {
  for (auto reason : {views::Widget::ClosedReason::kCloseButtonClicked,
                      views::Widget::ClosedReason::kEscKeyPressed,
                      views::Widget::ClosedReason::kLostFocus}) {
    SCOPED_TRACE(static_cast<int>(reason));
    ExpectSurveyLaunch();
    ASSERT_TRUE(ShowAndDismissBubble(reason, /*show_active=*/true));
    ASSERT_TRUE(VerifyCapturedPsdMatchesConfig());
    EXPECT_EQ(captured_strings().at(kHatsScheduledRestartUserChoice),
              "dismiss");
    int duration_val = -1;
    EXPECT_TRUE(base::StringToInt(
        captured_strings().at(kHatsScheduledRestartTimePromptVisible),
        &duration_val));
    EXPECT_GE(duration_val, 0);
    EXPECT_FALSE(
        captured_strings().at(kHatsScheduledRestartClientChannel).empty());
    Mock::VerifyAndClearExpectations(mock_hats_service());
  }
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyNotTriggeredOnProgrammaticClose) {
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  ASSERT_TRUE(ShowAndDismissBubble(views::Widget::ClosedReason::kUnspecified));
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyNotTriggeredOnReplacement) {
  auto widget = ShowBubbleLikeController();
  ASSERT_TRUE(widget);

  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  ScheduledRestartBubbleView::DestroyReplacedBubble(std::move(widget));
  EXPECT_TRUE(pending_survey_dict().empty());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyNotTriggeredOnBrowserClose) {
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(second_browser);

  auto widget = ShowBubbleLikeController(second_browser);
  ASSERT_TRUE(widget);

  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  CloseBrowserSynchronously(second_browser);
  EXPECT_TRUE(pending_survey_dict().empty());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest, HatsSurveyTriggerType) {
  {
    SCOPED_TRACE("automatic");
    ExpectSurveyLaunch();
    ASSERT_TRUE(
        ShowAndDismissBubble(views::Widget::ClosedReason::kCloseButtonClicked,
                             /*show_active=*/false));
    EXPECT_EQ(captured_strings().at(kHatsScheduledRestartTriggerType),
              "automatic");
  }

  {
    SCOPED_TRACE("manual");
    ExpectSurveyLaunch();
    ASSERT_TRUE(
        ShowAndDismissBubble(views::Widget::ClosedReason::kCloseButtonClicked,
                             /*show_active=*/true));
    EXPECT_EQ(captured_strings().at(kHatsScheduledRestartTriggerType),
              "manual");
  }
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       PendingHatsSurveyExpired) {
  // 1. Survey older than 10 minutes is discarded.
  SetPendingSurveyDict(
      MakeValidPendingSurveyDict(base::Time::Now() - base::Minutes(11)));
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  MaybeLaunchPendingHatsSurvey(browser()->GetProfile());
  EXPECT_TRUE(pending_survey_dict().empty());

  // 2. Future timestamp (clock skew / clock moved backwards) is discarded.
  SetPendingSurveyDict(
      MakeValidPendingSurveyDict(base::Time::Now() + base::Minutes(5)));
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  MaybeLaunchPendingHatsSurvey(browser()->GetProfile());
  EXPECT_TRUE(pending_survey_dict().empty());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       PendingHatsSurveyMalformedMissingKey) {
  base::DictValue pending = MakeValidPendingSurveyDict();
  // Missing an expected string data key causes the entry to be discarded.
  pending.EnsureDict(kHatsScheduledRestartPendingStringDataKey)
      ->Remove(kHatsScheduledRestartUserChoice);
  SetPendingSurveyDict(std::move(pending));

  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  MaybeLaunchPendingHatsSurvey(browser()->GetProfile());
  EXPECT_TRUE(pending_survey_dict().empty());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       PendingHatsSurveyIgnoresExtraUnknownKey) {
  base::DictValue pending = MakeValidPendingSurveyDict();
  // An extra unknown key (e.g. from an older or newer binary) is safely
  // ignored.
  pending.EnsureDict(kHatsScheduledRestartPendingStringDataKey)
      ->Set("unknown_future_field", "extra");
  SetPendingSurveyDict(std::move(pending));

  ExpectSurveyLaunch();
  MaybeLaunchPendingHatsSurvey(browser()->GetProfile());
  EXPECT_TRUE(pending_survey_dict().empty());
  ASSERT_TRUE(VerifyCapturedPsdMatchesConfig());
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartUserChoice),
            "restart now");
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyNotTriggeredInIncognito) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser);

  // 1. Dismissing the bubble in an incognito window does not launch a survey.
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  ASSERT_TRUE(
      ShowAndDismissBubble(views::Widget::ClosedReason::kCloseButtonClicked,
                           /*show_active=*/true, incognito_browser));

  // 2. Clicking "Restart now" in an incognito window neither launches a survey
  // nor persists a pending survey to LocalState.
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurveyForWebContents).Times(0);
  EXPECT_CALL(*mock_hats_service(), LaunchDelayedSurvey).Times(0);
  ASSERT_TRUE(ShowAndAcceptBubble(incognito_browser, /*show_active=*/true));
  EXPECT_TRUE(pending_survey_dict().empty());

  CloseBrowserSynchronously(incognito_browser);
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyIncognitoAndGuestPsd) {
  {
    SCOPED_TRACE("Case 1: No incognito windows open");
    ExpectSurveyLaunch();
    ASSERT_TRUE(ShowAndDismissBubble());
    EXPECT_FALSE(captured_bits().at(kHatsScheduledRestartIncognitoPresentBit));
  }

  {
    SCOPED_TRACE("Case 2: Incognito window open");
    BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
    ASSERT_TRUE(incognito_browser);
    ExpectSurveyLaunch();
    ASSERT_TRUE(ShowAndDismissBubble());
    EXPECT_TRUE(captured_bits().at(kHatsScheduledRestartIncognitoPresentBit));
    CloseBrowserSynchronously(incognito_browser);
  }

  {
    SCOPED_TRACE("Case 3: Guest window open");
    BrowserWindowInterface* guest_browser = CreateGuestBrowser();
    ASSERT_TRUE(guest_browser);
    ExpectSurveyLaunch();
    ASSERT_TRUE(ShowAndDismissBubble());
    EXPECT_FALSE(captured_bits().at(kHatsScheduledRestartIncognitoPresentBit));
    CloseBrowserSynchronously(guest_browser);
  }
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsBrowserTest,
                       HatsSurveyOpenTabsAndWindowsCount) {
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(second_browser);

  ExpectSurveyLaunch();
  ASSERT_TRUE(ShowAndDismissBubble());
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartTabsAndWindows),
            "3 tabs, 2 windows");

  CloseBrowserSynchronously(second_browser);
}

class ScheduledRestartHatsNudgeParamsBrowserTest
    : public ScheduledRestartHatsBrowserTest {
 public:
  ScheduledRestartHatsNudgeParamsBrowserTest()
      : ScheduledRestartHatsBrowserTest(
            {{"first_nudge_delay", "7d"}, {"nudge_cooldown", "7d"}},
            /*enable_hats=*/true) {}
};

IN_PROC_BROWSER_TEST_F(ScheduledRestartHatsNudgeParamsBrowserTest,
                       HatsSurveyReportsNudgeParameters) {
  ExpectSurveyLaunch();
  ASSERT_TRUE(ShowAndDismissBubble());
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartFirstNudgeDelay), "7");
  EXPECT_EQ(captured_strings().at(kHatsScheduledRestartNudgeCooldown), "7");
}

}  // namespace scheduled_restart
