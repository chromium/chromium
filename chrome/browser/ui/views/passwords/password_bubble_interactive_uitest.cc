// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/focus_changed_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/focus/focus_manager.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using testing::_;
using testing::Eq;
using testing::Field;

namespace {

const char kDisplayDispositionMetric[] = "PasswordBubble.DisplayDisposition";

bool IsBubbleShowing() {
  return PasswordBubbleViewBase::manage_password_bubble() &&
         PasswordBubbleViewBase::manage_password_bubble()
             ->GetWidget()
             ->IsVisible();
}

views::EditableCombobox* GetUsernameDropdown(
    const PasswordBubbleViewBase* bubble) {
  const PasswordSaveUpdateView* save_bubble =
      static_cast<const PasswordSaveUpdateView*>(bubble);
  return save_bubble->username_dropdown_for_testing();
}

}  // namespace

namespace metrics_util = password_manager::metrics_util;

class PasswordBubbleInteractiveUiTest : public ManagePasswordsTest {
 public:
  PasswordBubbleInteractiveUiTest() {}

  PasswordBubbleInteractiveUiTest(const PasswordBubbleInteractiveUiTest&) =
      delete;
  PasswordBubbleInteractiveUiTest& operator=(
      const PasswordBubbleInteractiveUiTest&) = delete;

  ~PasswordBubbleInteractiveUiTest() override {}
};

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, BasicOpenAndClose) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_FALSE(IsBubbleShowing());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  const PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  EXPECT_FALSE(bubble->GetFocusManager()->GetFocusedView());
  PasswordBubbleViewBase::CloseCurrentBubble();
  EXPECT_FALSE(IsBubbleShowing());
  // Drain message pump to ensure the bubble view is cleared so that it can be
  // created again (it is checked on Mac to prevent re-opening the bubble when
  // clicking the location bar button repeatedly).
  content::RunAllPendingInMessageLoop();

  // And, just for grins, ensure that we can re-open the bubble.
  TabDialogs::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->ShowManagePasswordsBubble(true /* user_action */);
  EXPECT_TRUE(IsBubbleShowing());
  bubble = PasswordBubbleViewBase::manage_password_bubble();
  // A pending password with empty username should initially focus on the
  // username field.
  EXPECT_TRUE(GetUsernameDropdown(bubble)->Contains(
      bubble->GetFocusManager()->GetFocusedView()));
  PasswordBubbleViewBase::CloseCurrentBubble();
  EXPECT_FALSE(IsBubbleShowing());
}

// Same as 'BasicOpenAndClose', but use the command rather than the static
// method directly.
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, CommandControlsBubble) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // The command only works if the icon is visible, so get into management mode.
  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());
  const LocationBarBubbleDelegateView* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  EXPECT_TRUE(bubble->GetOkButton());
  EXPECT_EQ(bubble->GetOkButton(), bubble->GetFocusManager()->GetFocusedView());
  PasswordBubbleViewBase::CloseCurrentBubble();
  EXPECT_FALSE(IsBubbleShowing());
  // Drain message pump to ensure the bubble view is cleared so that it can be
  // created again (it is checked on Mac to prevent re-opening the bubble when
  // clicking the location bar button repeatedly).
  content::RunAllPendingInMessageLoop();

  // And, just for grins, ensure that we can re-open the bubble.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::CloseCurrentBubble();
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       CommandExecutionInManagingState) {
  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());

  std::unique_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(0,
            samples->GetCount(metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1, samples->GetCount(metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       CommandExecutionInAutomaticState) {
  // Open with pending password: automagical!
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());

  // Bubble should not be focused by default.
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble()
                   ->GetFocusManager()
                   ->GetFocusedView());
  // Bubble can be active if user clicks it.
  EXPECT_TRUE(PasswordBubbleViewBase::manage_password_bubble()->CanActivate());

  std::unique_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(1,
            samples->GetCount(metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       CommandExecutionInPendingState) {
  // Open once with pending password: automagical!
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::CloseCurrentBubble();
  // Drain message pump to ensure the bubble view is cleared so that it can be
  // created again (it is checked on Mac to prevent re-opening the bubble when
  // clicking the location bar button repeatedly).
  content::RunAllPendingInMessageLoop();

  // This opening should be measured as manual.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());

  std::unique_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(1,
            samples->GetCount(metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1, samples->GetCount(metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       CommandExecutionInAutomaticSaveState) {
  SetupAutomaticPassword();
  EXPECT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::CloseCurrentBubble();
  content::RunAllPendingInMessageLoop();
  // Re-opening should count as manual.
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());

  std::unique_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(1, samples->GetCount(
                   metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(1, samples->GetCount(metrics_util::MANUAL_MANAGE_PASSWORDS));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, DontCloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble()
                   ->GetFocusManager()
                   ->GetFocusedView());
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_TRUE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DontCloseOnEscWithoutFocus) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_TRUE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, DontCloseOnKey) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::FocusChangedObserver focus_observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>")));
  focus_observer.Wait();
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble()
                   ->GetFocusManager()
                   ->GetFocusedView());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_TRUE(web_contents->IsFocusedElementEditable());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_K, false,
                                              false, false, false));
  EXPECT_TRUE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, DontCloseOnNavigation) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<body>Welcome!</body>")));
  EXPECT_TRUE(IsBubbleShowing());
}

// crbug.com/1194950.
// Test that the automatic save bubble ignores the browser activation and
// deactivation events.
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DontCloseOnDeactivation) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());

  browser()->window()->Deactivate();
  EXPECT_TRUE(IsBubbleShowing());

  browser()->window()->Activate();
  EXPECT_TRUE(IsBubbleShowing());
}

// crbug.com/1194950.
// Test that the automatic save bubble ignores the focus lost event.
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, DontCloseOnLostFocus) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::manage_password_bubble()
      ->GetOkButton()
      ->RequestFocus();

  browser()->window()->Deactivate();
  EXPECT_TRUE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       TwoTabsWithBubbleSwitch) {
  // Set up the first tab with the bubble.
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Set up the second tab and bring the bubble again.
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/empty.html"),
                            ui::PAGE_TRANSITION_TYPED));
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(IsBubbleShowing());
  EXPECT_EQ(1, tab_model->active_index());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Back to the first tab.
  tab_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       TwoTabsWithBubbleClose) {
  // Set up the second tab and bring the bubble there.
  ASSERT_TRUE(AddTabAtIndex(1, embedded_test_server()->GetURL("/empty.html"),
                            ui::PAGE_TRANSITION_TYPED));
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(IsBubbleShowing());
  EXPECT_EQ(1, tab_model->active_index());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Back to the first tab. Set up the bubble.
  tab_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  // Drain message pump to ensure the bubble view is cleared so that it can be
  // created again (it is checked on Mac to prevent re-opening the bubble when
  // clicking the location bar button repeatedly).
  content::RunAllPendingInMessageLoop();
  SetupPendingPassword();
  ASSERT_TRUE(IsBubbleShowing());

  // Queue an event to interact with the bubble (bubble should stay open for
  // now). Ideally this would use ui_controls::SendKeyPress(..), but picking
  // the event that would activate a button is tricky. It's also hard to send
  // events directly to the button, since that's buried in private classes.
  // Instead, simulate the action in
  // PasswordBubbleViewBase::PendingView:: ButtonPressed(), and
  // simulate the OS event queue by posting a task.
  auto press_button = [](PasswordBubbleViewBase* bubble, bool* ran) {
    bubble->Cancel();
    *ran = true;
  };

  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  bool ran_event_task = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(press_button, bubble, &ran_event_task));
  EXPECT_TRUE(IsBubbleShowing());

  // Close the tab.
  ASSERT_TRUE(tab_model->CloseWebContentsAt(0, 0));
  EXPECT_FALSE(IsBubbleShowing());

  // The bubble is now hidden, but not destroyed. However, the WebContents _is_
  // destroyed. Emptying the runloop will process the queued event, and should
  // not cause a crash trying to access objects owned by the WebContents.
  EXPECT_TRUE(bubble->GetWidget()->IsClosed());
  EXPECT_FALSE(ran_event_task);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(ran_event_task);
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, AutoSignin) {
  test_form()->url = GURL("https://example.com");
  test_form()->display_name = u"Peter";
  test_form()->username_value = u"pet12@gmail.com";
  test_form()->icon_url = embedded_test_server()->GetURL("/icon.png");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));

  SetupAutoSignin(std::move(local_credentials));
  EXPECT_TRUE(IsBubbleShowing());

  PasswordBubbleViewBase::CloseCurrentBubble();
  EXPECT_FALSE(IsBubbleShowing());
  content::RunAllPendingInMessageLoop();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE,
            PasswordsModelDelegateFromWebContents(web_contents)->GetState());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, AutoSigninNoFocus) {
  test_form()->url = GURL("https://example.com");
  test_form()->display_name = u"Peter";
  test_form()->username_value = u"pet12@gmail.com";
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(*test_form()));

  // Open another window with focus.
  Browser* focused_window = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));

  PasswordAutoSignInView::set_auto_signin_toast_timeout(0);
  SetupAutoSignin(std::move(local_credentials));
  EXPECT_TRUE(IsBubbleShowing());

  // Bring the first window back.
  ui_test_utils::BrowserDeactivationWaiter waiter(focused_window);
  browser()->window()->Activate();
  waiter.WaitForDeactivation();

  // Let asynchronous tasks run until the bubble stops showing.
  while (IsBubbleShowing())
    base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsBubbleShowing());
}

// Test that triggering the leak detection dialog successfully hides a showing
// bubble.
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, LeakPromptHidesBubble) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());

  GetController()->OnCredentialLeak(
      password_manager::CredentialLeakFlags::kPasswordSaved,
      GURL("https://example.com"), std::u16string(u"Eve"));
  EXPECT_FALSE(IsBubbleShowing());
}

// This is a regression test for crbug.com/1335418
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, SaveUiDismissalReason) {
  base::HistogramTester histogram_tester;

  SetupPendingPassword();
  ASSERT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::manage_password_bubble()->AcceptDialog();
  content::RunAllPendingInMessageLoop();
  ASSERT_FALSE(IsBubbleShowing());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}
