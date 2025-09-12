// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
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

class SessionRestoreInfobarInteractiveTest : public InteractiveBrowserTest {
 public:
  SessionRestoreInfobarInteractiveTest() = default;
  ~SessionRestoreInfobarInteractiveTest() override = default;

  SessionRestoreInfobarInteractiveTest(
      const SessionRestoreInfobarInteractiveTest&) = delete;
  SessionRestoreInfobarInteractiveTest& operator=(
      const SessionRestoreInfobarInteractiveTest&) = delete;

 protected:
  void CreateInfobar(bool was_restarted, bool is_post_crash_launch) {
    auto controller = std::make_unique<SessionRestoreInfobarController>();
    controller->MaybeShowInfoBar(*browser()->profile(), was_restarted,
                                 is_post_crash_launch);
  }
};

// Test that the session restore infobar has no value set by the user and the
// untouched session restore preference shows the correct message.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest,
                       InfobarUntouchedSessionRestoreDefaultPref) {
  CreateInfobar(false, false);
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
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest,
                       InfobarShownForSessionRestore) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);

  CreateInfobar(false, false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar is not shown when the user's
// preferences are set to open the new tab page.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest,
                       InfobarNotShownForOtherSettings) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);

  CreateInfobar(false, false);
  RunTestSequence(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
}

// Test that the session restore infobar has the correct message value when the
// browser session is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest,
                       InfobarMessageValueForRestart) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  CreateInfobar(true, false);

  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId),
      CheckView(ConfirmInfoBar::kInfoBarElementId, [](ConfirmInfoBar* infobar) {
        return static_cast<SessionRestoreInfoBarDelegate*>(infobar->delegate())
                   ->GetMessageText() ==
               l10n_util::GetStringUTF16(
                   IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_RESTART);
      }));
}

// Test that the session restore infobar is global and appears on all tabs.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest, InfobarIsGlobal) {
  CreateInfobar(false, false);
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  // Check that the infobar is also shown on new tabs.
                  AddInstrumentedTab(kSecondTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  AddInstrumentedTab(kThirdTabContents, GURL("about:blank")),
                  WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

// Test that dismissing one infobar dismisses all other infobars.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarInteractiveTest,
                       DismissOneInfobarDismissesAll) {
  CreateInfobar(false, false);
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

}  // namespace session_restore_infobar
