// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
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
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

using content::BrowserThread;
using content::NavigationController;
using content::WebContents;
using password_manager::PasswordForm;

namespace {

// Auth prompt types for UMA. Do not reorder or delete entries; only add to the
// end.
enum AuthPromptType {
  AUTH_PROMPT_TYPE_WITH_INTERSTITIAL = 0,
  AUTH_PROMPT_TYPE_MAIN_FRAME = 1,
  AUTH_PROMPT_TYPE_SUBRESOURCE_SAME_ORIGIN = 2,
  AUTH_PROMPT_TYPE_SUBRESOURCE_CROSS_ORIGIN = 3,
  AUTH_PROMPT_TYPE_ENUM_COUNT = 4
};

void RecordHttpAuthPromptType(AuthPromptType prompt_type) {
  UMA_HISTOGRAM_ENUMERATION("Net.HttpAuthPromptType", prompt_type,
                            AUTH_PROMPT_TYPE_ENUM_COUNT);
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
  password_manager::HttpAuthManager* http_auth_manager =
      GetHttpAuthManagerForLogin();
  if (http_auth_manager)
    http_auth_manager->OnPasswordFormDismissed();

  if (!WasAuthHandled()) {
    auth_required_callback_.Reset();

    // TODO(https://crbug.com/916315): Remove this line.
    NotifyAuthCancelled();
  }
}

void LoginHandler::StartMainFrame(
    const content::GlobalRequestID& request_id,
    const GURL& request_url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    base::OnceCallback<void(const content::GlobalRequestID& request_id)>
        extension_cancellation_callback) {
  extension_main_frame_cancellation_callback_ =
      std::move(extension_cancellation_callback);
  StartInternal(request_id, true /* is_main_frame */, request_url,
                response_headers);
}

void LoginHandler::StartSubresource(
    const content::GlobalRequestID& request_id,
    const GURL& request_url,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  StartInternal(request_id, false /* is_main_frame */, request_url,
                response_headers);
}

void LoginHandler::ShowLoginPromptAfterCommit(const GURL& request_url) {
  // The request may have been handled while the WebRequest API was processing.
  if (!web_contents_ || !web_contents_->GetDelegate() ||
      web_contents_->IsBeingDestroyed() || WasAuthHandled()) {
    CancelAuth();
    return;
  }

  // This is OK; we break out of the Observe() if we aren't handling the same
  // auth_info() or BrowserContext.
  //
  // TODO(davidben): Only listen to notifications within a single
  // BrowserContext.
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllBrowserContextsAndSources());

  prompt_started_ = true;
  ShowLoginPrompt(request_url);
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
  registrar_.RemoveAll();

  // Calling NotifyAuthSupplied() first allows other LoginHandler instances to
  // call CloseContents() before us. Closing dialogs in the opposite order as
  // they were created avoids races where remaining dialogs in the same tab may
  // be briefly displayed to the user before they are removed.
  NotifyAuthSupplied(username, password);
  CloseContents();
  std::move(callback).Run(net::AuthCredentials(username, password));
}

void LoginHandler::CancelAuth() {
  if (WasAuthHandled())
    return;

  LoginAuthRequiredCallback callback = std::move(auth_required_callback_);
  registrar_.RemoveAll();

  NotifyAuthCancelled();
  CloseContents();
  std::move(callback).Run(absl::nullopt);
}

void LoginHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
         type == chrome::NOTIFICATION_AUTH_CANCELLED);

  // Break out early if we aren't interested in the notification.
  if (!web_contents_ || WasAuthHandled())
    return;

  LoginNotificationDetails* login_details =
      content::Details<LoginNotificationDetails>(details).ptr();

  // WasAuthHandled() should always test positive before we publish
  // AUTH_SUPPLIED or AUTH_CANCELLED notifications.
  DCHECK(login_details->handler() != this);

  // Only handle notification for the identical auth info. When comparing
  // AuthChallengeInfos, ignore path because the same credentials can be used
  // for different paths.
  if (!auth_info().MatchesExceptPath(login_details->handler()->auth_info()))
    return;

  // Ignore login notification events from other profiles.
  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (!controller ||
      controller->GetBrowserContext() != web_contents_->GetBrowserContext()) {
    return;
  }

  // Set or cancel the auth in this handler. Defer an event loop iteration to
  // avoid potential reentrancy issues.
  if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    AuthSuppliedLoginNotificationDetails* supplied_details =
        content::Details<AuthSuppliedLoginNotificationDetails>(details).ptr();
    SetAuth(supplied_details->username(), supplied_details->password());
  } else {
    DCHECK(type == chrome::NOTIFICATION_AUTH_CANCELLED);
    CancelAuth();
  }
}

LoginHandler::LoginHandler(const net::AuthChallengeInfo& auth_info,
                           content::WebContents* web_contents,
                           LoginAuthRequiredCallback auth_required_callback)
    : web_contents_(web_contents->GetWeakPtr()),
      auth_info_(auth_info),
      auth_required_callback_(std::move(auth_required_callback)),
      prompt_started_(false) {}

void LoginHandler::StartInternal(
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& request_url,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents_);
  DCHECK(!WasAuthHandled());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the WebRequest API wants to take a shot at intercepting this, we can
  // return immediately. |continuation| will eventually be invoked if the
  // request isn't cancelled.
  auto* api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          web_contents_->GetBrowserContext());
  auto continuation = base::BindOnce(
      &LoginHandler::MaybeSetUpLoginPromptBeforeCommit,
      weak_factory_.GetWeakPtr(), request_url, request_id, is_main_frame);
  if (api->MaybeProxyAuthRequest(web_contents_->GetBrowserContext(), auth_info_,
                                 std::move(response_headers), request_id,
                                 is_main_frame, std::move(continuation))) {
    return;
  }
#endif

  // To avoid reentrancy problems, this function must not call
  // |auth_required_callback_| synchronously. Defer
  // MaybeSetUpLoginPromptBeforeCommit by an event loop iteration.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&LoginHandler::MaybeSetUpLoginPromptBeforeCommit,
                     weak_factory_.GetWeakPtr(), request_url, request_id,
                     is_main_frame, absl::nullopt, false /* should_cancel */));
}

void LoginHandler::NotifyAuthNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WasAuthHandled() || !prompt_started_)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller =
      web_contents_ ? &web_contents_->GetController() : nullptr;
  LoginNotificationDetails details(this);

  service->Notify(chrome::NOTIFICATION_AUTH_NEEDED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthSupplied(const std::u16string& username,
                                      const std::u16string& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  if (!web_contents_ || !prompt_started_)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = &web_contents_->GetController();
  AuthSuppliedLoginNotificationDetails details(this, username, password);

  service->Notify(
      chrome::NOTIFICATION_AUTH_SUPPLIED,
      content::Source<NavigationController>(controller),
      content::Details<AuthSuppliedLoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  if (!prompt_started_)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller =
      web_contents_ ? &web_contents_->GetController() : nullptr;
  LoginNotificationDetails details(this);
  service->Notify(chrome::NOTIFICATION_AUTH_CANCELLED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
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

void LoginHandler::MaybeSetUpLoginPromptBeforeCommit(
    const GURL& request_url,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const absl::optional<net::AuthCredentials>& credentials,
    bool cancelled_by_extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The request may have been handled while the WebRequest API was processing.
  if (!web_contents_ || !web_contents_->GetDelegate() || WasAuthHandled() ||
      cancelled_by_extension) {
    if (cancelled_by_extension && is_request_for_main_frame &&
        !extension_main_frame_cancellation_callback_.is_null()) {
      std::move(extension_main_frame_cancellation_callback_).Run(request_id);
    }
    CancelAuth();
    return;
  }

  if (credentials) {
    SetAuth(credentials->username(), credentials->password());
    return;
  }

  // This is OK; we break out of the Observe() if we aren't handling the same
  // auth_info() or BrowserContext.
  //
  // TODO(davidben): Only listen to notifications within a single
  // BrowserContext.
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllBrowserContextsAndSources());

  // Always cancel main frame requests that receive auth challenges. An
  // interstitial will be committed as the result of the cancellation, and the
  // login prompt will be shown on top of it once the interstitial commits.
  //
  // Strictly speaking, it is not necessary to show an interstitial for all
  // main-frame navigations, just cross-origin ones. However, we show an
  // interstitial for all main-frame navigations for simplicity. Otherwise,
  // it's difficult to prevent repeated prompts on cancellation. For example,
  // imagine that we navigate from http://a.com/1 to http://a.com/2 and show a
  // login prompt without committing an interstitial. If the prompt is
  // cancelled, the request will then be resumed to read the 401 body and
  // commit the navigation. But the committed 401 error looks
  // indistinguishable from what we commit in the case of a cross-origin
  // navigation, so LoginHandler will show another login prompt. For
  // simplicity, and because same-origin auth prompts should be relatively
  // rare due to credential caching, we commit an interstitial for all
  // main-frame navigations.
  if (is_request_for_main_frame) {
    RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_WITH_INTERSTITIAL);
    CancelAuth();
    return;
  }

  prompt_started_ = true;
  RecordHttpAuthPromptType(
      web_contents_->GetLastCommittedURL().DeprecatedGetOriginAsURL() !=
              request_url.DeprecatedGetOriginAsURL()
          ? AUTH_PROMPT_TYPE_SUBRESOURCE_CROSS_ORIGIN
          : AUTH_PROMPT_TYPE_SUBRESOURCE_SAME_ORIGIN);
  ShowLoginPrompt(request_url);
}

void LoginHandler::ShowLoginPrompt(const GURL& request_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents_ || WasAuthHandled()) {
    CancelAuth();
    return;
  }
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents_.get());
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_AUTH_NEEDED);
    CancelAuth();
    return;
  }

  std::u16string authority;
  std::u16string explanation;
  GetDialogStrings(request_url, auth_info(), &authority, &explanation);

  password_manager::HttpAuthManager* httpauth_manager =
      GetHttpAuthManagerForLogin();

  if (!httpauth_manager) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // A WebContents in a <webview> (a GuestView type) does not have a password
    // manager, but still needs to be able to show login prompts.
    const auto* guest =
        guest_view::GuestViewBase::FromWebContents(web_contents_.get());
    if (guest && extensions::GetViewType(guest->owner_web_contents()) !=
                     extensions::mojom::ViewType::kExtensionBackgroundPage) {
      BuildViewAndNotify(authority, explanation, nullptr);
      return;
    }
#endif
    CancelAuth();
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
  base::WeakPtr<LoginHandler> guard = weak_factory_.GetWeakPtr();
  BuildViewImpl(authority, explanation, login_model_data);
  // BuildViewImpl may call Cancel, which may delete this object, so check a
  // WeakPtr before NotifyAuthNeeded.
  if (guard)
    NotifyAuthNeeded();
}
