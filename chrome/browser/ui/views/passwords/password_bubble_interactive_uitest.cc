// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_pending_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/window/dialog_client_view.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using testing::Eq;
using testing::Field;
using testing::_;

namespace {

const char kDisplayDispositionMetric[] = "PasswordBubble.DisplayDisposition";

bool IsBubbleShowing() {
  return PasswordBubbleViewBase::manage_password_bubble() &&
         PasswordBubbleViewBase::manage_password_bubble()
             ->GetWidget()
             ->IsVisible();
}

const views::DialogClientView* GetDialogClientView(
    const LocationBarBubbleDelegateView* bubble) {
  const views::DialogClientView* view =
      bubble->GetWidget()->client_view()->AsDialogClientView();
  DCHECK(view);
  return view;
}

}  // namespace

namespace metrics_util = password_manager::metrics_util;

class PasswordBubbleInteractiveUiTest : public ManagePasswordsTest {
 public:
  PasswordBubbleInteractiveUiTest() {}
  ~PasswordBubbleInteractiveUiTest() override {}

  MOCK_METHOD0(OnIconRequestDone, void());

  // Called on the server background thread.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    if (request.relative_url == "/icon.png") {
      OnIconRequestDone();
    }
    return std::move(response);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordBubbleInteractiveUiTest);
};

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, BasicOpenAndClose) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  EXPECT_FALSE(IsBubbleShowing());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  const PasswordPendingView* bubble = static_cast<const PasswordPendingView*>(
      PasswordBubbleViewBase::manage_password_bubble());
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
  bubble = static_cast<const PasswordPendingView*>(
      PasswordBubbleViewBase::manage_password_bubble());
  // A pending password with empty username should initially focus on the
  // username field.
  EXPECT_EQ(bubble->username_field(),
            bubble->GetFocusManager()->GetFocusedView());
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
  EXPECT_TRUE(GetDialogClientView(bubble)->ok_button());
  EXPECT_EQ(GetDialogClientView(bubble)->ok_button(),
            bubble->GetFocusManager()->GetFocusedView());
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

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, CloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble()
                   ->GetFocusManager()
                   ->GetFocusedView());
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, CloseOnEsc) {
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, CloseOnKey) {
  content::WindowedNotificationObserver focus_observer(
      content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>"));
  focus_observer.Wait();
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  EXPECT_FALSE(PasswordBubbleViewBase::manage_password_bubble()
                   ->GetFocusManager()
                   ->GetFocusedView());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents->IsFocusedElementEditable());
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_K, false,
                                              false, false, false));
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       TwoTabsWithBubbleSwitch) {
  // Set up the first tab with the bubble.
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Set up the second tab and bring the bubble again.
  AddTabAtIndex(1, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(1, true);
  EXPECT_FALSE(IsBubbleShowing());
  EXPECT_EQ(1, tab_model->active_index());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Back to the first tab.
  tab_model->ActivateTabAt(0, true);
  EXPECT_FALSE(IsBubbleShowing());
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       TwoTabsWithBubbleClose) {
  // Set up the second tab and bring the bubble there.
  AddTabAtIndex(1, GURL("http://example.com/"), ui::PAGE_TRANSITION_TYPED);
  TabStripModel* tab_model = browser()->tab_strip_model();
  tab_model->ActivateTabAt(1, true);
  EXPECT_FALSE(IsBubbleShowing());
  EXPECT_EQ(1, tab_model->active_index());
  SetupPendingPassword();
  EXPECT_TRUE(IsBubbleShowing());
  // Back to the first tab. Set up the bubble.
  tab_model->ActivateTabAt(0, true);
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
    bubble->model()->OnNeverForThisSiteClicked();
    *ran = true;
  };

  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  bool ran_event_task = false;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(press_button, bubble, &ran_event_task));
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
  // Set up the test server to handle the form icon request.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PasswordBubbleInteractiveUiTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  test_form()->origin = GURL("https://example.com");
  test_form()->display_name = base::ASCIIToUTF16("Peter");
  test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
  test_form()->icon_url = embedded_test_server()->GetURL("/icon.png");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  local_credentials.push_back(
      std::make_unique<autofill::PasswordForm>(*test_form()));

  // Prepare to capture the network request.
  EXPECT_CALL(*this, OnIconRequestDone());
  embedded_test_server()->StartAcceptingConnections();

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
  test_form()->origin = GURL("https://example.com");
  test_form()->display_name = base::ASCIIToUTF16("Peter");
  test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
  std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
  local_credentials.push_back(
      std::make_unique<autofill::PasswordForm>(*test_form()));

  // Open another window with focus.
  Browser* focused_window = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));

  PasswordAutoSignInView::set_auto_signin_toast_timeout(0);
  SetupAutoSignin(std::move(local_credentials));
  EXPECT_TRUE(IsBubbleShowing());

  // Bring the first window back. The toast closes by timeout.
  focused_window->window()->Close();
  browser()->window()->Activate();
  content::RunAllPendingInMessageLoop();
  ui_test_utils::BrowserActivationWaiter waiter(browser());
  waiter.WaitForActivation();

  EXPECT_FALSE(IsBubbleShowing());
}
