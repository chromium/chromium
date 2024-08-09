// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/view_type_utils.h"  // nogncheck
#include "extensions/common/mojom/view_type.mojom.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;
using password_manager::PasswordForm;

namespace {

// All login handlers should be tracked in this global singleton.
using LoginHandlerVector = std::vector<base::WeakPtr<LoginHandler>>;
LoginHandlerVector& GetAllLoginHandlers() {
  static base::NoDestructor<LoginHandlerVector> instance;
  return *instance;
}

}  // namespace

// ----------------------------------------------------------------------------
// LoginHandler

LoginHandler::LoginModelData::LoginModelData(
    password_manager::HttpAuthManager* login_model,
    const password_manager::PasswordForm& observed_form)
    : model(login_model), form(observed_form) {
  DCHECK(model);
}

LoginHandler::~LoginHandler() {
  auto& login_handlers = GetAllLoginHandlers();
  for (auto it = login_handlers.begin(); it != login_handlers.end(); ++it) {
    if (it->get() == this) {
      login_handlers.erase(it);
      break;
    }
  }

  password_manager::HttpAuthManager* http_auth_manager =
      GetHttpAuthManagerForLogin();
  if (http_auth_manager)
    http_auth_manager->OnPasswordFormDismissed();

  if (!WasAuthHandled()) {
    auth_required_callback_.Reset();
  }
}

// static
std::vector<LoginHandler*> LoginHandler::GetAllLoginHandlersForTest() {
  std::vector<LoginHandler*> output;
  for (auto& weak_ptr : GetAllLoginHandlers()) {
    if (weak_ptr) {
      output.push_back(weak_ptr.get());
    }
  }
  return output;
}

void LoginHandler::SetAuth(const std::u16string& username,
                           const std::u16string& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  password_manager::PasswordManagerClient* client =
      GetPasswordManagerClientFromWebContent();
  if (client && client->GetLogManager()->IsLoggingActive()) {
    logger =
        std::make_unique<password_manager::BrowserSavePasswordProgressLogger>(
            client->GetLogManager());
    logger->LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SET_AUTH_METHOD);
  }

  bool already_handled = WasAuthHandled();
  if (logger) {
    logger->LogBoolean(
        autofill::SavePasswordProgressLogger::STRING_AUTHENTICATION_HANDLED,
        already_handled);
  }
  if (already_handled)
    return;

  password_manager::HttpAuthManager* httpauth_manager =
      GetHttpAuthManagerForLogin();

  // Tell the http-auth manager the credentials were submitted / accepted.
  if (httpauth_manager) {
    password_form_.username_value = username;
    password_form_.password_value = password;
    httpauth_manager->OnPasswordFormSubmitted(password_form_);
    if (logger) {
      logger->LogPasswordForm(
          autofill::SavePasswordProgressLogger::STRING_LOGINHANDLER_FORM,
          password_form_);
    }
  }

  LoginAuthRequiredCallback callback = std::move(auth_required_callback_);

  // Calling NotifyAuthSupplied() first allows other LoginHandler instances to
  // call CloseContents() before us. Closing dialogs in the opposite order as
  // they were created avoids races where remaining dialogs in the same tab may
  // be briefly displayed to the user before they are removed.
  if (web_contents_) {
    NotifyAuthSupplied(username, password);
  }
  CloseContents();
  std::move(callback).Run(net::AuthCredentials(username, password));
}

void LoginHandler::CancelAuth(bool notify_others) {
  if (WasAuthHandled())
    return;

  LoginAuthRequiredCallback callback = std::move(auth_required_callback_);

  if (notify_others) {
    NotifyAuthCancelled();
  }

  CloseContents();
  std::move(callback).Run(std::nullopt);
}

LoginHandler::LoginHandler(const net::AuthChallengeInfo& auth_info,
                           content::WebContents* web_contents,
                           LoginAuthRequiredCallback auth_required_callback)
    : web_contents_(web_contents->GetWeakPtr()),
      auth_info_(auth_info),
      auth_required_callback_(std::move(auth_required_callback)) {
  GetAllLoginHandlers().push_back(weak_factory_.GetWeakPtr());
}

void LoginHandler::NotifyAuthNeeded() {
  // Only used by tests. This is being refactored. https://crbug.com/1371177.
}

void LoginHandler::NotifyAuthSupplied(const std::u16string& username,
                                      const std::u16string& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  // Intentionally make a copy to avoid issues with iterator invalidation.
  LoginHandlerVector vec = GetAllLoginHandlers();
  for (auto& weak_login_handler : vec) {
    if (weak_login_handler && weak_login_handler.get() != this) {
      weak_login_handler->OtherHandlerFinished(/*supplied=*/true, this,
                                               username, password);
    }
  }
}

void LoginHandler::NotifyAuthCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  // Intentionally make a copy to avoid issues with iterator invalidation.
  LoginHandlerVector vec = GetAllLoginHandlers();
  for (auto& weak_login_handler : vec) {
    if (weak_login_handler && weak_login_handler.get() != this) {
      weak_login_handler->OtherHandlerFinished(/*supplied=*/false, this,
                                               /*username=*/std::u16string(),
                                               /*password=*/std::u16string());
    }
  }
}

void LoginHandler::OtherHandlerFinished(bool supplied,
                                        LoginHandler* other_handler,
                                        const std::u16string& username,
                                        const std::u16string& password) {
  // Break out early if we aren't interested in the notification.
  if (!web_contents_ || WasAuthHandled()) {
    return;
  }

  // Only listen to notifications within a single BrowserContext.
  if (other_handler->web_contents() &&
      other_handler->web_contents()->GetBrowserContext() !=
          web_contents_->GetBrowserContext()) {
    return;
  }

  // We should never dispatch to self.
  DCHECK(other_handler != this);

  // Only handle notification for the identical auth info. When comparing
  // AuthChallengeInfos, ignore path because the same credentials can be used
  // for different paths.
  if (!auth_info().MatchesExceptPath(other_handler->auth_info())) {
    return;
  }

  // Ignore login notification events from other StoragePartitions.
  // TODO(crbug.com/40202416): Getting the StoragePartition from the WebContents
  // is fine for now, but we'll need to plumb frame information to LoginHandler
  // as part of removing the multi-WebContents architecture.
  content::StoragePartition* source_partition =
      other_handler->web_contents() ? other_handler->web_contents()
                                          ->GetPrimaryMainFrame()
                                          ->GetStoragePartition()
                                    : nullptr;
  content::StoragePartition* partition =
      web_contents()->GetPrimaryMainFrame()->GetStoragePartition();
  if (!source_partition || source_partition != partition) {
    return;
  }

  // Set or cancel the auth in this handler. Defer an event loop iteration to
  // avoid potential reentrancy issues.
  if (supplied) {
    SetAuth(username, password);
  } else {
    CancelAuth(/*notify_others=*/true);
  }
}

password_manager::PasswordManagerClient*
LoginHandler::GetPasswordManagerClientFromWebContent() {
  if (!web_contents_)
    return nullptr;
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents_.get());
  return client;
}

password_manager::HttpAuthManager* LoginHandler::GetHttpAuthManagerForLogin() {
  password_manager::PasswordManagerClient* client =
      GetPasswordManagerClientFromWebContent();
  return client ? client->GetHttpAuthManager() : nullptr;
}

// Returns whether authentication had been handled (SetAuth or CancelAuth).
bool LoginHandler::WasAuthHandled() const {
  return auth_required_callback_.is_null();
}

// Closes the view_contents from the UI loop.
void LoginHandler::CloseContents() {
  CloseDialog();
}

// static
std::string LoginHandler::GetSignonRealm(
    const GURL& url,
    const net::AuthChallengeInfo& auth_info) {
  std::string signon_realm;
  if (auth_info.is_proxy) {
    // Historically we've been storing the signon realm for proxies using
    // net::HostPortPair::ToString().
    net::HostPortPair host_port_pair =
        net::HostPortPair::FromURL(auth_info.challenger.GetURL());
    signon_realm = host_port_pair.ToString();
    signon_realm.append("/");
  } else {
    // Take scheme, host, and port from the url.
    signon_realm = url.DeprecatedGetOriginAsURL().spec();
    // This ends with a "/".
  }
  signon_realm.append(auth_info.realm);
  return signon_realm;
}

// static
PasswordForm LoginHandler::MakeInputForPasswordManager(
    const GURL& request_url,
    const net::AuthChallengeInfo& auth_info) {
  PasswordForm dialog_form;
  if (base::EqualsCaseInsensitiveASCII(auth_info.scheme,
                                       net::kBasicAuthScheme)) {
    dialog_form.scheme = PasswordForm::Scheme::kBasic;
  } else if (base::EqualsCaseInsensitiveASCII(auth_info.scheme,
                                              net::kDigestAuthScheme)) {
    dialog_form.scheme = PasswordForm::Scheme::kDigest;
  } else {
    dialog_form.scheme = PasswordForm::Scheme::kOther;
  }
  dialog_form.url = auth_info.challenger.GetURL();
  DCHECK(auth_info.is_proxy ||
         auth_info.challenger == url::SchemeHostPort(request_url));
  dialog_form.signon_realm = GetSignonRealm(dialog_form.url, auth_info);
  return dialog_form;
}

// static
void LoginHandler::GetDialogStrings(const GURL& request_url,
                                    const net::AuthChallengeInfo& auth_info,
                                    std::u16string* authority,
                                    std::u16string* explanation) {
  GURL authority_url;

  if (auth_info.is_proxy) {
    *authority = l10n_util::GetStringFUTF16(
        IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
        url_formatter::FormatUrlForSecurityDisplay(
            auth_info.challenger.GetURL(), url_formatter::SchemeDisplay::SHOW));
    authority_url = auth_info.challenger.GetURL();
  } else {
    *authority = url_formatter::FormatUrlForSecurityDisplay(request_url);
#if BUILDFLAG(IS_ANDROID)
    // Android concatenates with a space rather than displaying on two separate
    // lines, so it needs some surrounding text.
    *authority =
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_AUTHORITY, *authority);
#endif
    authority_url = request_url;
  }

  if (!network::IsUrlPotentiallyTrustworthy(authority_url)) {
    // TODO(asanka): The string should be different for proxies and servers.
    // http://crbug.com/620756
    *explanation = l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_NOT_PRIVATE);
  } else {
    explanation->clear();
  }
}

void LoginHandler::ShowLoginPrompt(const GURL& request_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::u16string authority;
  std::u16string explanation;
  GetDialogStrings(request_url, auth_info(), &authority, &explanation);

  password_manager::HttpAuthManager* httpauth_manager =
      GetHttpAuthManagerForLogin();

  if (!httpauth_manager) {
    // If there is no password manager, then show a view with no password
    // manager hooked up.
    BuildViewAndNotify(authority, explanation, nullptr);
    return;
  }

  password_manager::PasswordManagerClient* client =
      GetPasswordManagerClientFromWebContent();
  if (client && client->GetLogManager()->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        client->GetLogManager());
    logger.LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SHOW_LOGIN_PROMPT_METHOD);
  }

  PasswordForm observed_form(
      MakeInputForPasswordManager(request_url, auth_info()));
  LoginModelData model_data(httpauth_manager, observed_form);
  BuildViewAndNotify(authority, explanation, &model_data);
}

void LoginHandler::BuildViewAndNotify(
    const std::u16string& authority,
    const std::u16string& explanation,
    LoginHandler::LoginModelData* login_model_data) {
  if (login_model_data)
    password_form_ = *login_model_data->form;
  bool success = BuildViewImpl(authority, explanation, login_model_data);
  if (success) {
    NotifyAuthNeeded();
  } else {
    // CancelAuth results in synchronous destruction of `this`. As building the
    // view happens synchronously, we dispatch the cancellation to avoid
    // re-entrancy into the calling code.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&LoginHandler::CancelAuth, weak_factory_.GetWeakPtr(),
                       /*notify_others=*/false));
  }
}
