// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/syslog_logging.h"
#include "base/win/win_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/startup/credential_provider_signin_info_fetcher_win.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/url_util.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace {

#if BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
bool g_enable_gcpw_signin_during_tests = false;
#endif  // BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)

// This message must match the one sent in inline_login.js: sendLSTFetchResults.
constexpr char kLSTFetchResultsMessage[] = "lstFetchResults";

void WriteResultToHandle(const base::Value& result) {
  std::string json_result;
  if (base::JSONWriter::Write(result, &json_result) && !json_result.empty()) {
    // The caller of this Chrome process must provide a stdout handle  to
    // which Chrome can output the results, otherwise by default the call to
    // ::GetStdHandle(STD_OUTPUT_HANDLE) will result in an invalid or null
    // handle if Chrome was started without providing a console.
    HANDLE output_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (output_handle != nullptr && output_handle != INVALID_HANDLE_VALUE) {
      DWORD written;
      if (!::WriteFile(output_handle, json_result.c_str(), json_result.length(),
                       &written, nullptr)) {
        SYSLOG(ERROR)
            << "Failed to write result of GCPW signin to inherited handle.";
      }
    }
  }
}

void WriteResultToHandleWithKeepAlive(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    base::Value signin_result) {
  WriteResultToHandle(signin_result);

  // Release the keep_alive implicitly and allow the dialog to die.
}

void HandleAllGcpwInfoFetched(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<CredentialProviderSigninInfoFetcher> fetcher,
    base::Value signin_result,
    base::Value fetch_result) {
  DCHECK(signin_result.is_dict());
  DCHECK(fetch_result.is_dict());
  if (!signin_result.DictEmpty() && !fetch_result.DictEmpty()) {
    signin_result.MergeDictionary(&fetch_result);
    WriteResultToHandle(signin_result);
  }

  // Release the fetcher and mark it for eventual delete. It is not immediately
  // deleted here in case it still wants to do further processing after
  // returning from this callback
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, fetcher.release());

  // Release the keep_alive implicitly and allow the dialog to die.
}

void HandleSigninCompleteForGcpwLogin(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    base::Value signin_result,
    const std::string& additional_mdm_oauth_scopes,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(signin_result.is_dict());
  DCHECK(!signin_result.DictEmpty());
  int exit_code = signin_result
                      .FindKeyOfType(credential_provider::kKeyExitCode,
                                     base::Value::Type::INTEGER)
                      ->GetInt();

  // If there is an error code, write out the signin results directly.
  // Otherwise fetch more info required for the signin.  In either case,
  // make sure the keep alive is not destroyed on return of this function
  // or a reentrancy crash will occur in HWNDMessageHandler().
  if (exit_code != credential_provider::kUiecSuccess) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&WriteResultToHandleWithKeepAlive, std::move(keep_alive),
                       std::move(signin_result)));
  } else if (signin_result.DictSize() > 1) {
    std::string access_token =
        signin_result
            .FindKeyOfType(credential_provider::kKeyAccessToken,
                           base::Value::Type::STRING)
            ->GetString();
    std::string refresh_token =
        signin_result
            .FindKeyOfType(credential_provider::kKeyRefreshToken,
                           base::Value::Type::STRING)
            ->GetString();
    DCHECK(!access_token.empty() && !refresh_token.empty());

    // Create the fetcher and pass it to the callback so that it can be
    // deleted once it is finished.
    auto fetcher = std::make_unique<CredentialProviderSigninInfoFetcher>(
        refresh_token, url_loader_factory);
    fetcher->SetCompletionCallbackAndStart(
        access_token, additional_mdm_oauth_scopes,
        base::BindOnce(&HandleAllGcpwInfoFetched, std::move(keep_alive),
                       std::move(fetcher), std::move(signin_result)));
  }

  // If the function has not passed ownership of the keep alive yet at this
  // point this means there was some error reading the sign in result or the
  // result was empty. In this case, return from the method which will
  // implicitly release the keep_alive which will close and release everything.
}

class CredentialProviderWebUIMessageHandler
    : public content::WebUIMessageHandler {
 public:
  explicit CredentialProviderWebUIMessageHandler(
      HandleGcpwSigninCompleteResult signin_callback,
      const std::string& additional_mdm_oauth_scopes)
      : signin_callback_(std::move(signin_callback)),
        additional_mdm_oauth_scopes_(additional_mdm_oauth_scopes) {}

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kLSTFetchResultsMessage,
        base::BindRepeating(
            &CredentialProviderWebUIMessageHandler::OnSigninComplete,
            base::Unretained(this)));

    // This message is always sent as part of the SAML flow but we don't really
    // need to process it. We do however have to handle the message or else
    // there will be a DCHECK failure in web_ui about an unhandled message.
    web_ui()->RegisterMessageCallback(
        "updatePasswordAttributes",
        base::BindRepeating([](const base::ListValue* args) {}));
  }

  void AbortIfPossible() {
    // If the callback was already called, ignore.
    if (!signin_callback_)
      return;

    // Build a result for the credential provider that includes only the abort
    // exit code.
    std::unique_ptr<base::Value> result(
        new base::Value(base::Value::Type::DICTIONARY));
    result->SetKey(credential_provider::kKeyExitCode,
                   base::Value(credential_provider::kUiecAbort));
    base::ListValue args;
    args.Append(std::move(result));
    OnSigninComplete(&args);
  }

 private:
  base::Value ParseArgs(const base::ListValue* args, int* out_exit_code) {
    DCHECK(out_exit_code);

    const base::Value* dict_result = nullptr;
    if (!args || args->empty() || !args->Get(0, &dict_result) ||
        !dict_result->is_dict()) {
      *out_exit_code = credential_provider::kUiecMissingSigninData;
      return base::Value(base::Value::Type::DICTIONARY);
    }
    const base::Value* exit_code = dict_result->FindKeyOfType(
        credential_provider::kKeyExitCode, base::Value::Type::INTEGER);

    if (exit_code && exit_code->GetInt() != credential_provider::kUiecSuccess) {
      *out_exit_code = exit_code->GetInt();
      return base::Value(base::Value::Type::DICTIONARY);
    }

    const base::Value* email = dict_result->FindKeyOfType(
        credential_provider::kKeyEmail, base::Value::Type::STRING);
    const base::Value* password = dict_result->FindKeyOfType(
        credential_provider::kKeyPassword, base::Value::Type::STRING);
    const base::Value* id = dict_result->FindKeyOfType(
        credential_provider::kKeyId, base::Value::Type::STRING);
    const base::Value* access_token = dict_result->FindKeyOfType(
        credential_provider::kKeyAccessToken, base::Value::Type::STRING);
    const base::Value* refresh_token = dict_result->FindKeyOfType(
        credential_provider::kKeyRefreshToken, base::Value::Type::STRING);

    if (!email || email->GetString().empty() || !password ||
        password->GetString().empty() || !id || id->GetString().empty() ||
        !access_token || access_token->GetString().empty() || !refresh_token ||
        refresh_token->GetString().empty()) {
      *out_exit_code = credential_provider::kUiecMissingSigninData;
      return base::Value(base::Value::Type::DICTIONARY);
    }

    *out_exit_code = credential_provider::kUiecSuccess;
    return dict_result->Clone();
  }

  void OnSigninComplete(const base::ListValue* args) {
    // If the callback was already called, ignore.  This may happen if the
    // user presses Escape right after finishing the signin process, the
    // Escape is processed first by AbortIfPossible(), and the signin then
    // completes before WriteResultToHandleWithKeepAlive() executes.
    if (!signin_callback_)
      return;

    int exit_code;
    base::Value signin_result = ParseArgs(args, &exit_code);

    signin_result.SetKey(credential_provider::kKeyExitCode,
                         base::Value(exit_code));

    content::WebContents* contents = web_ui()->GetWebContents();
    content::StoragePartition* partition =
        content::BrowserContext::GetStoragePartitionForSite(
            contents->GetBrowserContext(), signin::GetSigninPartitionURL());

    // Regardless of the results of ParseArgs, |signin_callback_| will always
    // be called to allow it to release any additional references it may hold
    // (like the keep_alive in HandleSigninCompleteForGCPWLogin) or perform
    // possible error handling.
    std::move(signin_callback_)
        .Run(std::move(signin_result), additional_mdm_oauth_scopes_,
             partition->GetURLLoaderFactoryForBrowserProcess());
  }

  HandleGcpwSigninCompleteResult signin_callback_;
  const std::string additional_mdm_oauth_scopes_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderWebUIMessageHandler);
};

}  // namespace

// Delegate to control a views::WebDialogView for purposes of showing a gaia
// sign in page for purposes of the credential provider.
class CredentialProviderWebDialogDelegate : public ui::WebDialogDelegate {
 public:
  // |reauth_email| is used to pre fill in the sign in dialog with the user's
  // e-mail during a reauthorize sign in. This type of sign in is used to update
  // the user's password.
  // |email_domains| is used to pre fill the email domain on Gaia's signin page
  // so that the user only needs to enter their user name.
  CredentialProviderWebDialogDelegate(
      const std::string& reauth_email,
      const std::string& reauth_gaia_id,
      const std::string& email_domains,
      const std::string& gcpw_endpoint_path,
      const std::string& additional_mdm_oauth_scopes,
      HandleGcpwSigninCompleteResult signin_callback)
      : reauth_email_(reauth_email),
        reauth_gaia_id_(reauth_gaia_id),
        email_domains_(email_domains),
        gcpw_endpoint_path_(gcpw_endpoint_path),
        additional_mdm_oauth_scopes(additional_mdm_oauth_scopes),
        signin_callback_(std::move(signin_callback)) {}

  GURL GetDialogContentURL() const override {
    signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_MACHINE_LOGON;
    signin_metrics::Reason reason =
        signin_metrics::Reason::REASON_FETCH_LST_ONLY;

    auto base_url =
        reauth_email_.empty()
            ? signin::GetEmbeddedPromoURL(access_point, reason, false)
            : signin::GetEmbeddedReauthURLWithEmail(access_point, reason,
                                                    reauth_email_);
    if (!reauth_gaia_id_.empty()) {
      base_url = net::AppendQueryParameter(
          base_url, credential_provider::kValidateGaiaIdSigninPromoParameter,
          reauth_gaia_id_);
    }

    if (!gcpw_endpoint_path_.empty()) {
      base_url = net::AppendQueryParameter(
          base_url, credential_provider::kGcpwEndpointPathPromoParameter,
          gcpw_endpoint_path_);
    }

    if (email_domains_.empty())
      return base_url;

    return net::AppendQueryParameter(
        base_url, credential_provider::kEmailDomainsSigninPromoParameter,
        email_domains_);
  }

  ui::ModalType GetDialogModalType() const override {
    return ui::MODAL_TYPE_SYSTEM;
  }

  base::string16 GetDialogTitle() const override { return base::string16(); }

  base::string16 GetAccessibleDialogTitle() const override {
    return base::string16();
  }

  std::string GetDialogName() const override {
    // Return an empty window name; otherwise chrome will try to persist the
    // window's position and DCHECK.
    return std::string();
  }

  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override {
    // The WebDialogUI will own and delete this message handler.
    DCHECK(!handler_);
    handler_ = new CredentialProviderWebUIMessageHandler(
        std::move(signin_callback_), additional_mdm_oauth_scopes);
    handlers->push_back(handler_);
  }

  void GetDialogSize(gfx::Size* size) const override {
    // TODO(crbug.com/901947): Figure out exactly what size the dialog should
    // be.
    size->SetSize(448, 610);
  }

  void GetMinimumDialogSize(gfx::Size* size) const override {
    GetDialogSize(size);
  }

  std::string GetDialogArgs() const override { return std::string(); }

  void OnDialogClosed(const std::string& json_retval) override {
    // To handle the case where the user presses Esc to cancel the sign in,
    // write out an "abort" result for the credential provider.  However,
    // this function is also called when the user completes the sign in
    // successfully and output has already been written.  In this case the
    // abort is not possible and this call is a noop.
    handler_->AbortIfPossible();

    // Class owns itself and thus needs to be deleted eventually after the
    // closed call back has been signalled since it will no longer be accessed
    // by the WebDialogView.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override {}

  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override {
    return true;
  }

  bool ShouldShowDialogTitle() const override { return false; }

 protected:
  // E-mail used to pre-fill the e-mail field when a reauth signin is required.
  const std::string reauth_email_;

  // Gaia id to check against if a |reauth_email_| is provided but the final
  // e-mail used in sign in does not match. This allows the same gaia user to
  // sign in with another e-mail if it has changed.
  const std::string reauth_gaia_id_;

  // Default domain used for all sign in requests.
  const std::string email_domains_;

  // Specific gaia endpoint path to use for signin page.
  const std::string gcpw_endpoint_path_;

  // Additional mdm oauth scopes flag value.
  const std::string additional_mdm_oauth_scopes;

  // Callback that will be called when a valid sign in has been completed
  // through the dialog.
  mutable HandleGcpwSigninCompleteResult signin_callback_;

  mutable CredentialProviderWebUIMessageHandler* handler_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialProviderWebDialogDelegate);
};

bool ValidateSigninCompleteResult(const std::string& access_token,
                                  const std::string& refresh_token,
                                  const base::Value& signin_result) {
  return !access_token.empty() && !refresh_token.empty() &&
         signin_result.is_dict();
}

#if BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
void EnableGcpwSigninDialogForTesting(bool enable) {
  g_enable_gcpw_signin_during_tests = enable;
}
#endif  // BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)

bool CanStartGCPWSignin() {
#if BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
  if (g_enable_gcpw_signin_during_tests)
    return true;
#endif  // BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
  // Ensure that we are running under a "winlogon" desktop before starting the
  // gcpw sign in dialog.
  return base::win::IsRunningUnderDesktopName(STRING16_LITERAL("winlogon"));
}

bool StartGCPWSignin(const base::CommandLine& command_line,
                     content::BrowserContext* context) {
  // If we are prevented from showing gcpw signin, return false and write our
  // result so that the launch fails and the process can exit gracefully.
  if (!CanStartGCPWSignin()) {
    base::Value failure_result(base::Value::Type::DICTIONARY);
    failure_result.SetKey(credential_provider::kKeyExitCode,
                          base::Value(static_cast<int>(
                              credential_provider::kUiecMissingSigninData)));
    WriteResultToHandle(failure_result);
    return false;
  }

  // This keep_alive is created since there is no browser created when
  // --gcpw-logon is specified. Since there is no browser there is no holder
  // of a ScopedKeepAlive present that will ensure Chrome kills itself when
  // the last keep alive is released. So instead, keep the keep alive across
  // the callbacks that will be sent during the signin process. Once the full
  // fetch of the information necesssary for the GCPW is finished (or there is
  // a failure) release the keep alive so that Chrome can shutdown.
  ShowCredentialProviderSigninDialog(
      command_line, context,
      base::BindOnce(&HandleSigninCompleteForGcpwLogin,
                     std::make_unique<ScopedKeepAlive>(
                         KeepAliveOrigin::CREDENTIAL_PROVIDER_SIGNIN_DIALOG,
                         KeepAliveRestartOption::DISABLED)));
  return true;
}

views::WebDialogView* ShowCredentialProviderSigninDialog(
    const base::CommandLine& command_line,
    content::BrowserContext* context,
    HandleGcpwSigninCompleteResult signin_complete_handler) {
  DCHECK(signin_complete_handler);

  // Open a frameless window whose entire surface displays a gaia sign in web
  // page.
  std::string reauth_email = command_line.GetSwitchValueASCII(
      credential_provider::kPrefillEmailSwitch);
  std::string reauth_gaia_id =
      command_line.GetSwitchValueASCII(credential_provider::kGaiaIdSwitch);
  std::string email_domains = command_line.GetSwitchValueASCII(
      credential_provider::kEmailDomainsSwitch);
  std::string gcpw_endpoint_path = command_line.GetSwitchValueASCII(
      credential_provider::kGcpwEndpointPathSwitch);
  std::string additional_mdm_oauth_scopes = command_line.GetSwitchValueASCII(
      credential_provider::kGcpwAdditionalOauthScopes);

  // Delegate to handle the result of the sign in request. This will
  // delete itself eventually when it receives the OnDialogClosed call.
  auto delegate = std::make_unique<CredentialProviderWebDialogDelegate>(
      reauth_email, reauth_gaia_id, email_domains, gcpw_endpoint_path,
      additional_mdm_oauth_scopes, std::move(signin_complete_handler));

  // The web dialog view that will contain the web ui for the login screen.
  // This view will be automatically deleted by the widget that owns it when it
  // is closed.
  auto view = std::make_unique<views::WebDialogView>(
      context, delegate.release(),
      std::make_unique<ChromeWebContentsHandler>());
  views::Widget::InitParams init_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  views::WebDialogView* web_view = view.release();
  init_params.name = "GCPW";  // Used for debugging only.
  init_params.delegate = web_view;

  // This widget will automatically delete itself and its WebDialogView when the
  // dialog window is closed.
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(init_params));
  widget->Show();

  return web_view;
}
