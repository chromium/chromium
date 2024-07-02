// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"
#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using ::testing::Field;
using ::testing::ReturnRef;

namespace {

// ManagePasswordsUIController subclass to capture the dialog instance
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(content::WebContents* web_contents);

  TestManagePasswordsUIController(const TestManagePasswordsUIController&) =
      delete;
  TestManagePasswordsUIController& operator=(
      const TestManagePasswordsUIController&) = delete;

  void OnDialogHidden() override;
  AccountChooserPrompt* CreateAccountChooser(
      CredentialManagerDialogController* controller) override;
  AutoSigninFirstRunPrompt* CreateAutoSigninPrompt(
      CredentialManagerDialogController* controller) override;
  CredentialLeakPrompt* CreateCredentialLeakPrompt(
      CredentialLeakDialogController* controller) override;

  AccountChooserDialogView* current_account_chooser() const {
    return static_cast<AccountChooserDialogView*>(current_account_chooser_);
  }

  AutoSigninFirstRunDialogView* current_autosignin_prompt() const {
    return static_cast<AutoSigninFirstRunDialogView*>(
        current_autosignin_prompt_);
  }

  CredentialLeakDialogView* current_credential_leak_prompt() const {
    return static_cast<CredentialLeakDialogView*>(
        current_credential_leak_prompt_);
  }

  MOCK_METHOD(void, OnDialogClosed, (), ());

 private:
  raw_ptr<AccountChooserPrompt, AcrossTasksDanglingUntriaged>
      current_account_chooser_;
  raw_ptr<AutoSigninFirstRunPrompt, AcrossTasksDanglingUntriaged>
      current_autosignin_prompt_;
  raw_ptr<CredentialLeakPrompt, AcrossTasksDanglingUntriaged>
      current_credential_leak_prompt_;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* web_contents)
    : ManagePasswordsUIController(web_contents),
      current_account_chooser_(nullptr),
      current_autosignin_prompt_(nullptr),
      current_credential_leak_prompt_(nullptr) {
  // Attach TestManagePasswordsUIController to |web_contents| so the default
  // ManagePasswordsUIController isn't created.
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(web_contents->GetUserData(UserDataKey()));
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

void TestManagePasswordsUIController::OnDialogHidden() {
  ManagePasswordsUIController::OnDialogHidden();
  OnDialogClosed();
}

AccountChooserPrompt* TestManagePasswordsUIController::CreateAccountChooser(
    CredentialManagerDialogController* controller) {
  current_account_chooser_ =
      ManagePasswordsUIController::CreateAccountChooser(controller);
  return current_account_chooser_;
}

AutoSigninFirstRunPrompt*
TestManagePasswordsUIController::CreateAutoSigninPrompt(
    CredentialManagerDialogController* controller) {
  current_autosignin_prompt_ =
      ManagePasswordsUIController::CreateAutoSigninPrompt(controller);
  return current_autosignin_prompt_;
}

CredentialLeakPrompt*
TestManagePasswordsUIController::CreateCredentialLeakPrompt(
    CredentialLeakDialogController* controller) {
  current_credential_leak_prompt_ =
      ManagePasswordsUIController::CreateCredentialLeakPrompt(controller);
  return current_credential_leak_prompt_;
}

std::unique_ptr<password_manager::PasswordFormManagerForUI> WrapFormInManager(
    const password_manager::PasswordForm* form) {
  auto submitted_manager =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();
  ON_CALL(*submitted_manager, GetPendingCredentials)
      .WillByDefault(ReturnRef(*form));
  return submitted_manager;
}

class PasswordDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void SetUpOnMainThread() override;
  void ShowUi(const std::string& name) override;

  void SetupChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin);

  content::WebContents* SetupTabWithTestController(Browser* browser);

  TestManagePasswordsUIController* controller() const { return controller_; }

  ChromePasswordManagerClient* client() const {
    return ChromePasswordManagerClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MOCK_METHOD(void,
              OnChooseCredential,
              (const password_manager::PasswordForm*),
              ());
  MOCK_METHOD(void, OnIconRequestDone, (), ());

  // Called on the server background thread.
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response(new BasicHttpResponse);
    if (request.relative_url == "/icon.png") {
      OnIconRequestDone();
    }
    return std::move(response);
  }

 private:
  raw_ptr<TestManagePasswordsUIController, AcrossTasksDanglingUntriaged>
      controller_;
};

void PasswordDialogViewTest::SetUpOnMainThread() {
#if BUILDFLAG(IS_MAC)
  // On non-Mac platforms, animations are globally disabled during tests; on
  // Mac they are generally not, but these tests are dramatically slower and
  // flakier with animations.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableModalAnimations);
#endif
  SetupTabWithTestController(browser());
}

void PasswordDialogViewTest::SetupChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials,
    const url::Origin& origin) {
  client()->PromptUserToChooseCredentials(
      std::move(local_credentials), origin,
      base::BindOnce(&PasswordDialogViewTest::OnChooseCredential,
                     base::Unretained(this)));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
}

content::WebContents* PasswordDialogViewTest::SetupTabWithTestController(
    Browser* browser) {
  // Open a new tab with modified ManagePasswordsUIController.
  content::WebContents* tab =
      browser->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<content::WebContents> new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::WebContents* raw_new_tab = new_tab.get();
  EXPECT_TRUE(raw_new_tab);

  // ManagePasswordsUIController needs ChromePasswordManagerClient for logging
  // and ChromePasswordManagerClient needs ChromeAutofillClient.
  autofill::ChromeAutofillClient::CreateForWebContents(raw_new_tab);
  ChromePasswordManagerClient::CreateForWebContents(raw_new_tab);
  EXPECT_TRUE(ChromePasswordManagerClient::FromWebContents(raw_new_tab));
  controller_ = new TestManagePasswordsUIController(raw_new_tab);
  browser->tab_strip_model()->AppendWebContents(std::move(new_tab), true);

  // Navigate to a Web URL.
  EXPECT_NO_FATAL_FAILURE(EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser, GURL("http://www.google.com"))));
  EXPECT_EQ(controller_,
            ManagePasswordsUIController::FromWebContents(raw_new_tab));
  return raw_new_tab;
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithMultipleCredentialsReturnEmpty) {
  // Set up the test server to handle the form icon request.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &PasswordDialogViewTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.icon_url = GURL("broken url");
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));
  form.icon_url = embedded_test_server()->GetURL("/icon.png");
  form.display_name = u"Peter Pan";
  form.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/federation"));
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  // Prepare to capture the network request.
  EXPECT_CALL(*this, OnIconRequestDone());
  embedded_test_server()->StartAcceptingConnections();

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));
  ASSERT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  dialog->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    PopupAccountChooserWithMultipleCredentialsReturnNonEmpty) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.icon_url = GURL("broken url");
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));
  GURL icon_url("https://google.com/icon.png");
  form.icon_url = icon_url;
  form.display_name = u"Peter Pan";
  form.federation_origin =
      url::SchemeHostPort(GURL("https://google.com/federation"));
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));
  ASSERT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, but we should not pop up the autosignin prompt as there were
  // multiple credentials available.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialReturnEmpty) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));

  EXPECT_TRUE(controller()->current_account_chooser());
  AccountChooserDialogView* dialog = controller()->current_account_chooser();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  dialog->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialClickSignIn) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));

  EXPECT_TRUE(controller()->current_account_chooser());
  views::BubbleDialogDelegateView* dialog =
      controller()->current_account_chooser();
  views::test::WidgetDestroyedWaiter bubble_observer(dialog->GetWidget());
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  dialog->Accept();
  bubble_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithSingleCredentialReturnNonEmpty) {
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  EXPECT_TRUE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAccountChooserWithDisabledAutoSignin) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  SetupChooseCredentials(std::move(local_credentials),
                         url::Origin::Create(origin));

  EXPECT_TRUE(controller()->current_account_chooser());

  // After picking a credential, we should pass it back to the caller via the
  // callback, and pop up the autosignin prompt iff we should show it.
  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because the setting is off.
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, PopupAccountChooserInIncognito) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin));
  GURL origin("https://example.com");
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;
  local_credentials.push_back(
      std::make_unique<password_manager::PasswordForm>(form));

  Browser* incognito = CreateIncognitoBrowser();
  content::WebContents* tab = SetupTabWithTestController(incognito);
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(tab);
  client->PromptUserToChooseCredentials(
      std::move(local_credentials), url::Origin::Create(origin),
      base::BindOnce(&PasswordDialogViewTest::OnChooseCredential,
                     base::Unretained(this)));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->current_account_chooser());

  EXPECT_CALL(*this, OnChooseCredential(testing::Pointee(form)));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because of Incognito.
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, EscCancelsAutoSigninPrompt) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
  controller()->OnPromptEnableAutoSignin();
  ASSERT_TRUE(controller()->current_autosignin_prompt());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  AutoSigninFirstRunDialogView* dialog =
      controller()->current_autosignin_prompt();
  views::test::WidgetDestroyedWaiter bubble_observer(dialog->GetWidget());
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_CALL(*controller(), OnDialogClosed());
  EXPECT_TRUE(dialog->GetWidget()->client_view()->AcceleratorPressed(esc));
  bubble_observer.Wait();
  content::RunAllPendingInMessageLoop();
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(controller());
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, PopupCredentialsLeakedPrompt) {
  CredentialLeakType leak_type = CredentialLeakFlags::kPasswordSaved |
                                 CredentialLeakFlags::kPasswordUsedOnOtherSites;
  GURL origin("https://example.com");
  std::u16string username(u"Eve");
  controller()->OnCredentialLeak(leak_type, origin, username);
  ASSERT_TRUE(controller()->current_credential_leak_prompt());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  CredentialLeakDialogView* dialog =
      controller()->current_credential_leak_prompt();
  views::test::WidgetDestroyedWaiter bubble_observer(dialog->GetWidget());
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_TRUE(dialog->GetWidget()->client_view()->AcceleratorPressed(esc));
  bubble_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest,
                       PopupAutoSigninPromptAfterBlockedZeroclick) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->profile()->GetPrefs()));

  GURL origin("https://example.com");
  password_manager::PasswordForm form;
  form.url = origin;
  form.username_value = u"peter@pan.test";
  form.password_value = u"I can fly!";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;

  // Successful login alone will not prompt:
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Blocked automatic sign-in will not prompt:
  std::unique_ptr<password_manager::PasswordForm> blocked_form(
      new password_manager::PasswordForm(form));
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with a distinct form after block will not prompt:
  blocked_form = std::make_unique<password_manager::PasswordForm>(form);
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  form.username_value = u"notpeter@pan.test";
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());

  // Successful login with the same form after block will not prompt if auto
  // sign-in is off:
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  blocked_form = std::make_unique<password_manager::PasswordForm>(form);
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());
  browser()->profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  // Successful login with the same form after block will *prompt:
  blocked_form = std::make_unique<password_manager::PasswordForm>(form);
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_TRUE(controller()->current_autosignin_prompt());
}

// DialogBrowserTest methods for interactive dialog invocation.
void PasswordDialogViewTest::ShowUi(const std::string& name) {
  if (name == "AutoSigninFirstRun") {
    controller()->OnPromptEnableAutoSignin();
    return;
  }

  GURL origin("https://example.com");
  std::u16string username(u"Eve");
  if (name == "CredentialLeak") {
    CredentialLeakType leak_type =
        CredentialLeakFlags::kPasswordSaved |
        CredentialLeakFlags::kPasswordUsedOnOtherSites;
    controller()->OnCredentialLeak(leak_type, origin, username);
    return;
  }

  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      local_credentials;
  password_manager::PasswordForm form;
  form.url = origin;
  form.display_name = u"Peter Pan";
  form.username_value = u"peter@pan.test";
  form.match_type = password_manager::PasswordForm::MatchType::kExact;

  if (name == "PopupAutoSigninPrompt") {
    form.icon_url = GURL("broken url");
    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));
    form.icon_url = GURL("https://google.com/icon.png");
    form.display_name = u"Peter";
    form.federation_origin =
        url::SchemeHostPort(GURL("https://google.com/federation"));
    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));
    controller()->OnAutoSignin(std::move(local_credentials),
                               url::Origin::Create(origin));
    EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE,
              controller()->GetState());
  } else if (base::StartsWith(name, "PopupAccountChooserWith",
                              base::CompareCase::SENSITIVE)) {
    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));
    if (name == "PopupAccountChooserWithMultipleCredentialClickSignIn") {
      form.icon_url = GURL("https://google.com/icon.png");
      form.display_name = u"Tinkerbell";
      form.username_value = u"tinkerbell@pan.test";
      form.federation_origin =
          url::SchemeHostPort(GURL("https://google.com/neverland"));
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(form));
      form.display_name = u"James Hook";
      form.username_value = u"james@pan.test";
      form.federation_origin =
          url::SchemeHostPort(GURL("https://google.com/jollyroger"));
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(form));
      form.display_name = u"Wendy Darling";
      form.username_value = u"wendy@pan.test";
      form.federation_origin =
          url::SchemeHostPort(GURL("https://google.com/london"));
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(form));
    }
    SetupChooseCredentials(std::move(local_credentials),
                           url::Origin::Create(origin));
  } else {
    ADD_FAILURE() << "Unknown dialog type";
    return;
  }
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, InvokeUi_AutoSigninFirstRun) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, InvokeUi_CredentialLeak) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordDialogViewTest, InvokeUi_PopupAutoSigninPrompt) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithSingleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithMultipleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

}  // namespace
