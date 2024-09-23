// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_list_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/focus_changed_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/widget_test.h"

using base::Bucket;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::PasswordForm;
using testing::_;
using testing::ElementsAre;
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

void ClickOnView(views::View* view) {
  CHECK(view);
  ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMousePressed(pressed);
  ui::MouseEvent released_event =
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON);
  view->OnMouseReleased(released_event);
}

PasswordForm CreateSharedCredentials(
    const GURL& url,
    const std::u16string& username = u"username",
    const std::u16string& sender_name = u"Elisa Becket") {
  PasswordForm shared_credentials;
  shared_credentials.signon_realm = url.GetWithEmptyPath().spec();
  shared_credentials.url = url;
  shared_credentials.username_value = username;
  shared_credentials.password_value = u"12345";
  shared_credentials.match_type = PasswordForm::MatchType::kExact;
  shared_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  shared_credentials.sender_name = sender_name;
  return shared_credentials;
}

}  // namespace

namespace metrics_util = password_manager::metrics_util;

class PasswordBubbleInteractiveUiTest : public ManagePasswordsTest {
 public:
  PasswordBubbleInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            password_manager::features::kPasswordManualFallbackAvailable});
  }

  PasswordBubbleInteractiveUiTest(const PasswordBubbleInteractiveUiTest&) =
      delete;
  PasswordBubbleInteractiveUiTest& operator=(
      const PasswordBubbleInteractiveUiTest&) = delete;

  ~PasswordBubbleInteractiveUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  ExecuteManagePasswordsCommand();
  EXPECT_TRUE(IsBubbleShowing());

  std::unique_ptr<base::HistogramSamples> samples(
      GetSamples(kDisplayDispositionMetric));
  EXPECT_EQ(2, samples->GetCount(metrics_util::AUTOMATIC_ADD_USERNAME_BUBBLE));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_WITH_PASSWORD_PENDING));
  EXPECT_EQ(0, samples->GetCount(metrics_util::MANUAL_MANAGE_PASSWORDS));
  EXPECT_EQ(0, samples->GetCount(
                   metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION));
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
  int previous_tab_count = tab_model->count();
  tab_model->CloseWebContentsAt(0, 0);
  ASSERT_EQ(previous_tab_count - 1, tab_model->count());
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
  while (IsBubbleShowing()) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(IsBubbleShowing());
}

// Test that triggering the leak detection dialog successfully hides a showing
// bubble.
IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest, LeakPromptHidesBubble) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  SetupPendingPassword();
  ASSERT_NE(PasswordBubbleViewBase::manage_password_bubble(), nullptr);
  views::Widget* password_bubble =
      PasswordBubbleViewBase::manage_password_bubble()->GetWidget();
  ASSERT_NE(password_bubble, nullptr);
  views::test::WidgetVisibleWaiter(password_bubble).Wait();

  GetController()->OnCredentialLeak(
      password_manager::CredentialLeakFlags::kPasswordSaved,
      GURL("https://example.com"), std::u16string(u"Eve"));
  views::test::WidgetDestroyedWaiter(password_bubble).Wait();
}

class PasswordBubbleInteractiveUiTestWithExplicitBrowserSigninParam
    : public PasswordBubbleInteractiveUiTest,
      public base::test::WithFeatureOverride {
 public:
  PasswordBubbleInteractiveUiTestWithExplicitBrowserSigninParam()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop) {}

  bool is_explicit_browser_signin() const { return IsParamFeatureEnabled(); }
};

// This is a regression test for crbug.com/1335418
IN_PROC_BROWSER_TEST_P(
    PasswordBubbleInteractiveUiTestWithExplicitBrowserSigninParam,
    SaveUiDismissalReason) {
  base::HistogramTester histogram_tester;

  SetupPendingPassword();
  ASSERT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::manage_password_bubble()->AcceptDialog();
  content::RunAllPendingInMessageLoop();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (is_explicit_browser_signin()) {
    // Bubble is still showing because of the Sign in Promo showing after saving
    // the password.
    ASSERT_TRUE(IsBubbleShowing());
    // Close it without any action.
    PasswordBubbleViewBase::manage_password_bubble()->CloseCurrentBubble();
  }
#endif

  ASSERT_FALSE(IsBubbleShowing());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason",
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_P(
    PasswordBubbleInteractiveUiTestWithExplicitBrowserSigninParam,
    DismissBubbleBeforeSignInPromoDoesNotIncrementPref) {
  signin::IdentityTestEnvironment identity_test_env;

  AccountInfo info = identity_test_env.MakeAccountAvailable(
      "test@email.com", {.set_cookie = true});
  SetupPendingPassword();
  ASSERT_TRUE(IsBubbleShowing());
  PasswordBubbleViewBase::manage_password_bubble()->Cancel();

  EXPECT_EQ(0, browser()->profile()->GetPrefs()->GetInteger(
                   prefs::kAutofillSignInPromoDismissCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*browser()->profile()->GetPrefs())
                   .GetAutofillSigninPromoDismissCount(info.gaia));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    PasswordBubbleInteractiveUiTestWithExplicitBrowserSigninParam);

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       ClosesBubbleOnNavigationToFullPasswordManager) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kManagePasswordsButton)));
  EXPECT_FALSE(IsBubbleShowing());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_manager::metrics_util::PasswordManagementBubbleInteractions::
          kManagePasswordsButtonClicked,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       ClosesBubbleOnClickingGooglePasswordManagerLink) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());
  // Navigate to the details view, and click on the Edit Note button to display
  // the footer.
  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());
  ClickOnView(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kEditNoteButton)));

  views::View* footnote_view = PasswordBubbleViewBase::manage_password_bubble()
                                   ->GetFootnoteViewForTesting();
  ASSERT_TRUE(footnote_view);
  views::Link* link =
      static_cast<views::StyledLabel*>(footnote_view)->GetFirstLinkForTesting();
  ClickOnView(link);
  EXPECT_FALSE(IsBubbleShowing());

  histogram_tester.ExpectBucketCount(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_manager::metrics_util::PasswordManagementBubbleInteractions::
          kGooglePasswordManagerLinkClicked,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       CopiesPasswordDetailsToClipboardOnCopyButtonClicks) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string clipboard_text;
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  static_cast<ManagePasswordsView*>(
      PasswordBubbleViewBase::manage_password_bubble())
      ->DisplayDetailsOfPasswordForTesting(*test_form());

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kCopyUsernameButton)));
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, u"test_username");

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kCopyPasswordButton)));
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, u"test_password");

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "PasswordManager.PasswordManagementBubble.UserAction"),
              ElementsAre(Bucket(password_manager::metrics_util::
                                     PasswordManagementBubbleInteractions::
                                         kUsernameCopyButtonClicked,
                                 1),
                          Bucket(password_manager::metrics_util::
                                     PasswordManagementBubbleInteractions::
                                         kPasswordCopyButtonClicked,
                                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       RevealPasswordOnEyeIconClicks) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  static_cast<ManagePasswordsView*>(
      PasswordBubbleViewBase::manage_password_bubble())
      ->DisplayDetailsOfPasswordForTesting(*test_form());

  views::Label* password_label = static_cast<views::Label*>(
      PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
          static_cast<int>(
              password_manager::ManagePasswordsViewIDs::kPasswordLabel)));
  ASSERT_TRUE(password_label->GetObscured());

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kRevealPasswordButton)));
  EXPECT_FALSE(password_label->GetObscured());

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kRevealPasswordButton)));
  EXPECT_TRUE(password_label->GetObscured());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_manager::metrics_util::PasswordManagementBubbleInteractions::
          kPasswordShowButtonClicked,
      1);
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DisplaysNewUsernameAfterEditing) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->username_value = u"";
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  auto* username_label =
      static_cast<views::Label*>(bubble->GetViewByID(static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kUsernameLabel)));
  auto* username_textfield =
      static_cast<views::Textfield*>(bubble->GetViewByID(static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kUsernameTextField)));
  ASSERT_EQ(username_label->GetText(), u"No username");
  ASSERT_FALSE(username_textfield->IsDrawn());

  ClickOnView(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kEditUsernameButton)));
  EXPECT_FALSE(username_label->IsDrawn());
  EXPECT_EQ(username_textfield->GetText(), u"");

  username_textfield->SetText(u"new_username");
  bubble->AcceptDialog();
  EXPECT_EQ(static_cast<views::Label*>(
                bubble->GetViewByID(static_cast<int>(
                    password_manager::ManagePasswordsViewIDs::kUsernameLabel)))
                ->GetText(),
            u"new_username");
  EXPECT_FALSE(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kUsernameTextField)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::
                         kUsernameEditButtonClicked,
                 1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kUsernameAdded,
                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DisplaysCorrectTextAfterAddingNote) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  auto* note_label = static_cast<views::Label*>(bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel)));
  auto* note_textarea =
      static_cast<views::Textarea*>(bubble->GetViewByID(static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kNoteTextarea)));
  ASSERT_EQ(note_label->GetText(), u"No note added");
  EXPECT_FALSE(note_textarea->IsDrawn());

  ClickOnView(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kEditNoteButton)));
  EXPECT_FALSE(note_label->IsDrawn());
  EXPECT_EQ(note_textarea->GetText(), u"");

  note_textarea->SetText(u"new note");
  bubble->AcceptDialog();
  EXPECT_EQ(static_cast<views::Label*>(
                bubble->GetViewByID(static_cast<int>(
                    password_manager::ManagePasswordsViewIDs::kNoteLabel)))
                ->GetText(),
            u"new note");
  EXPECT_FALSE(bubble
                   ->GetViewByID(static_cast<int>(
                       password_manager::ManagePasswordsViewIDs::kNoteTextarea))
                   ->IsDrawn());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(
              password_manager::metrics_util::
                  PasswordManagementBubbleInteractions::kNoteEditButtonClicked,
              1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteAdded,
                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DisplaysCorrectTextAfterEditingNote) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  auto* note_label = static_cast<views::Label*>(bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel)));
  auto* note_textarea =
      static_cast<views::Textarea*>(bubble->GetViewByID(static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kNoteTextarea)));
  ASSERT_EQ(note_label->GetText(), u"current note");
  ASSERT_FALSE(note_textarea->IsDrawn());

  ClickOnView(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kEditNoteButton)));
  EXPECT_FALSE(note_label->IsDrawn());
  EXPECT_EQ(note_textarea->GetText(), u"current note");

  note_textarea->SetText(u"new note");
  bubble->AcceptDialog();
  EXPECT_EQ(static_cast<views::Label*>(
                bubble->GetViewByID(static_cast<int>(
                    password_manager::ManagePasswordsViewIDs::kNoteLabel)))
                ->GetText(),
            u"new note");
  EXPECT_FALSE(bubble
                   ->GetViewByID(static_cast<int>(
                       password_manager::ManagePasswordsViewIDs::kNoteTextarea))
                   ->IsDrawn());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(
              password_manager::metrics_util::
                  PasswordManagementBubbleInteractions::kNoteEditButtonClicked,
              1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteEdited,
                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       DisplaysCorrectTextAfterDeletingNote) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  auto* note_label = static_cast<views::Label*>(bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel)));
  auto* note_textarea =
      static_cast<views::Textarea*>(bubble->GetViewByID(static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kNoteTextarea)));
  ASSERT_EQ(note_label->GetText(), u"current note");
  ASSERT_FALSE(note_textarea->IsDrawn());

  ClickOnView(bubble->GetViewByID(static_cast<int>(
      password_manager::ManagePasswordsViewIDs::kEditNoteButton)));
  EXPECT_EQ(note_textarea->GetText(), u"current note");
  EXPECT_FALSE(note_label->IsDrawn());

  note_textarea->SetText(u"");
  bubble->AcceptDialog();
  EXPECT_EQ(static_cast<views::Label*>(
                bubble->GetViewByID(static_cast<int>(
                    password_manager::ManagePasswordsViewIDs::kNoteLabel)))
                ->GetText(),
            u"No note added");
  EXPECT_FALSE(bubble
                   ->GetViewByID(static_cast<int>(
                       password_manager::ManagePasswordsViewIDs::kNoteTextarea))
                   ->IsDrawn());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(
              password_manager::metrics_util::
                  PasswordManagementBubbleInteractions::kNoteEditButtonClicked,
              1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteDeleted,
                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       RecordsMetricsForCopyingFullNoteWithKeyboardShortcuts) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  views::View* note_view = bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel));
  note_view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A,
                                       ui::EF_CONTROL_DOWN));
  note_view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_C,
                                       ui::EF_CONTROL_DOWN));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullySelected,
                 1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullyCopied,
                 1)));
}

IN_PROC_BROWSER_TEST_F(
    PasswordBubbleInteractiveUiTest,
    RecordsMetricsForCopyingFullNoteWithSelectAllAndCopyCommands) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  auto* note_label = static_cast<views::Label*>(bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel)));
  note_label->ExecuteCommand(views::Label::MenuCommands::kSelectAll,
                             /*event_flags=*/0);
  note_label->ExecuteCommand(views::Label::MenuCommands::kCopy,
                             /*event_flags=*/0);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullySelected,
                 1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullyCopied,
                 1)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       RecordsMetricsForCopyingFullNoteAfterMouseSelection) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  views::View* note_view = bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel));
  auto* note_label = static_cast<views::Label*>(note_view);
  note_view->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  note_label->SelectAll();
  note_view->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  note_view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_C,
                                       ui::EF_CONTROL_DOWN));
  note_label->ExecuteCommand(views::Label::MenuCommands::kCopy,
                             /*event_flags=*/0);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullySelected,
                 1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNoteFullyCopied,
                 2)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       RecordsMetricsForCopyingPartOfNoteAfterMouseSelection) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  auto* bubble = PasswordBubbleViewBase::manage_password_bubble();
  test_form()->SetNoteWithEmptyUniqueDisplayName(u"current note");
  static_cast<ManagePasswordsView*>(bubble)->DisplayDetailsOfPasswordForTesting(
      *test_form());

  views::View* note_view = bubble->GetViewByID(
      static_cast<int>(password_manager::ManagePasswordsViewIDs::kNoteLabel));
  auto* note_label = static_cast<views::Label*>(note_view);
  note_view->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  note_label->SelectRange(gfx::Range(0, 5));
  note_view->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  note_view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_C,
                                       ui::EF_CONTROL_DOWN));
  note_label->ExecuteCommand(views::Label::MenuCommands::kCopy,
                             /*event_flags=*/0);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.PasswordManagementBubble.UserAction"),
      ElementsAre(
          Bucket(
              password_manager::metrics_util::
                  PasswordManagementBubbleInteractions::kNotePartiallySelected,
              1),
          Bucket(password_manager::metrics_util::
                     PasswordManagementBubbleInteractions::kNotePartiallyCopied,
                 2)));
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleInteractiveUiTest,
                       NavigateToManagementDetailsViewAndTakeScreenshot) {
  const char kFirstCredentialsRow[] = "FirstCredentialsRow";

  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth_for_testing =
      GetController()->BypassUserAuthtForTesting();
  auto setup_passwords = [this]() { SetupManagingPasswords(); };

  RunTestSequence(
      Do(setup_passwords), PressButton(kPasswordsOmniboxKeyIconElementId),
      WaitForShow(ManagePasswordsView::kTopView),
      EnsurePresent(ManagePasswordsListView::kTopView),
      NameChildViewByType<RichHoverButton>(ManagePasswordsListView::kTopView,
                                           kFirstCredentialsRow),
      PressButton(kFirstCredentialsRow),
      WaitForShow(ManagePasswordsDetailsView::kTopView),
      EnsureNotPresent(ManagePasswordsListView::kTopView),
      // Screenshots are supposed only on Windows.
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(ManagePasswordsDetailsView::kTopView,
                 /*screenshot_name=*/std::string(), /*baseline_cl=*/"5189779"));
}

// TODO(crbug.com/364687935): Failing on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NavigateToManagementDetailsViewWithMoveFooterVisibleAndTakeScreenshot \
  DISABLED_NavigateToManagementDetailsViewWithMoveFooterVisibleAndTakeScreenshot
#else
#define MAYBE_NavigateToManagementDetailsViewWithMoveFooterVisibleAndTakeScreenshot \
  NavigateToManagementDetailsViewWithMoveFooterVisibleAndTakeScreenshot
#endif
IN_PROC_BROWSER_TEST_F(
    PasswordBubbleInteractiveUiTest,
    MAYBE_NavigateToManagementDetailsViewWithMoveFooterVisibleAndTakeScreenshot) {
  const char kFirstCredentialsRow[] = "FirstCredentialsRow";

  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth_for_testing =
      GetController()->BypassUserAuthtForTesting();
  auto setup_passwords = [this]() {
    ConfigurePasswordSync(SyncConfiguration::kAccountStorageOnly);
    SetupManagingPasswords(GURL(u"http://test-url.com"));
  };

  RunTestSequence(
      Do(setup_passwords), PressButton(kPasswordsOmniboxKeyIconElementId),
      WaitForShow(ManagePasswordsView::kTopView),
      EnsurePresent(ManagePasswordsListView::kTopView),
      NameChildViewByType<RichHoverButton>(ManagePasswordsListView::kTopView,
                                           kFirstCredentialsRow),
      PressButton(kFirstCredentialsRow),
      WaitForShow(ManagePasswordsDetailsView::kTopView),
      EnsureNotPresent(ManagePasswordsListView::kTopView),
      // Screenshots are supposed only on Windows.
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(ManagePasswordsView::kFooterId,
                 /*screenshot_name=*/std::string(), /*baseline_cl=*/"5189779"));
}

class PasswordBubbleWithManagePasswordButtonInteractiveUiTest
    : public PasswordBubbleInteractiveUiTest {
 public:
  ~PasswordBubbleWithManagePasswordButtonInteractiveUiTest() override = default;
  auto ScreenshotSharedPasswordsNotificationRootView(const char* baseline);

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      password_manager::features::kPasswordManualFallbackAvailable};
};

IN_PROC_BROWSER_TEST_F(PasswordBubbleWithManagePasswordButtonInteractiveUiTest,
                       ClosesBubbleOnNavigationToPasswordDetailsSubpage) {
  base::HistogramTester histogram_tester;

  SetupManagingPasswords();
  EXPECT_FALSE(IsBubbleShowing());
  ExecuteManagePasswordsCommand();
  ASSERT_TRUE(IsBubbleShowing());

  static_cast<ManagePasswordsView*>(
      PasswordBubbleViewBase::manage_password_bubble())
      ->DisplayDetailsOfPasswordForTesting(*test_form());

  ClickOnView(PasswordBubbleViewBase::manage_password_bubble()->GetViewByID(
      static_cast<int>(
          password_manager::ManagePasswordsViewIDs::kManagePasswordButton)));
  EXPECT_FALSE(IsBubbleShowing());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_manager::metrics_util::PasswordManagementBubbleInteractions::
          kManagePasswordButtonClicked,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.UIDismissalReason",
      password_manager::metrics_util::CLICKED_MANAGE_PASSWORD, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordBubbleWithManagePasswordButtonInteractiveUiTest,
                       NavigateToManagementDetailsViewAndTakeScreenshot) {
  const char kFirstCredentialsRow[] = "FirstCredentialsRow";

  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth_for_testing =
      GetController()->BypassUserAuthtForTesting();
  auto setup_passwords = [this]() { SetupManagingPasswords(); };

  RunTestSequence(
      Do(setup_passwords), PressButton(kPasswordsOmniboxKeyIconElementId),
      WaitForShow(ManagePasswordsView::kTopView),
      EnsurePresent(ManagePasswordsListView::kTopView),
      NameChildViewByType<RichHoverButton>(ManagePasswordsListView::kTopView,
                                           kFirstCredentialsRow),
      PressButton(kFirstCredentialsRow),
      WaitForShow(ManagePasswordsDetailsView::kTopView),
      EnsureNotPresent(ManagePasswordsListView::kTopView),
      // Screenshots are supposed only on Windows.
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(ManagePasswordsDetailsView::kTopView,
                 /*screenshot_name=*/std::string(), /*baseline_cl=*/"5314579"));
}

class SharedPasswordsNotificationBubbleInteractiveUiTest
    : public PasswordBubbleInteractiveUiTest {
 public:
  ~SharedPasswordsNotificationBubbleInteractiveUiTest() override = default;
  auto ScreenshotSharedPasswordsNotificationRootView(const char* baseline);
};

auto SharedPasswordsNotificationBubbleInteractiveUiTest::
    ScreenshotSharedPasswordsNotificationRootView(const char* baseline) {
  constexpr char kRootViewName[] = "ScreenshotRootView";
  return Steps(
      NameViewRelative(
          SharedPasswordsNotificationView::kTopView, kRootViewName,
          [](views::View* view) { return view->GetWidget()->GetRootView(); }),
      Screenshot(kRootViewName,
                 /*screenshot_name=*/std::string(), /*baseline_cl=*/baseline));
}

IN_PROC_BROWSER_TEST_F(SharedPasswordsNotificationBubbleInteractiveUiTest,
                       SharedPasswordNotificationUIShowsUpAndTakeScreenshot) {
  GURL test_url = GURL("https://example.com");
  PasswordForm shared_credentials = CreateSharedCredentials(test_url);
  shared_credentials.sharing_notification_displayed = false;

  std::vector<password_manager::PasswordForm> forms = {shared_credentials};

  auto setup_shared_passwords = [&]() {
    GetController()->OnPasswordAutofilled(forms, url::Origin::Create(test_url),
                                          /*federated_matches=*/{});
  };

  RunTestSequence(Do(setup_shared_passwords),
                  WaitForShow(SharedPasswordsNotificationView::kTopView),
                  // Screenshots are supposed only on Windows.
                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshot can only run in pixel_tests on Windows."),
                  ScreenshotSharedPasswordsNotificationRootView("4783493"));
}

IN_PROC_BROWSER_TEST_F(
    SharedPasswordsNotificationBubbleInteractiveUiTest,
    MultipleSharedPasswordsNotificationUIShowsUpAndTakeScreenshot) {
  GURL test_url = GURL("https://example.com");
  PasswordForm shared_credentials1 =
      CreateSharedCredentials(test_url, u"username1");
  shared_credentials1.sharing_notification_displayed = false;

  PasswordForm shared_credentials2 =
      CreateSharedCredentials(test_url, u"username2");
  shared_credentials2.sharing_notification_displayed = false;

  std::vector<password_manager::PasswordForm> forms = {shared_credentials1,
                                                       shared_credentials2};

  auto setup_shared_passwords = [&]() {
    GetController()->OnPasswordAutofilled(forms, url::Origin::Create(test_url),
                                          /*federated_matches=*/{});
  };

  RunTestSequence(Do(setup_shared_passwords),
                  WaitForShow(SharedPasswordsNotificationView::kTopView),
                  // Screenshots are supposed only on Windows.
                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshot can only run in pixel_tests on Windows."),
                  ScreenshotSharedPasswordsNotificationRootView("4783493"));
}

// Tests the case when there are multiple shared passwords, but only one is not
// notified yet.
IN_PROC_BROWSER_TEST_F(
    SharedPasswordsNotificationBubbleInteractiveUiTest,
    OnlyUnnotifiedPasswordsNotificationUIShowsUpAndTakeScreenshot) {
  GURL test_url = GURL("https://example.com");
  PasswordForm shared_credentials1 =
      CreateSharedCredentials(test_url, u"username1", u"Sender One");
  shared_credentials1.sharing_notification_displayed = true;

  PasswordForm shared_credentials2 =
      CreateSharedCredentials(test_url, u"username2", u"Sender Two");
  shared_credentials2.sharing_notification_displayed = false;

  std::vector<password_manager::PasswordForm> forms = {shared_credentials1,
                                                       shared_credentials2};

  auto setup_shared_passwords = [&]() {
    GetController()->OnPasswordAutofilled(forms, url::Origin::Create(test_url),
                                          /*federated_matches=*/{});
  };

  RunTestSequence(Do(setup_shared_passwords),
                  WaitForShow(SharedPasswordsNotificationView::kTopView),
                  // Screenshots are supposed only on Windows.
                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshot can only run in pixel_tests on Windows."),
                  ScreenshotSharedPasswordsNotificationRootView("4783493"));
}

IN_PROC_BROWSER_TEST_F(
    SharedPasswordsNotificationBubbleInteractiveUiTest,
    SharedPasswordNotificationUIShouldNotShowIfNotifiedAlready) {
  GURL test_url = GURL("https://example.com");
  PasswordForm shared_credentials = CreateSharedCredentials(test_url);
  shared_credentials.sharing_notification_displayed = true;

  std::vector<password_manager::PasswordForm> forms = {shared_credentials};

  auto setup_shared_passwords = [&]() {
    GetController()->OnPasswordAutofilled(forms, url::Origin::Create(test_url),
                                          /*/*federated_matches=*/{});
  };

  RunTestSequence(Do(setup_shared_passwords),
                  EnsureNotPresent(SharedPasswordsNotificationView::kTopView));
}
