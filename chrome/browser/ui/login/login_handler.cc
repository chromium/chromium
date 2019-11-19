// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/login/login_interstitial_delegate.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/logging/log_manager.h"
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
#include "content/public/common/origin_util.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/view_type_utils.h"
#endif

using autofill::PasswordForm;
using content::BrowserThread;
using content::NavigationController;
using content::WebContents;

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
    const autofill::PasswordForm& observed_form)
    : model(login_model), form(observed_form) {
  DCHECK(model);
}

LoginHandler::LoginHandler(const net::AuthChallengeInfo& auth_info,
                           content::WebContents* web_contents,
                           LoginAuthRequiredCallback auth_required_callback)
    : WebContentsObserver(web_contents),
      auth_info_(auth_info),
      auth_required_callback_(std::move(auth_required_callback)),
      prompt_started_(false) {
  DCHECK(web_contents);
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

    // This may be called as the browser is closing a tab, so run the
    // interstitial logic on a fresh event loop iteration.
    if (interstitial_delegate_) {
      base::PostTask(FROM_HERE, {BrowserThread::UI},
                     base::BindOnce(&LoginInterstitialDelegate::DontProceed,
                                    interstitial_delegate_));
    }
  }
}

void LoginHandler::Start(
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& request_url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    HandlerMode mode) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents());
  DCHECK(!WasAuthHandled());

  if (base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials)) {
    // When committed interstitials are enabled, the login prompt is not shown
    // until the interstitial is committed. Create the LoginTabHelper here so
    // that it can observe the interstitial committing and show the prompt then.
    LoginTabHelper::CreateForWebContents(web_contents());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the WebRequest API wants to take a shot at intercepting this, we can
  // return immediately. |continuation| will eventually be invoked if the
  // request isn't cancelled.
  auto* api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          web_contents()->GetBrowserContext());
  auto continuation = base::BindOnce(&LoginHandler::MaybeSetUpLoginPrompt,
                                     weak_factory_.GetWeakPtr(), request_url,
                                     is_main_frame, mode);
  if (api->MaybeProxyAuthRequest(web_contents()->GetBrowserContext(),
                                 auth_info_, std::move(response_headers),
                                 request_id, is_main_frame,
                                 std::move(continuation))) {
    return;
  }
#endif

  // To avoid reentrancy problems, this function must not call
  // |auth_required_callback_| synchronously. Defer MaybeSetUpLoginPrompt by an
  // event loop iteration.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&LoginHandler::MaybeSetUpLoginPrompt,
                     weak_factory_.GetWeakPtr(), request_url, is_main_frame,
                     mode, base::nullopt, false /* should_cancel */));
}

void LoginHandler::SetAuth(const base::string16& username,
                           const base::string16& password) {
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
  std::move(callback).Run(base::nullopt);
}

void LoginHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(type == chrome::NOTIFICATION_AUTH_SUPPLIED ||
         type == chrome::NOTIFICATION_AUTH_CANCELLED);

  // Break out early if we aren't interested in the notification.
  if (!web_contents() || WasAuthHandled())
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
      controller->GetBrowserContext() != web_contents()->GetBrowserContext()) {
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

void LoginHandler::NotifyAuthNeeded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (WasAuthHandled() || !prompt_started_)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller =
      web_contents() ? &web_contents()->GetController() : nullptr;
  LoginNotificationDetails details(this);

  service->Notify(chrome::NOTIFICATION_AUTH_NEEDED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

void LoginHandler::NotifyAuthSupplied(const base::string16& username,
                                      const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(WasAuthHandled());

  if (!web_contents() || !prompt_started_)
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  NavigationController* controller = &web_contents()->GetController();
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
      web_contents() ? &web_contents()->GetController() : nullptr;
  LoginNotificationDetails details(this);
  service->Notify(chrome::NOTIFICATION_AUTH_CANCELLED,
                  content::Source<NavigationController>(controller),
                  content::Details<LoginNotificationDetails>(&details));
}

password_manager::PasswordManagerClient*
LoginHandler::GetPasswordManagerClientFromWebContent() {
  if (!web_contents())
    return nullptr;
  password_manager::PasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(web_contents());
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

  if (interstitial_delegate_) {
    // This may be called as the browser is closing a tab, so run the
    // interstitial logic on a fresh event loop iteration.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&LoginInterstitialDelegate::Proceed,
                                  interstitial_delegate_));
  }
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
    signon_realm = url.GetOrigin().spec();
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
  if (base::LowerCaseEqualsASCII(auth_info.scheme, net::kBasicAuthScheme)) {
    dialog_form.scheme = PasswordForm::Scheme::kBasic;
  } else if (base::LowerCaseEqualsASCII(auth_info.scheme,
                                        net::kDigestAuthScheme)) {
    dialog_form.scheme = PasswordForm::Scheme::kDigest;
  } else {
    dialog_form.scheme = PasswordForm::Scheme::kOther;
  }
  dialog_form.origin = auth_info.challenger.GetURL();
  DCHECK(auth_info.is_proxy || auth_info.challenger.IsSameOriginWith(
                                   url::Origin::Create(request_url)));
  dialog_form.signon_realm = GetSignonRealm(dialog_form.origin, auth_info);
  return dialog_form;
}

// static
void LoginHandler::GetDialogStrings(const GURL& request_url,
                                    const net::AuthChallengeInfo& auth_info,
                                    base::string16* authority,
                                    base::string16* explanation) {
  GURL authority_url;

  if (auth_info.is_proxy) {
    *authority = l10n_util::GetStringFUTF16(
        IDS_LOGIN_DIALOG_PROXY_AUTHORITY,
        url_formatter::FormatOriginForSecurityDisplay(
            auth_info.challenger, url_formatter::SchemeDisplay::SHOW));
    authority_url = auth_info.challenger.GetURL();
  } else {
    *authority = url_formatter::FormatUrlForSecurityDisplay(request_url);
#if defined(OS_ANDROID)
    // Android concatenates with a space rather than displaying on two separate
    // lines, so it needs some surrounding text.
    *authority =
        l10n_util::GetStringFUTF16(IDS_LOGIN_DIALOG_AUTHORITY, *authority);
#endif
    authority_url = request_url;
  }

  if (!content::IsOriginSecure(authority_url)) {
    // TODO(asanka): The string should be different for proxies and servers.
    // http://crbug.com/620756
    *explanation = l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_NOT_PRIVATE);
  } else {
    explanation->clear();
  }
}

void LoginHandler::MaybeSetUpLoginPrompt(
    const GURL& request_url,
    bool is_request_for_main_frame,
    HandlerMode mode,
    const base::Optional<net::AuthCredentials>& credentials,
    bool should_cancel) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The request may have been handled while the WebRequest API was processing.
  if (!web_contents() || !web_contents()->GetDelegate() || WasAuthHandled() ||
      should_cancel) {
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

  // When committed interstitials are enabled, a login prompt is triggered once
  // the interstitial commits (that is, when |mode| is POST_COMMIT).
  if (base::FeatureList::IsEnabled(features::kHTTPAuthCommittedInterstitials)) {
    if (mode == POST_COMMIT) {
      prompt_started_ = true;
      ShowLoginPrompt(request_url);
      return;
    }

    // In PRE_COMMIT mode, always cancel main frame requests that receive auth
    // challenges. An interstitial will be committed as the result of the
    // cancellation, and the login prompt will be shown on top of it in
    // POST_COMMIT mode once the interstitial commits.
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
    // navigation, so LoginHandler will run in POST_COMMIT mode and show another
    // login prompt. For simplicity, and because same-origin auth prompts should
    // be relatively rare due to credential caching, we commit an interstitial
    // for all main-frame navigations.
    if (is_request_for_main_frame) {
      DCHECK(mode == PRE_COMMIT);
      RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_WITH_INTERSTITIAL);
      CancelAuth();
      return;
    }
  }

  prompt_started_ = true;

  // Check if this is a main frame navigation and
  // (a) if the request is cross origin or
  // (b) if an interstitial is already being shown or
  // (c) the prompt is for proxy authentication
  // (d) we're not displaying a standalone app
  //
  // For (a), there are two different ways the navigation can occur:
  // 1- The user enters the resource URL in the omnibox.
  // 2- The page redirects to the resource.
  // In both cases, the last committed URL is different than the resource URL,
  // so checking it is sufficient.
  // Note that (1) will not be true once site isolation is enabled, as any
  // navigation could cause a cross-process swap, including link clicks.
  //
  // For (b), the login interstitial should always replace an existing
  // interstitial. This is because |LoginHandler::CloseContents| tries to
  // proceed whatever interstitial is being shown when the login dialog is
  // closed, so that interstitial should only be a login interstitial.
  //
  // For (c), the authority information in the omnibox will be (and should be)
  // different from the authority information in the authentication prompt. An
  // interstitial with an empty URL clears the omnibox and reduces the possible
  // user confusion that may result from the different authority information
  // being displayed simultaneously. This is specially important when the proxy
  // is accessed via an open connection while the target server is considered
  // secure.
  const bool is_cross_origin_request =
      web_contents()->GetLastCommittedURL().GetOrigin() !=
      request_url.GetOrigin();
  if (is_request_for_main_frame &&
      (is_cross_origin_request || web_contents()->ShowingInterstitialPage() ||
       auth_info().is_proxy) &&
      web_contents()->GetDelegate()->GetDisplayMode(web_contents()) !=
          blink::mojom::DisplayMode::kStandalone) {
    DCHECK(!base::FeatureList::IsEnabled(
        features::kHTTPAuthCommittedInterstitials));
    RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_WITH_INTERSTITIAL);

    // Show a blank interstitial for main-frame, cross origin requests
    // so that the correct URL is shown in the omnibox.
    base::OnceClosure callback =
        base::BindOnce(&LoginHandler::ShowLoginPrompt,
                       weak_factory_.GetWeakPtr(), request_url);
    // The interstitial delegate is owned by the interstitial that it creates.
    // This cancels any existing interstitial.
    interstitial_delegate_ =
        (new LoginInterstitialDelegate(
             web_contents(), auth_info().is_proxy ? GURL() : request_url,
             std::move(callback)))
            ->GetWeakPtr();

  } else {
    if (is_request_for_main_frame) {
      RecordHttpAuthPromptType(AUTH_PROMPT_TYPE_MAIN_FRAME);
    } else {
      RecordHttpAuthPromptType(is_cross_origin_request
                                   ? AUTH_PROMPT_TYPE_SUBRESOURCE_CROSS_ORIGIN
                                   : AUTH_PROMPT_TYPE_SUBRESOURCE_SAME_ORIGIN);
    }
    ShowLoginPrompt(request_url);
  }
}

void LoginHandler::ShowLoginPrompt(const GURL& request_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents() || WasAuthHandled()) {
    CancelAuth();
    return;
  }
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents());
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_AUTH_NEEDED);
    CancelAuth();
    return;
  }

  base::string16 authority;
  base::string16 explanation;
  GetDialogStrings(request_url, auth_info(), &authority, &explanation);

  password_manager::HttpAuthManager* httpauth_manager =
      GetHttpAuthManagerForLogin();

  if (!httpauth_manager) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // A WebContents in a <webview> (a GuestView type) does not have a password
    // manager, but still needs to be able to show login prompts.
    const auto* guest =
        guest_view::GuestViewBase::FromWebContents(web_contents());
    if (guest && extensions::GetViewType(guest->owner_web_contents()) !=
                     extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
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
    const base::string16& authority,
    const base::string16& explanation,
    LoginHandler::LoginModelData* login_model_data) {
  if (login_model_data)
    password_form_ = login_model_data->form;
  base::WeakPtr<LoginHandler> guard = weak_factory_.GetWeakPtr();
  BuildViewImpl(authority, explanation, login_model_data);
  // BuildViewImpl may call Cancel, which may delete this object, so check a
  // WeakPtr before NotifyAuthNeeded.
  if (guard)
    NotifyAuthNeeded();
}

// ----------------------------------------------------------------------------
// Public API
std::unique_ptr<content::LoginDelegate> CreateLoginPrompt(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    LoginHandler::HandlerMode mode,
    LoginAuthRequiredCallback auth_required_callback) {
  std::unique_ptr<LoginHandler> handler = LoginHandler::Create(
      auth_info, web_contents, std::move(auth_required_callback));
  handler->Start(request_id, is_request_for_main_frame, url,
                 std::move(response_headers), mode);
  return handler;
}
