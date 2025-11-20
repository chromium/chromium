// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace session_restore_infobar {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTabContents);
}  // namespace

class SessionRestoreInfobarInteractiveTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SessionRestoreInfobarInteractiveTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSessionRestoreInfobar,
          {{features::kSetDefaultToContinueSession.name,
            IsDefaultContinueSession() ? "true" : "false"}}}},
        {});
  }

  ~SessionRestoreInfobarInteractiveTest() override = default;

  SessionRestoreInfobarInteractiveTest(
      const SessionRestoreInfobarInteractiveTest&) = delete;
  SessionRestoreInfobarInteractiveTest& operator=(
      const SessionRestoreInfobarInteractiveTest&) = delete;

 protected:
  bool IsDefaultContinueSession() const { return GetParam(); }

  void CreateInfobar(Browser* browser, bool is_post_crash_launch) {
    auto* controller =
        session_restore_infobar::SessionRestoreInfobarController::From(browser);
    controller->MaybeShowInfoBar(*browser->profile(), is_post_crash_launch);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SessionRestoreInfobarDefaultTest : public InteractiveBrowserTest {
 public:
  SessionRestoreInfobarDefaultTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSessionRestoreInfobar,
          {{features::kSetDefaultToContinueSession.name, "false"}}}},
        {});
  }
  ~SessionRestoreInfobarDefaultTest() override = default;
  SessionRestoreInfobarDefaultTest(const SessionRestoreInfobarDefaultTest&) =
      delete;
  SessionRestoreInfobarDefaultTest& operator=(
      const SessionRestoreInfobarDefaultTest&) = delete;

 protected:
  void CreateInfobar(Browser* browser, bool is_post_crash_launch) {
    auto* controller =
        session_restore_infobar::SessionRestoreInfobarController::From(browser);
    controller->MaybeShowInfoBar(*browser->profile(), is_post_crash_launch);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SessionRestoreInfobarDefaultOffTest : public InteractiveBrowserTest {
 public:
  SessionRestoreInfobarDefaultOffTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSessionRestoreInfobar,
          {{features::kSetDefaultToContinueSession.name, "true"}}}},
        {});
  }
  ~SessionRestoreInfobarDefaultOffTest() override = default;
  SessionRestoreInfobarDefaultOffTest(
      const SessionRestoreInfobarDefaultOffTest&) = delete;
  SessionRestoreInfobarDefaultOffTest& operator=(
      const SessionRestoreInfobarDefaultOffTest&) = delete;

 protected:
  void CreateInfobar(Browser* browser, bool is_post_crash_launch) {
    auto* controller =
        session_restore_infobar::SessionRestoreInfobarController::From(browser);
    controller->MaybeShowInfoBar(*browser->profile(), is_post_crash_launch);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that the session restore infobar has the correct message value when the
// browser session is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarDefaultOffTest,
                       InfobarMessageValueForNewSession) {
  CreateInfobar(browser(), false);

  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      CheckView(ConfirmInfoBar::kInfoBarElementId, [](ConfirmInfoBar* infobar) {
        return static_cast<SessionRestoreInfoBarDelegate*>(infobar->delegate())
                   ->GetMessageText() ==
               l10n_util::GetStringUTF16(
                   IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_RESTART);
      }));
}

// Test that the session restore infobar has no value set by the user and the
// untouched session restore preference shows the correct message.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfobarUntouchedSessionRestoreDefaultPref) {

  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is shown on a new session when the
// session restore preference is at its default value.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarDefaultTest,
                       InfoBarShownOnNewSessionWithDefaultPref) {
  CreateInfobar(browser(), false);
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      CheckView(ConfirmInfoBar::kInfoBarElementId, [](ConfirmInfoBar* infobar) {
        return static_cast<SessionRestoreInfoBarDelegate*>(infobar->delegate())
                   ->GetMessageText() ==
               l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_TURN_ON);
      }));
}

// Test that the session restore infobar is shown when the user's preferences
// are set to continue where they left off.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfobarShownForSessionRestore) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is not shown when the user's
// preferences are set to open the new tab page.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfobarNotShownForOtherSettings) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);

  CreateInfobar(browser(), false);
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is not shown when clear on exit is
// enabled.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfoBarNotShownWhenClearOnExit) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);

  CreateInfobar(browser(), false);
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}


// Test that the session restore infobar has the correct message value when the
// browser session is restored and the session restore preference is at its
// default value.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarDefaultTest,
                       InfobarMessageValueForNewSessionWithDefaultPref) {
  CreateInfobar(browser(), false);

  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      CheckView(ConfirmInfoBar::kInfoBarElementId, [](ConfirmInfoBar* infobar) {
        return static_cast<SessionRestoreInfoBarDelegate*>(infobar->delegate())
                   ->GetMessageText() ==
               l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_TURN_ON);
      }));
}

// Test that the session restore infobar is global and appears on all tabs.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest, InfobarIsGlobal) {
  CreateInfobar(browser(), false);

  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  // Check that the infobar is also shown on new tabs.
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kThirdTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that dismissing one infobar dismisses all other infobars.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       DismissOneInfobarDismissesAll) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kThirdTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),

                  // Dismiss on the third tab.
                  SelectTab(kTabStripElementId, 2),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId),

                  // Check it's gone from the second tab.
                  SelectTab(kTabStripElementId, 1),
                  EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId),

                  // Check it's gone from the first tab.
                  SelectTab(kTabStripElementId, 0),
                  EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is dismissed when the pref is changed.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfobarDismissedOnPrefChange) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  // Change the pref to open the new tab page.
                  Do([this]() {
                    browser()->profile()->GetPrefs()->SetInteger(
                        prefs::kRestoreOnStartup, 4);
                  }),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is shown once on a new session and
// not again when dismissed.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfoBarShownOnceOnNewSessionWithContinueSessionDismiss) {
  // The infobar should be shown on the first session.
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));

  // The infobar should not be shown on the second session.
  CreateInfobar(browser(), false);
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is shown on the first new session.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       PRE_PRE_PRE_InfoBarShownThreeTimesOnNewSession) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is shown on the second new session.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       PRE_PRE_InfoBarShownThreeTimesOnNewSession) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is shown on the third new session.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       PRE_InfoBarShownThreeTimesOnNewSession) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is not shown on the fourth new
// session.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfoBarShownThreeTimesOnNewSession) {
  CreateInfobar(browser(), false);
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is not shown on a new tab after the
// pref is changed.
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest,
                       InfobarNotShownOnNewTabAfterPrefChange) {
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  // Change the pref to continue where you left off.
                  Do([this]() {
                    browser()->profile()->GetPrefs()->SetInteger(
                        prefs::kRestoreOnStartup, 1);
                  }),
                  // The infobar should be hidden after the pref change.
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                  // Open a new tab.
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  // Ensure the infobar is not present on the new tab.
                  EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}
IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest, MultipleMetrics) {
  base::HistogramTester histogram_tester;
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),

                  // Open 2 more tabs.
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kThirdTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),

                  // Dismiss the infobar
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),

                  // The infobars should be hidden.
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));

  const std::string histogram_name =
      IsDefaultContinueSession()
          ? "SessionRestore.InfoBar.TurnOffFromRestart"
          : "SessionRestore.InfoBar.TurnOnSessionRestore";

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kShown, 1);

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kDismissed,
      1);

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kIgnored,
      0);
}

IN_PROC_BROWSER_TEST_P(SessionRestoreInfobarInteractiveTest, MetricsIgnored) {
  base::HistogramTester histogram_tester;
  CreateInfobar(browser(), false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),

                  // Open 2 more tabs.
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kThirdTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId));

  CloseBrowserSynchronously(browser());

  const std::string histogram_name =
      IsDefaultContinueSession()
          ? "SessionRestore.InfoBar.TurnOffFromRestart"
          : "SessionRestore.InfoBar.TurnOnSessionRestore";

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kShown, 1);

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kDismissed,
      0);

  histogram_tester.ExpectBucketCount(
      histogram_name, SessionRestoreInfoBarDelegate::InfobarAction::kIgnored,
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SessionRestoreInfobarInteractiveTest,
                         testing::Bool());

}  // namespace session_restore_infobar
