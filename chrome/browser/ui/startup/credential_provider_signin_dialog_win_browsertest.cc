// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/buildflags.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/test/widget_test.h"

#define TEST_TASK_FUNC_NAME_FOR_THREAD_RUNNER(Func) Run##Func##Test

class CredentialProviderSigninDialogWinIntegrationTest;

class SigninDialogLoadingStoppedObserver : public content::WebContentsObserver {
 public:
  SigninDialogLoadingStoppedObserver(content::WebContents* web_contents,
                                     base::OnceClosure idle_closure)
      : content::WebContentsObserver(web_contents),
        idle_closure_(std::move(idle_closure)) {}

  void DidStopLoading() override {
    if (idle_closure_)
      std::move(idle_closure_).Run();
  }

  base::OnceClosure idle_closure_;
};

class CredentialProviderSigninDialogWinBaseTest : public InProcessBrowserTest {
 public:
  CredentialProviderSigninDialogWinBaseTest(
      const CredentialProviderSigninDialogWinBaseTest&) = delete;
  CredentialProviderSigninDialogWinBaseTest& operator=(
      const CredentialProviderSigninDialogWinBaseTest&) = delete;

 protected:
  CredentialProviderSigninDialogWinBaseTest();

  content::WebContents* web_contents() { return web_contents_; }
  virtual void WaitForDialogToLoad();

  raw_ptr<views::WebDialogView, AcrossTasksDanglingUntriaged> web_view_ =
      nullptr;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
};

CredentialProviderSigninDialogWinBaseTest::
    CredentialProviderSigninDialogWinBaseTest()
    : InProcessBrowserTest() {}

void CredentialProviderSigninDialogWinBaseTest::WaitForDialogToLoad() {
  EXPECT_TRUE(web_contents());

  base::RunLoop run_loop;
  SigninDialogLoadingStoppedObserver observer(web_contents(),
                                              run_loop.QuitWhenIdleClosure());
  run_loop.Run();
}

///////////////////////////////////////////////////////////////////////////////
// CredentialProviderSigninDialogWinDialogTest tests the dialog portion of the
// credential provider sign in without checking whether the fetch of additional
// information was successful.

class CredentialProviderSigninDialogWinDialogTest
    : public CredentialProviderSigninDialogWinBaseTest {
 public:
  CredentialProviderSigninDialogWinDialogTest(
      const CredentialProviderSigninDialogWinDialogTest&) = delete;
  CredentialProviderSigninDialogWinDialogTest& operator=(
      const CredentialProviderSigninDialogWinDialogTest&) = delete;

 protected:
  CredentialProviderSigninDialogWinDialogTest();

  void SendSigninCompleteMessage(const base::Value::Dict& value);
  void SendValidSigninCompleteMessage();
  void WaitForSigninCompleteMessage();

  void ShowSigninDialog(const base::CommandLine& command_line);

  // A HandleGCPWSiginCompleteResult callback to check that the signin dialog
  // has correctly received and procesed the sign in complete message.
  void HandleSignInComplete(
      base::Value::Dict signin_result,
      const std::string& additional_mdm_oauth_scopes,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader);
  bool signin_complete_called_ = false;

  std::string result_access_token_;
  std::string result_refresh_token_;
  std::string additional_mdm_oauth_scopes_;
  int exit_code_;
  base::Value::Dict result_dict_;
  CredentialProviderSigninDialogTestDataStorage test_data_storage_;

 private:
  base::OnceClosure signin_complete_closure_;
};

CredentialProviderSigninDialogWinDialogTest::
    CredentialProviderSigninDialogWinDialogTest()
    : CredentialProviderSigninDialogWinBaseTest() {}

void CredentialProviderSigninDialogWinDialogTest::SendSigninCompleteMessage(
    const base::Value::Dict& value) {
  std::string json_string;
  EXPECT_TRUE(base::JSONWriter::Write(value, &json_string));

  std::string login_complete_message =
      "chrome.send('lstFetchResults', [" + json_string + "]);";
  content::RenderFrameHost* root = web_contents()->GetPrimaryMainFrame();
  content::ExecuteScriptAsync(root, login_complete_message);
  WaitForSigninCompleteMessage();
}

void CredentialProviderSigninDialogWinDialogTest::
    SendValidSigninCompleteMessage() {
  SendSigninCompleteMessage(test_data_storage_.MakeValidSignInResponseValue());
}

void CredentialProviderSigninDialogWinDialogTest::
    WaitForSigninCompleteMessage() {
  // Run until the dialog has received the signin complete message.
  base::RunLoop run_loop;
  signin_complete_closure_ = run_loop.QuitWhenIdleClosure();
  run_loop.Run();
}

void CredentialProviderSigninDialogWinDialogTest::ShowSigninDialog(
    const base::CommandLine& command_line) {
  web_view_ = ShowCredentialProviderSigninDialog(
      command_line, browser()->profile(),
      base::BindOnce(
          &CredentialProviderSigninDialogWinDialogTest::HandleSignInComplete,
          base::Unretained(this)));
  web_contents_ = web_view_->web_contents();
}

void CredentialProviderSigninDialogWinDialogTest::HandleSignInComplete(
    base::Value::Dict signin_result,
    const std::string& additional_mdm_oauth_scopes,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader) {
  additional_mdm_oauth_scopes_ = additional_mdm_oauth_scopes;
  exit_code_ = *signin_result.FindInt(credential_provider::kKeyExitCode);
  if (exit_code_ == credential_provider::kUiecSuccess) {
    result_access_token_ =
        *signin_result.FindString(credential_provider::kKeyAccessToken);
    result_refresh_token_ =
        *signin_result.FindString(credential_provider::kKeyRefreshToken);
  }
  EXPECT_FALSE(signin_complete_called_);
  signin_complete_called_ = true;
  result_dict_ = std::move(signin_result);

  if (signin_complete_closure_)
    std::move(signin_complete_closure_).Run();
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       ShowTosInUrlParams) {
  base::CommandLine command_line =
      base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM);
  // Append show_tos switch and verify if the tos is part of URL.
  const std::string show_tos = "1";
  command_line.AppendSwitchASCII(::credential_provider::kShowTosSwitch,
                                 show_tos);
  ShowSigninDialog(command_line);
  WaitForDialogToLoad();

  EXPECT_TRUE(web_view_->GetDialogContentURL().has_query());
  std::string query_parameters = web_view_->GetDialogContentURL().query();
  EXPECT_TRUE(query_parameters.find("show_tos=1") != std::string::npos);

  web_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SimulateEscape) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();

  web_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  std::optional<int> exit_code =
      result_dict_.FindInt(credential_provider::kKeyExitCode);
  EXPECT_TRUE(exit_code);
  EXPECT_EQ(credential_provider::kUiecAbort, exit_code.value());
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       ShouldNotCreateWebContents) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();

  ASSERT_TRUE(web_view_->IsWebContentsCreationOverridden(
      nullptr /* source_site_instance */,
      content::mojom::WindowContainerType::NORMAL /* window_container_type */,
      GURL() /* opener_url */, "foo" /* frame_name */,
      GURL() /* target_url */));

  web_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendEmptySigninComplete) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue());
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoId) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      std::string(), test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoPassword) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(), std::string(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoEmail) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(), std::string(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoAccessToken) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(), std::string(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoRefreshToken) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(), std::string()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(result_dict_.size(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SuccessfulLoginMessage) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  SendValidSigninCompleteMessage();
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_GT(result_dict_.size(), 1u);
  const std::string* id_in_dict =
      result_dict_.FindString(credential_provider::kKeyId);
  ASSERT_NE(id_in_dict, nullptr);
  const std::string* email_in_dict =
      result_dict_.FindString(credential_provider::kKeyEmail);
  ASSERT_NE(email_in_dict, nullptr);
  const std::string* password_in_dict =
      result_dict_.FindString(credential_provider::kKeyPassword);
  ASSERT_NE(password_in_dict, nullptr);

  EXPECT_EQ(*id_in_dict, test_data_storage_.GetSuccessId());
  EXPECT_EQ(*email_in_dict, test_data_storage_.GetSuccessEmail());
  EXPECT_EQ(*password_in_dict, test_data_storage_.GetSuccessPassword());
  EXPECT_EQ(result_access_token_, test_data_storage_.GetSuccessAccessToken());
  EXPECT_EQ(result_refresh_token_, test_data_storage_.GetSuccessRefreshToken());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SuccessfulLoginMessageWithAdditionalOauthScopes) {
  const std::string additional_oauth_scopes_flag_value = "dummy_scopes";
  base::CommandLine command_line =
      base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      credential_provider::kGcpwAdditionalOauthScopes,
      additional_oauth_scopes_flag_value);
  ShowSigninDialog(command_line);
  WaitForDialogToLoad();
  SendValidSigninCompleteMessage();
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(additional_oauth_scopes_flag_value, additional_mdm_oauth_scopes_);
}

// Tests the various exit codes for success / failure.
class CredentialProviderSigninDialogWinDialogExitCodeTest
    : public CredentialProviderSigninDialogWinDialogTest,
      public ::testing::WithParamInterface<int> {};

IN_PROC_BROWSER_TEST_P(CredentialProviderSigninDialogWinDialogExitCodeTest,
                       SigninResultWithExitCode) {
  ShowSigninDialog(base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM));
  WaitForDialogToLoad();
  base::Value::Dict signin_result =
      test_data_storage_.MakeValidSignInResponseValue();

  int expected_error_code = GetParam();
  bool should_succeed = expected_error_code ==
                        static_cast<int>(credential_provider::kUiecSuccess);
  signin_result.Set(credential_provider::kKeyExitCode, expected_error_code);

  SendSigninCompleteMessage(signin_result);
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_EQ(exit_code_, expected_error_code);
  std::optional<int> exit_code_value =
      result_dict_.FindInt(credential_provider::kKeyExitCode);
  EXPECT_EQ(exit_code_value, expected_error_code);

  if (should_succeed) {
    EXPECT_GT(result_dict_.size(), 1u);

    std::string id_in_dict =
        *result_dict_.FindString(credential_provider::kKeyId);
    std::string email_in_dict =
        *result_dict_.FindString(credential_provider::kKeyEmail);
    std::string password_in_dict =
        *result_dict_.FindString(credential_provider::kKeyPassword);

    EXPECT_EQ(id_in_dict, test_data_storage_.GetSuccessId());
    EXPECT_EQ(email_in_dict, test_data_storage_.GetSuccessEmail());
    EXPECT_EQ(password_in_dict, test_data_storage_.GetSuccessPassword());
    EXPECT_EQ(result_access_token_, test_data_storage_.GetSuccessAccessToken());
    EXPECT_EQ(result_refresh_token_,
              test_data_storage_.GetSuccessRefreshToken());
  } else {
    EXPECT_EQ(result_dict_.size(), 1u);
    EXPECT_TRUE(result_access_token_.empty());
    EXPECT_TRUE(result_refresh_token_.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CredentialProviderSigninDialogWinDialogExitCodeTest,
    ::testing::Range(0, static_cast<int>(credential_provider::kUiecCount)));

///////////////////////////////////////////////////////////////////////////////
// CredentialProviderSigninDialogWinIntegrationTest is used for testing
// the integration of the dialog into Chrome, This test mainly verifies
// correct start up state if we provide the --gcpw-signin switch.

class CredentialProviderSigninDialogWinIntegrationTestBase
    : public CredentialProviderSigninDialogWinBaseTest {
 public:
  CredentialProviderSigninDialogWinIntegrationTestBase(
      const CredentialProviderSigninDialogWinIntegrationTestBase&) = delete;
  CredentialProviderSigninDialogWinIntegrationTestBase& operator=(
      const CredentialProviderSigninDialogWinIntegrationTestBase&) = delete;

 protected:
  CredentialProviderSigninDialogWinIntegrationTestBase();
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

CredentialProviderSigninDialogWinIntegrationTestBase::
    CredentialProviderSigninDialogWinIntegrationTestBase()
    : CredentialProviderSigninDialogWinBaseTest() {}

void CredentialProviderSigninDialogWinIntegrationTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(::credential_provider::kGcpwSigninSwitch);
}

// In the default state, the dialog would not be allowed to be displayed since
// Chrome will not be running on Winlogon desktop.
class CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest
    : public CredentialProviderSigninDialogWinIntegrationTestBase {
 public:
  CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest(
      const CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest&) =
      delete;
  CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest&
  operator=(
      const CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest&) =
      delete;

 protected:
  CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest();
};

CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest::
    CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest()
    : CredentialProviderSigninDialogWinIntegrationTestBase() {}

IN_PROC_BROWSER_TEST_F(
    CredentialProviderSigninDialogWinIntegrationDesktopVerificationTest,
    DISABLED_DialogFailsToLoadOnIncorrectDesktop) {
  // Normally the GCPW signin dialog should only run on "winlogon" desktops. If
  // we are just running the test, we should not be under this desktop and the
  // dialog should fail to load.

  // No widgets should have been created.
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  EXPECT_EQ(all_widgets.size(), 0ull);
}

#if BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)

// This test overrides the check for the correct desktop to allow the dialog to
// be displayed even when not running on Winlogon desktop.
class CredentialProviderSigninDialogWinIntegrationDialogDisplayTest
    : public CredentialProviderSigninDialogWinIntegrationTestBase {
 public:
  CredentialProviderSigninDialogWinIntegrationDialogDisplayTest(
      const CredentialProviderSigninDialogWinIntegrationDialogDisplayTest&) =
      delete;
  CredentialProviderSigninDialogWinIntegrationDialogDisplayTest& operator=(
      const CredentialProviderSigninDialogWinIntegrationDialogDisplayTest&) =
      delete;

 protected:
  CredentialProviderSigninDialogWinIntegrationDialogDisplayTest();
  ~CredentialProviderSigninDialogWinIntegrationDialogDisplayTest() override;

  // CredentialProviderSigninDialogWinBaseTest:
  void WaitForDialogToLoad() override;
};

CredentialProviderSigninDialogWinIntegrationDialogDisplayTest::
    CredentialProviderSigninDialogWinIntegrationDialogDisplayTest()
    : CredentialProviderSigninDialogWinIntegrationTestBase() {
  EnableGcpwSigninDialogForTesting(true);
}

CredentialProviderSigninDialogWinIntegrationDialogDisplayTest::
    ~CredentialProviderSigninDialogWinIntegrationDialogDisplayTest() {
  EnableGcpwSigninDialogForTesting(false);
}

void CredentialProviderSigninDialogWinIntegrationDialogDisplayTest::
    WaitForDialogToLoad() {
  // The browser has already been created by the time this start starts and
  // web_contents_ is not yet available. In this run case there should only
  // be one widget available and that widget should contain the web contents
  // needed for the test.
  EXPECT_FALSE(web_contents_);
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  EXPECT_EQ(all_widgets.size(), 1ull);
  views::WebDialogView* web_dialog = static_cast<views::WebDialogView*>(
      (*all_widgets.begin())->GetContentsView());
  web_contents_ = web_dialog->web_contents();
  EXPECT_TRUE(web_contents_);

  CredentialProviderSigninDialogWinIntegrationTestBase::WaitForDialogToLoad();

  // When running with --gcpw-signin, browser creation is completely bypassed
  // only a dialog for the signin should be created directly. In a normal
  // browser test, there is always a browser created so make sure that is not
  // the case for our tests.
  EXPECT_FALSE(browser());
}

IN_PROC_BROWSER_TEST_F(
    CredentialProviderSigninDialogWinIntegrationDialogDisplayTest,
    ShowDialogOnlyTest) {
  WaitForDialogToLoad();
  EXPECT_TRUE(reinterpret_cast<Profile*>(web_contents_->GetBrowserContext())
                  ->IsIncognitoProfile());
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  (*all_widgets.begin())->Close();
  RunUntilBrowserProcessQuits();
}

IN_PROC_BROWSER_TEST_F(
    CredentialProviderSigninDialogWinIntegrationDialogDisplayTest,
    EscapeClosesDialogTest) {
  WaitForDialogToLoad();
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  ui::KeyEvent escape_key_event(ui::EventType::kKeyPressed,
                                ui::KeyboardCode::VKEY_ESCAPE,
                                ui::DomCode::ESCAPE, 0);
  (*all_widgets.begin())->OnKeyEvent(&escape_key_event);
  RunUntilBrowserProcessQuits();
}

#endif  // BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
