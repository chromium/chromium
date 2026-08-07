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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/account_chooser_dialog_view.h"
#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"
#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"
#include "chrome/browser/ui/views/passwords/password_combined_selector_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/features/password_features.h"
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
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr std::u16string_view kFirstDisplayName = u"Frank Sinatra";
constexpr std::u16string_view kFirstUsername = u"frank@sinat.ra";
constexpr std::u16string_view kSecondUsername = u"nancy@sinat.ra";

password_manager::PasswordForm CreatePasswordForm(
    const GURL& url,
    const std::u16string& username,
    const std::u16string& password) {
  password_manager::PasswordForm password_form;
  password_form.url = url;
  password_form.signon_realm = url.GetWithEmptyPath().spec();
  password_form.username_value = username;
  password_form.password_value = password;
  return password_form;
}

void GetRadioButtons(views::View* parent,
                     std::vector<views::RadioButton*>& buttons) {
  for (views::View* child : parent->children()) {
    if (views::IsViewClass<views::RadioButton>(child)) {
      buttons.push_back(static_cast<views::RadioButton*>(child));
    } else {
      GetRadioButtons(child, buttons);
    }
  }
}

std::vector<views::RadioButton*> GetRadioButtons(views::View* parent) {
  std::vector<views::RadioButton*> buttons;
  GetRadioButtons(parent, buttons);
  return buttons;
}

views::Label* GetLabelByID(views::View* parent, int id) {
  views::View* view = parent->GetViewByID(id);
  return view ? static_cast<views::Label*>(view) : nullptr;
}

void GetViewsByID(int id,
                  views::View* parent,
                  std::vector<views::View*>& views) {
  if (parent->GetID() == id) {
    views.push_back(parent);
  }
  for (views::View* child : parent->children()) {
    GetViewsByID(id, child, views);
  }
}

std::vector<views::View*> GetViewsByID(int id, views::View* parent) {
  std::vector<views::View*> views;
  GetViewsByID(id, parent, views);
  return views;
}

// ManagePasswordsUIController subclass to capture the dialog instance
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(content::WebContents* web_contents);

  TestManagePasswordsUIController(const TestManagePasswordsUIController&) =
      delete;
  TestManagePasswordsUIController& operator=(
      const TestManagePasswordsUIController&) = delete;

  void OnDialogHidden() override;

  std::unique_ptr<AccountChooserPrompt> CreateAccountChooser(
      CredentialManagerDialogController* controller) override;
  std::unique_ptr<AutoSigninFirstRunPrompt> CreateAutoSigninPrompt(
      CredentialManagerDialogController* controller) override;
  std::unique_ptr<CredentialLeakPrompt> CreateCredentialLeakPrompt(
      CredentialLeakDialogController* controller) override;

  AccountChooserPrompt* current_account_chooser() const {
    return current_account_chooser_;
  }

  AutoSigninFirstRunDialogView* current_autosignin_prompt() const {
    return static_cast<AutoSigninFirstRunDialogView*>(
        current_autosignin_prompt_);
  }

  views::Widget* current_credential_leak_widget() const {
    return current_credential_leak_prompt_->GetWidgetForTesting();
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

std::unique_ptr<AccountChooserPrompt>
TestManagePasswordsUIController::CreateAccountChooser(
    CredentialManagerDialogController* controller) {
  auto chooser = ManagePasswordsUIController::CreateAccountChooser(controller);
  current_account_chooser_ = chooser.get();
  return chooser;
}

std::unique_ptr<AutoSigninFirstRunPrompt>
TestManagePasswordsUIController::CreateAutoSigninPrompt(
    CredentialManagerDialogController* controller) {
  auto prompt = ManagePasswordsUIController::CreateAutoSigninPrompt(controller);
  current_autosignin_prompt_ = prompt.get();
  return prompt;
}

std::unique_ptr<CredentialLeakPrompt>
TestManagePasswordsUIController::CreateCredentialLeakPrompt(
    CredentialLeakDialogController* controller) {
  auto prompt =
      ManagePasswordsUIController::CreateCredentialLeakPrompt(controller);
  current_credential_leak_prompt_ = prompt.get();
  return prompt;
}

std::unique_ptr<password_manager::PasswordFormManagerForUI> WrapFormInManager(
    const password_manager::PasswordForm* form) {
  auto submitted_manager =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();
  ON_CALL(*submitted_manager, GetPendingCredentials)
      .WillByDefault(ReturnRef(*form));
  ON_CALL(*submitted_manager, IsFetchCompleted).WillByDefault(Return(true));
  return submitted_manager;
}

class PasswordDialogViewTest : public base::test::WithFeatureOverride,
                               public DialogBrowserTest {
 public:
  PasswordDialogViewTest()
      : base::test::WithFeatureOverride(
            password_manager::features::kCredentialManagementUnifiedUi) {}

  // DialogBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void ShowUi(const std::string& name) override;

  void SetupChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin);

  content::WebContents* SetupTabWithTestController(Browser* browser);

  TestManagePasswordsUIController* controller(
      Browser* target_browser = nullptr) const {
    if (!target_browser) {
      target_browser = browser();
    }
    content::WebContents* web_contents =
        target_browser->tab_strip_model()->GetActiveWebContents();
    return web_contents ? static_cast<TestManagePasswordsUIController*>(
                              ManagePasswordsUIController::FromWebContents(
                                  web_contents))
                        : nullptr;
  }

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
  std::unique_ptr<CredentialManagerDialogControllerMock>
      remote_actor_mock_controller_;
  std::unique_ptr<PasswordCombinedSelectorView> remote_actor_view_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      remote_actor_forms_;
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

void PasswordDialogViewTest::TearDownOnMainThread() {
  remote_actor_view_.reset();
  remote_actor_mock_controller_.reset();
  DialogBrowserTest::TearDownOnMainThread();
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
  new TestManagePasswordsUIController(raw_new_tab);
  browser->tab_strip_model()->AppendWebContents(std::move(new_tab), true);

  // Navigate to a Web URL.
  EXPECT_NO_FATAL_FAILURE(EXPECT_TRUE(
      ui_test_utils::NavigateToURL(browser, GURL("http://www.google.com"))));
  EXPECT_TRUE(ManagePasswordsUIController::FromWebContents(raw_new_tab));
  return raw_new_tab;
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
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
  views::Widget* widget = IsParamFeatureEnabled()
                              ? static_cast<PasswordCombinedSelectorView*>(
                                    controller()->current_account_chooser())
                                    ->GetWidget()
                              : static_cast<AccountChooserDialogView*>(
                                    controller()->current_account_chooser())
                                    ->GetWidget();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  widget->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(
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
  EXPECT_CALL(*this, OnChooseCredential(Pointee(form)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
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
  views::Widget* widget = IsParamFeatureEnabled()
                              ? static_cast<PasswordCombinedSelectorView*>(
                                    controller()->current_account_chooser())
                                    ->GetWidget()
                              : static_cast<AccountChooserDialogView*>(
                                    controller()->current_account_chooser())
                                    ->GetWidget();
  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  EXPECT_CALL(*controller(), OnDialogClosed());
  widget->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
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
  views::DialogDelegate* dialog =
      IsParamFeatureEnabled()
          ? static_cast<views::DialogDelegate*>(
                static_cast<PasswordCombinedSelectorView*>(
                    controller()->current_account_chooser()))
          : static_cast<views::DialogDelegate*>(
                static_cast<AccountChooserDialogView*>(
                    controller()->current_account_chooser()));
  views::test::WidgetDestroyedWaiter bubble_observer(dialog->GetWidget());
  EXPECT_CALL(*this, OnChooseCredential(Pointee(form)));
  dialog->Accept();
  bubble_observer.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
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
  EXPECT_CALL(*this, OnChooseCredential(Pointee(form)));
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  EXPECT_TRUE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       PopupAccountChooserWithDisabledAutoSignin) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));
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
  EXPECT_CALL(*this, OnChooseCredential(Pointee(form)));
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  controller()->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because the setting is off.
  EXPECT_FALSE(controller()->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, PopupAccountChooserInIncognito) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
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
            controller(incognito)->GetState());
  EXPECT_TRUE(controller(incognito)->current_account_chooser());

  EXPECT_CALL(*this, OnChooseCredential(Pointee(form)));
  controller(incognito)->ChooseCredential(
      form, password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  // The first run experience isn't shown because of Incognito.
  EXPECT_FALSE(controller(incognito)->current_autosignin_prompt());
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, EscCancelsAutoSigninPrompt) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));
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
          browser()->GetProfile()->GetPrefs()));
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, PopupCredentialsLeakedPrompt) {
  CredentialLeakType leak_type = CredentialLeakFlags::kPasswordSaved |
                                 CredentialLeakFlags::kPasswordUsedOnOtherSites;
  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      leak_type,
      CreatePasswordForm(GURL("https://example.com"), u"Eve", u"qwerty"),
      /*in_account_store=*/false));
  ASSERT_TRUE(controller()->current_credential_leak_widget());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  views::Widget* dialog = controller()->current_credential_leak_widget();
  views::test::WidgetDestroyedWaiter bubble_observer(dialog);
  ui::Accelerator esc(ui::VKEY_ESCAPE, 0);
  EXPECT_TRUE(dialog->client_view()->AcceleratorPressed(esc));
  bubble_observer.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       PopupAutoSigninPromptAfterBlockedZeroclick) {
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowAutoSignInPromptFirstRunExperience(
          browser()->GetProfile()->GetPrefs()));

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
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
  blocked_form = std::make_unique<password_manager::PasswordForm>(form);
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_FALSE(controller()->current_autosignin_prompt());
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, true);

  // Successful login with the same form after block will *prompt:
  blocked_form = std::make_unique<password_manager::PasswordForm>(form);
  client()->NotifyUserCouldBeAutoSignedIn(std::move(blocked_form));
  client()->NotifySuccessfulLoginWithExistingPassword(WrapFormInManager(&form));
  ASSERT_TRUE(controller()->current_autosignin_prompt());
}

// DialogBrowserTest methods for interactive dialog invocation.
void PasswordDialogViewTest::ShowUi(const std::string& name) {
  if (name == "RemoteActorSingle" || name == "RemoteActorMultiple") {
    remote_actor_mock_controller_ = std::make_unique<
        testing::NiceMock<CredentialManagerDialogControllerMock>>();

    EXPECT_CALL(*remote_actor_mock_controller_, GetDisplayType())
        .WillRepeatedly(Return(
            PasswordCombinedSelectorController::DisplayType::kRemoteActor));
    EXPECT_CALL(*remote_actor_mock_controller_, ShouldShowTopIllustration())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*remote_actor_mock_controller_, GetTitle())
        .WillRepeatedly(Return(
            u"Allow Gemini Spark to sign in to terracottaand.co for you?"));
    EXPECT_CALL(*remote_actor_mock_controller_, GetSubtitle())
        .WillRepeatedly(
            Return(u"Spark can use Google Password Manager to sign in "
                   u"for you. Learn how Spark handles your data"));
    EXPECT_CALL(*remote_actor_mock_controller_, GetOkButtonLabel())
        .WillRepeatedly(Return(u"Allow this time"));

    remote_actor_forms_.clear();
    auto form1 = std::make_unique<password_manager::PasswordForm>();
    form1->username_value = u"peter@pan.test";
    form1->password_value = u"I can fly!";
    form1->match_type = password_manager::PasswordForm::MatchType::kExact;
    remote_actor_forms_.push_back(std::move(form1));

    if (name == "RemoteActorMultiple") {
      auto form2 = std::make_unique<password_manager::PasswordForm>();
      form2->username_value = u"notpeter@pan.test";
      form2->password_value = u"I cannot fly!";
      form2->match_type = password_manager::PasswordForm::MatchType::kExact;
      remote_actor_forms_.push_back(std::move(form2));
    }

    EXPECT_CALL(*remote_actor_mock_controller_, GetLocalForms())
        .WillRepeatedly(ReturnRef(remote_actor_forms_));
    EXPECT_CALL(*remote_actor_mock_controller_, GetOrigin())
        .WillRepeatedly(
            Return(url::Origin::Create(GURL("https://terracottaand.co"))));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    remote_actor_view_ = std::make_unique<PasswordCombinedSelectorView>(
        remote_actor_mock_controller_.get(), web_contents);
    remote_actor_view_->ShowAccountChooser();
    return;
  }

  if (name == "AutoSigninFirstRun") {
    controller()->OnPromptEnableAutoSignin();
    return;
  }

  GURL origin("https://example.com");
  if (name == "CredentialLeak") {
    CredentialLeakType leak_type =
        CredentialLeakFlags::kPasswordSaved |
        CredentialLeakFlags::kPasswordUsedOnOtherSites;

    controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
        leak_type, CreatePasswordForm(origin, u"Eve", u"qwerty"),
        /*in_account_store=*/false));
    return;
  }

  if (name == "ManyCredentials") {
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials;
    for (int i = 0; i < 5; ++i) {
      password_manager::PasswordForm form;
      form.url = GURL("https://example.com");
      form.signon_realm = form.url.GetWithEmptyPath().spec();
      form.display_name = base::ASCIIToUTF16(base::StringPrintf("User %d", i));
      form.username_value =
          base::ASCIIToUTF16(base::StringPrintf("user%d@example.com", i));
      form.match_type = password_manager::PasswordForm::MatchType::kExact;
      local_credentials.push_back(
          std::make_unique<password_manager::PasswordForm>(form));
    }
    SetupChooseCredentials(std::move(local_credentials),
                           url::Origin::Create(GURL("https://example.com")));
    return;
  }
  if (name == "FederatedCredentials") {
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials;
    password_manager::PasswordForm form;
    form.url = GURL("https://example.com");
    form.signon_realm = form.url.GetWithEmptyPath().spec();
    form.display_name = u"Peter Pan";
    form.username_value = u"peter@pan.test";
    form.federation_origin =
        url::SchemeHostPort(GURL("https://google.com/federation"));
    form.match_type = password_manager::PasswordForm::MatchType::kExact;
    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));

    form.display_name = u"Wendy Darling";
    form.username_value = u"wendy@pan.test";
    form.federation_origin =
        url::SchemeHostPort(GURL("https://example.com/federation"));
    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));

    SetupChooseCredentials(std::move(local_credentials),
                           url::Origin::Create(GURL("https://example.com")));
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
  } else if (name == "MultipleCredentials" || name == "SingleCredential") {
    form.url = origin;
    form.display_name = kFirstDisplayName;
    form.username_value = kFirstUsername;
    form.match_type = password_manager::PasswordForm::MatchType::kExact;

    local_credentials.push_back(
        std::make_unique<password_manager::PasswordForm>(form));

    if (name == "MultipleCredentials") {
      form.username_value = kSecondUsername;
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

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_AutoSigninFirstRun) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_CredentialLeak) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_PopupAutoSigninPrompt) {
  if (IsParamFeatureEnabled()) {
    // With Unified UI, OnAutoSignin shows a toast instead of a bubble.
    GTEST_SKIP() << "Unified UI shows a toast instead of a bubble";
  }
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithSingleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(
    PasswordDialogViewTest,
    InvokeUi_PopupAccountChooserWithMultipleCredentialClickSignIn) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_ManyCredentials) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_FederatedCredentials) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_RemoteActorSingle) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, InvokeUi_RemoteActorMultiple) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, ShowMultipleCredentials) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  ShowUi("MultipleCredentials");

  PasswordCombinedSelectorView* view =
      static_cast<PasswordCombinedSelectorView*>(
          controller()->current_account_chooser());
  ASSERT_TRUE(view);

  EXPECT_CALL(*this, OnChooseCredential(Pointee(
                         Field(&password_manager::PasswordForm::username_value,
                               Eq(kFirstUsername)))));
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->Accept();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, ChooseSecondCredential) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  ShowUi("MultipleCredentials");

  PasswordCombinedSelectorView* view =
      static_cast<PasswordCombinedSelectorView*>(
          controller()->current_account_chooser());
  ASSERT_TRUE(view);

  std::vector<views::RadioButton*> radio_buttons =
      GetRadioButtons(view->GetWidget()->GetContentsView());
  ASSERT_EQ(2u, radio_buttons.size());

  // Click the second radio button.
  radio_buttons[1]->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  radio_buttons[1]->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_CALL(*this, OnChooseCredential(Pointee(
                         Field(&password_manager::PasswordForm::username_value,
                               Eq(kSecondUsername)))));
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->Accept();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       ShowCombinedSelectorWithSingleCredential) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  ShowUi("SingleCredential");

  PasswordCombinedSelectorView* view =
      static_cast<PasswordCombinedSelectorView*>(
          controller()->current_account_chooser());
  ASSERT_TRUE(view);

  // No radio buttons should be shown for a single credential.
  EXPECT_TRUE(GetRadioButtons(view->GetWidget()->GetContentsView()).empty());

  EXPECT_CALL(*this, OnChooseCredential(Pointee(
                         Field(&password_manager::PasswordForm::username_value,
                               Eq(kFirstUsername)))));
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->Accept();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest, CancelCombinedSelectorDialog) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  ShowUi("MultipleCredentials");

  PasswordCombinedSelectorView* view =
      static_cast<PasswordCombinedSelectorView*>(
          controller()->current_account_chooser());
  ASSERT_TRUE(view);

  EXPECT_CALL(*this, OnChooseCredential(nullptr));
  views::test::WidgetDestroyedWaiter waiter(view->GetWidget());
  view->GetWidget()->Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       PopupAccountChooserWithRemoteActorSingle) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  CredentialManagerDialogControllerMock mock_controller;

  // 1. Setup mock expectations
  EXPECT_CALL(mock_controller, GetDisplayType())
      .WillRepeatedly(Return(
          PasswordCombinedSelectorController::DisplayType::kRemoteActor));
  EXPECT_CALL(mock_controller, ShouldShowTopIllustration())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_controller, OnCloseDialog());

  std::u16string expected_title =
      u"Allow Gemini Spark to sign in to terracottaand.co for you?";
  std::u16string expected_subtitle =
      u"Spark can use Google Password Manager to sign in for you. Because "
      u"Spark is experimental, this carries security risks.";
  std::u16string expected_ok_button = u"Allow this time";

  EXPECT_CALL(mock_controller, GetTitle())
      .WillRepeatedly(Return(expected_title));
  EXPECT_CALL(mock_controller, GetSubtitle())
      .WillRepeatedly(Return(expected_subtitle));
  EXPECT_CALL(mock_controller, GetOkButtonLabel())
      .WillRepeatedly(Return(expected_ok_button));

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto form = std::make_unique<password_manager::PasswordForm>();
  form->username_value = u"peter@pan.test";
  form->password_value = u"I can fly!";
  form->match_type = password_manager::PasswordForm::MatchType::kExact;
  forms.push_back(std::move(form));

  EXPECT_CALL(mock_controller, GetLocalForms())
      .WillRepeatedly(ReturnRef(forms));

  // 2. Instantiate and show the view
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto view = std::make_unique<PasswordCombinedSelectorView>(&mock_controller,
                                                             web_contents);
  // ShowAccountChooser internally creates the widget and maps it.
  view->ShowAccountChooser();
  views::Widget* widget = view->GetWidget();
  ASSERT_TRUE(widget);

  // 3. Verify labels and title
  EXPECT_EQ(widget->widget_delegate()->GetWindowTitle(), expected_title);
  views::Label* subtitle_label =
      GetLabelByID(widget->GetContentsView(),
                   PasswordCombinedSelectorView::kSubtitleLabelId);
  ASSERT_TRUE(subtitle_label);
  EXPECT_EQ(subtitle_label->GetText(), expected_subtitle);
  EXPECT_EQ(view->GetOkButton()->GetText(), expected_ok_button);

  views::BubbleFrameView* frame_view = view->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
  EXPECT_NE(frame_view->GetHeaderViewForTesting(), nullptr);
#else
  EXPECT_EQ(frame_view->GetHeaderViewForTesting(), nullptr);
#endif

  // Verify row labels (should use raw username and 8 password dots)
  std::vector<views::View*> rows =
      GetViewsByID(PasswordCombinedSelectorView::kCredentialRowId,
                   widget->GetContentsView());
  ASSERT_EQ(rows.size(), 1u);

  views::Label* username_label =
      GetLabelByID(rows[0], PasswordCombinedSelectorView::kRowUsernameLabelId);
  ASSERT_TRUE(username_label);
  EXPECT_EQ(username_label->GetText(), u"peter@pan.test");

  std::vector<views::View*> details =
      GetViewsByID(PasswordCombinedSelectorView::kRowDetailLabelId, rows[0]);
  ASSERT_GE(details.size(), 1u);
  EXPECT_EQ(static_cast<views::Label*>(details[0])->GetText(), u"••••••••");

  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       PopupAccountChooserWithRemoteActorMultiple) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  CredentialManagerDialogControllerMock mock_controller;

  // 1. Setup mock expectations
  EXPECT_CALL(mock_controller, GetDisplayType())
      .WillRepeatedly(Return(
          PasswordCombinedSelectorController::DisplayType::kRemoteActor));
  EXPECT_CALL(mock_controller, ShouldShowTopIllustration())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_controller, OnCloseDialog());

  std::u16string expected_title =
      u"Allow Gemini Spark to sign in to terracottaand.co for you?";
  std::u16string expected_subtitle =
      u"Spark can use Google Password Manager to sign in for you. Because "
      u"Spark is experimental, this carries security risks.";
  std::u16string expected_ok_button = u"Allow this time";

  EXPECT_CALL(mock_controller, GetTitle())
      .WillRepeatedly(Return(expected_title));
  EXPECT_CALL(mock_controller, GetSubtitle())
      .WillRepeatedly(Return(expected_subtitle));
  EXPECT_CALL(mock_controller, GetOkButtonLabel())
      .WillRepeatedly(Return(expected_ok_button));

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto form1 = std::make_unique<password_manager::PasswordForm>();
  form1->username_value = u"peter@pan.test";
  form1->password_value = u"I can fly!";
  form1->match_type = password_manager::PasswordForm::MatchType::kExact;
  forms.push_back(std::move(form1));

  auto form2 = std::make_unique<password_manager::PasswordForm>();
  form2->username_value = u"notpeter@pan.test";
  form2->password_value = u"I cannot fly!";
  form2->match_type = password_manager::PasswordForm::MatchType::kExact;
  forms.push_back(std::move(form2));

  EXPECT_CALL(mock_controller, GetLocalForms())
      .WillRepeatedly(ReturnRef(forms));

  // 2. Instantiate and show the view
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto view = std::make_unique<PasswordCombinedSelectorView>(&mock_controller,
                                                             web_contents);
  // ShowAccountChooser internally creates the widget and maps it.
  view->ShowAccountChooser();
  views::Widget* widget = view->GetWidget();
  ASSERT_TRUE(widget);

  // 3. Verify labels and title
  EXPECT_EQ(widget->widget_delegate()->GetWindowTitle(), expected_title);
  views::Label* subtitle_label =
      GetLabelByID(widget->GetContentsView(),
                   PasswordCombinedSelectorView::kSubtitleLabelId);
  ASSERT_TRUE(subtitle_label);
  EXPECT_EQ(subtitle_label->GetText(), expected_subtitle);
  EXPECT_EQ(view->GetOkButton()->GetText(), expected_ok_button);

  views::BubbleFrameView* frame_view = view->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)  // nocheck
  EXPECT_NE(frame_view->GetHeaderViewForTesting(), nullptr);
#else
  EXPECT_EQ(frame_view->GetHeaderViewForTesting(), nullptr);
#endif

  // Verify multiple rows radio button list is initialized
  EXPECT_EQ(GetRadioButtons(widget->GetContentsView()).size(), 2u);

  // Verify row labels
  std::vector<views::View*> rows =
      GetViewsByID(PasswordCombinedSelectorView::kCredentialRowId,
                   widget->GetContentsView());
  ASSERT_EQ(rows.size(), 2u);

  // Row 0
  views::Label* username_label1 =
      GetLabelByID(rows[0], PasswordCombinedSelectorView::kRowUsernameLabelId);
  ASSERT_TRUE(username_label1);
  EXPECT_EQ(username_label1->GetText(), u"peter@pan.test");

  std::vector<views::View*> details1 =
      GetViewsByID(PasswordCombinedSelectorView::kRowDetailLabelId, rows[0]);
  ASSERT_GE(details1.size(), 1u);
  EXPECT_EQ(static_cast<views::Label*>(details1[0])->GetText(), u"••••••••");

  // Row 1
  views::Label* username_label2 =
      GetLabelByID(rows[1], PasswordCombinedSelectorView::kRowUsernameLabelId);
  ASSERT_TRUE(username_label2);
  EXPECT_EQ(username_label2->GetText(), u"notpeter@pan.test");

  std::vector<views::View*> details2 =
      GetViewsByID(PasswordCombinedSelectorView::kRowDetailLabelId, rows[1]);
  ASSERT_GE(details2.size(), 1u);
  EXPECT_EQ(static_cast<views::Label*>(details2[0])->GetText(), u"••••••••");

  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_P(PasswordDialogViewTest,
                       PopupAccountChooserWithRemoteActorNonExactMatch) {
  if (!IsParamFeatureEnabled()) {
    return;
  }
  CredentialManagerDialogControllerMock mock_controller;

  // 1. Setup mock expectations
  EXPECT_CALL(mock_controller, GetDisplayType())
      .WillRepeatedly(Return(
          PasswordCombinedSelectorController::DisplayType::kRemoteActor));
  EXPECT_CALL(mock_controller, ShouldShowTopIllustration())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_controller, OnCloseDialog());

  std::u16string expected_title =
      u"Allow Gemini Spark to sign in to terracottaand.co for you?";
  std::u16string expected_subtitle =
      u"Spark can use Google Password Manager to sign in for you. Because "
      u"Spark is experimental, this carries security risks.";
  std::u16string expected_ok_button = u"Allow this time";

  EXPECT_CALL(mock_controller, GetTitle())
      .WillRepeatedly(Return(expected_title));
  EXPECT_CALL(mock_controller, GetSubtitle())
      .WillRepeatedly(Return(expected_subtitle));
  EXPECT_CALL(mock_controller, GetOkButtonLabel())
      .WillRepeatedly(Return(expected_ok_button));

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto form = std::make_unique<password_manager::PasswordForm>();
  form->url = GURL("https://m.terracottaand.co");
  form->username_value = u"peter@pan.test";
  form->password_value = u"I can fly!";
  form->match_type = password_manager::PasswordForm::MatchType::kPSL;
  forms.push_back(std::move(form));

  EXPECT_CALL(mock_controller, GetLocalForms())
      .WillRepeatedly(ReturnRef(forms));

  // 2. Instantiate and show the view
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto view = std::make_unique<PasswordCombinedSelectorView>(&mock_controller,
                                                             web_contents);
  // ShowAccountChooser internally creates the widget and maps it.
  view->ShowAccountChooser();
  views::Widget* widget = view->GetWidget();
  ASSERT_TRUE(widget);

  // 3. Verify row labels (should use raw username, 8 password dots, and origin)
  std::vector<views::View*> rows =
      GetViewsByID(PasswordCombinedSelectorView::kCredentialRowId,
                   widget->GetContentsView());
  ASSERT_EQ(rows.size(), 1u);

  views::Label* username_label =
      GetLabelByID(rows[0], PasswordCombinedSelectorView::kRowUsernameLabelId);
  ASSERT_TRUE(username_label);
  EXPECT_EQ(username_label->GetText(), u"peter@pan.test");

  std::vector<views::View*> details =
      GetViewsByID(PasswordCombinedSelectorView::kRowDetailLabelId, rows[0]);
  ASSERT_EQ(details.size(), 2u);
  EXPECT_EQ(static_cast<views::Label*>(details[0])->GetText(), u"••••••••");
  EXPECT_EQ(static_cast<views::Label*>(details[1])->GetText(),
            u"m.terracottaand.co");

  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PasswordDialogViewTest);

}  // namespace
