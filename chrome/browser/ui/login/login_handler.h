// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

// This is the base implementation for the OS-specific classes that prompt for
// authentication information.
class LoginHandler : public content::LoginDelegate {
 public:
  // The purpose of this struct is to enforce that BuildViewImpl receives either
  // both the login model and the observed form, or none. That is a bit spoiled
  // by the fact that the model is a pointer to LoginModel, as opposed to a
  // reference. Having it as a reference would go against the style guide, which
  // forbids non-const references in arguments, presumably also inside passed
  // structs, because the guide's rationale still applies. Therefore at least
  // the constructor DCHECKs that |login_model| is not null.
  struct LoginModelData {
    LoginModelData(password_manager::HttpAuthManager* login_model,
                   const password_manager::PasswordForm& observed_form);

    const raw_ptr<password_manager::HttpAuthManager> model;
    const raw_ref<const password_manager::PasswordForm> form;
  };

  ~LoginHandler() override;

  // Builds the platform specific LoginHandler. The resulting handler calls
  // auth_required_callback when credentials are available. If destroyed before
  // them, the login request is aborted and the callback will not be called. The
  // callback must remain valid until one of those two events occurs.
  static std::unique_ptr<LoginHandler> Create(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      LoginAuthRequiredCallback auth_required_callback);

  // Exposed for testing.
  static std::vector<LoginHandler*> GetAllLoginHandlersForTest();

  // The main entry point for an auth request for a main-frame request. This
  // method allows extensions to handle the auth request, and otherwise cancels
  // the request to show a blank error page. ShowLoginPromptAfterCommit() can be
  // called to show a login prompt atop the blank page once it commits.
  // |extension_cancellation_callback| will be called if an extension chooses to
  // cancel the auth request; the callback will be called immediately before the
  // auth request is actually cancelled.
  void StartMainFrame(
      const content::GlobalRequestID& request_id,
      const GURL& request_url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      base::OnceCallback<void(const content::GlobalRequestID& request_id)>
          extension_cancellation_callback);

  // The main entry point for an auth request for a subresource (include
  // subframe main resources). This method allows extensions to handle the auth
  // request, and may show an auth prompt to the user if extensions do not
  // handle the request.
  void StartSubresource(
      const content::GlobalRequestID& request_id,
      const GURL& request_url,
      scoped_refptr<net::HttpResponseHeaders> response_headers);

  void ShowLoginPromptAfterCommit(const GURL& request_url);

  // Resend the request with authentication credentials.
  // This function can be called from either thread.
  void SetAuth(const std::u16string& username, const std::u16string& password);

  // Display the error page without asking for credentials again.
  // This function can be called from either thread.
  void CancelAuth();

  // Who/where/what asked for the authentication.
  const net::AuthChallengeInfo& auth_info() const { return auth_info_; }

  // The WebContents.
  content::WebContents* web_contents() { return web_contents_.get(); }

 protected:
  LoginHandler(const net::AuthChallengeInfo& auth_info,
               content::WebContents* web_contents,
               LoginAuthRequiredCallback auth_required_callback);

  // Implement this to initialize the underlying platform specific view. If
  // |login_model_data| is not null, the contained LoginModel and PasswordForm
  // should be used to register the view with the password manager.
  virtual void BuildViewImpl(const std::u16string& authority,
                             const std::u16string& explanation,
                             LoginModelData* login_model_data) = 0;

  // Closes the native dialog.
  virtual void CloseDialog() = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(LoginHandlerTest, DialogStringsAndRealm);

  void StartInternal(const content::GlobalRequestID& request_id,
                     bool is_main_frame,
                     const GURL& request_url,
                     scoped_refptr<net::HttpResponseHeaders> response_headers);

  // Notify observers that authentication is needed.
  void NotifyAuthNeeded();

  // Notify observers that authentication is supplied.
  void NotifyAuthSupplied(const std::u16string& username,
                          const std::u16string& password);

  // Notify observers that authentication is cancelled.
  void NotifyAuthCancelled();

  // When any handler finishes, called on every other handler. |username| and
  // |password| are only valid if |supplied| is true. If |supplied| is false
  // then the handler was cancelled. This gives |this| handler the opportunity
  // to dismiss itself if it was waiting for the same authentication.
  void OtherHandlerFinished(bool supplied,
                            LoginHandler* other_handler,
                            const std::u16string& username,
                            const std::u16string& password);

  // Returns the PasswordManagerClient from the web content.
  password_manager::PasswordManagerClient*
  GetPasswordManagerClientFromWebContent();

  // Returns the HttpAuthManager.
  password_manager::HttpAuthManager* GetHttpAuthManagerForLogin();

  // Returns whether authentication had been handled (SetAuth or CancelAuth).
  bool WasAuthHandled() const;

  // Closes the view_contents from the UI loop.
  void CloseContents();

  // Get the signon_realm under which this auth info should be stored.
  //
  // The format of the signon_realm for proxy auth is:
  //     proxy-host:proxy-port/auth-realm
  // The format of the signon_realm for server auth is:
  //     url-scheme://url-host[:url-port]/auth-realm
  //
  // Be careful when changing this function, since you could make existing
  // saved logins un-retrievable.
  static std::string GetSignonRealm(const GURL& url,
                                    const net::AuthChallengeInfo& auth_info);

  // Helper to create a PasswordForm for PasswordManager to start looking for
  // saved credentials.
  static password_manager::PasswordForm MakeInputForPasswordManager(
      const GURL& url,
      const net::AuthChallengeInfo& auth_info);

  static void GetDialogStrings(const GURL& request_url,
                               const net::AuthChallengeInfo& auth_info,
                               std::u16string* authority,
                               std::u16string* explanation);

  // Continuation from |Start| after any potential interception from the
  // extensions WebRequest API. If |cancelled_by_extension| is |true| the
  // request is cancelled. Otherwise |credentials| are used if supplied. Finally
  // if the request is NOT cancelled AND |credentials| is empty, then we'll take
  // the necessary steps to show a login prompt. This may entail cancelling the
  // navigation if it is a main-frame request (and a login prompt will be shown
  // after commit), or showing the prompt directly otherwise.
  void MaybeSetUpLoginPromptBeforeCommit(
      const GURL& request_url,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const absl::optional<net::AuthCredentials>& credentials,
      bool cancelled_by_extension);

  void ShowLoginPrompt(const GURL& request_url);

  void BuildViewAndNotify(const std::u16string& authority,
                          const std::u16string& explanation,
                          LoginModelData* login_model_data);

  base::WeakPtr<content::WebContents> web_contents_;

  // Who/where/what asked for the authentication.
  net::AuthChallengeInfo auth_info_;

  // The PasswordForm sent to the PasswordManager. This is so we can refer to it
  // when later notifying the password manager if the credentials were accepted
  // or rejected.  This should only be accessed on the UI loop.
  password_manager::PasswordForm password_form_;

  // Observes other login handlers so this login handler can respond.
  content::NotificationRegistrar registrar_;

  LoginAuthRequiredCallback auth_required_callback_;

  // This callback is called if an extension cancels an auth request for a main
  // frame main resource.
  base::OnceCallback<void(const content::GlobalRequestID& request_id)>
      extension_main_frame_cancellation_callback_;

  // True if the extensions logic has run and the prompt logic has started.
  bool prompt_started_;
  base::WeakPtrFactory<LoginHandler> weak_factory_{this};
};

// Details to provide the content::NotificationObserver.  Used by the automation
// proxy for testing.
class LoginNotificationDetails {
 public:
  explicit LoginNotificationDetails(LoginHandler* handler)
      : handler_(handler) {}

  LoginNotificationDetails(const LoginNotificationDetails&) = delete;
  LoginNotificationDetails& operator=(const LoginNotificationDetails&) = delete;

  LoginHandler* handler() const { return handler_; }

 private:
  LoginNotificationDetails() = default;

  raw_ptr<LoginHandler, DanglingUntriaged>
      handler_;  // Where to send the response.
};

// Details to provide the NotificationObserver.  Used by the automation proxy
// for testing and by other LoginHandlers to dismiss themselves when an
// identical auth is supplied.
class AuthSuppliedLoginNotificationDetails : public LoginNotificationDetails {
 public:
  AuthSuppliedLoginNotificationDetails(LoginHandler* handler,
                                       const std::u16string& username,
                                       const std::u16string& password)
      : LoginNotificationDetails(handler),
        username_(username),
        password_(password) {}

  AuthSuppliedLoginNotificationDetails(
      const AuthSuppliedLoginNotificationDetails&) = delete;
  AuthSuppliedLoginNotificationDetails& operator=(
      const AuthSuppliedLoginNotificationDetails&) = delete;

  const std::u16string& username() const { return username_; }
  const std::u16string& password() const { return password_; }

 private:
  // The username that was used for the authentication.
  const std::u16string username_;

  // The password that was used for the authentication.
  const std::u16string password_;
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_H_
