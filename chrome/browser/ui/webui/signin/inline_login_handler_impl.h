// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace content {
class StoragePartition;
}

namespace network {
class SharedURLLoaderFactory;
}

class Browser;
class SigninUIError;

// Implementation for the inline login WebUI handler on desktop Chrome. Once
// CrOS migrates to the same webview approach as desktop Chrome, much of the
// code in this class should move to its base class |InlineLoginHandler|.
class InlineLoginHandlerImpl : public InlineLoginHandler {
 public:
  InlineLoginHandlerImpl();

  InlineLoginHandlerImpl(const InlineLoginHandlerImpl&) = delete;
  InlineLoginHandlerImpl& operator=(const InlineLoginHandlerImpl&) = delete;

  ~InlineLoginHandlerImpl() override;

  using InlineLoginHandler::web_ui;
  using InlineLoginHandler::CloseDialogFromJavascript;

  base::WeakPtr<InlineLoginHandlerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  Browser* GetDesktopBrowser();
  void SyncSetupFailed();
  void HandleLoginError(const SigninUIError& error);

  // Calls the javascript function 'sendLSTFetchResults' with the given
  // base::Value result. This value will be passed to another
  // WebUiMessageHandler which needs to handle the 'lstFetchResults' message
  // with a single argument that is the dictionary of values relevant for the
  // sign in of the user.
  void SendLSTFetchResultsMessage(const base::Value& arg);

 private:
  // InlineLoginHandler overrides:
  void SetExtraInitParams(base::Value::Dict& params) override;
  void CompleteLogin(const CompleteLoginParams& params) override;

  // This struct exists to pass parameters to the FinishCompleteLogin() method,
  // since the base::BindRepeating() call does not support this many template
  // args.
  struct FinishCompleteLoginParams {
   public:
    FinishCompleteLoginParams(InlineLoginHandlerImpl* handler,
                              content::StoragePartition* partition,
                              const GURL& url,
                              const base::FilePath& profile_path,
                              bool confirm_untrusted_signin,
                              const std::string& email,
                              const std::string& gaia_id,
                              const std::string& password,
                              const std::string& auth_code,
                              bool is_force_sign_in_with_usermanager);
    FinishCompleteLoginParams(const FinishCompleteLoginParams& other);
    ~FinishCompleteLoginParams();

    // Pointer to WebUI handler.  May be nullptr.
    raw_ptr<InlineLoginHandlerImpl> handler;
    // The isolate storage partition containing sign in cookies.
    raw_ptr<content::StoragePartition> partition;
    // URL of sign in containing parameters such as email, source, etc.
    GURL url;
    // Path to profile being signed in. Non empty only when unlocking a profile
    // from the user manager.
    base::FilePath profile_path;
    // When true, an extra prompt will be shown to the user before sign in
    // completes.
    bool confirm_untrusted_signin;
    // Email address of the account used to sign in.
    std::string email;
    // Obfustcated gaia id of the account used to sign in.
    std::string gaia_id;
    // Password of the account used to sign in.
    std::string password;
    // Authentication code used to exchange for a login scoped refresh token
    // for the account used to sign in.  Used only with password separated
    // signin flow.
    std::string auth_code;
    // True if user signing in with UserManager when force-sign-in policy is
    // enabled.
    bool is_force_sign_in_with_usermanager;
  };

  static void FinishCompleteLogin(const FinishCompleteLoginParams& params,
                                  Profile* profile);

  // True if the user has navigated to untrusted domains during the signin
  // process.
  bool confirm_untrusted_signin_;

  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_{this};
};

// Handles details of signing the user in with IdentityManager and turning on
// sync after InlineLoginHandlerImpl has acquired the auth tokens from GAIA.
// This is a separate class from InlineLoginHandlerImpl because the full signin
// process is asynchronous and can outlive the signin UI.
// InlineLoginHandlerImpl is destroyed once the UI is closed.
class InlineSigninHelper : public GaiaAuthConsumer {
 public:
  InlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      bool confirm_untrusted_signin,
      bool is_force_sign_in_with_usermanager);

  InlineSigninHelper(const InlineSigninHelper&) = delete;
  InlineSigninHelper& operator=(const InlineSigninHelper&) = delete;

  ~InlineSigninHelper() override;

 protected:
  GaiaAuthFetcher* GetGaiaAuthFetcherForTest() { return &gaia_auth_fetcher_; }

 private:
  // Overridden from GaiaAuthConsumer.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error)
      override;

  void OnClientOAuthSuccessAndBrowserOpened(const ClientOAuthResult& result);

  // Callback invoked once the user has responded to the signin confirmation UI.
  // If confirmed is false, the signin is aborted.
  void UntrustedSigninConfirmed(const std::string& refresh_token,
                                bool confirmed);

  // Creates the sync starter.  Virtual for tests. Call to exchange oauth code
  // for tokens.
  virtual void CreateSyncStarter(const std::string& refresh_token);

  GaiaAuthFetcher gaia_auth_fetcher_;
  base::WeakPtr<InlineLoginHandlerImpl> handler_;
  raw_ptr<Profile> profile_;
  const GURL current_url_;
  const std::string email_;
  const std::string gaia_id_;
  const std::string password_;
  const std::string auth_code_;
  const bool confirm_untrusted_signin_;
  const bool is_force_sign_in_with_usermanager_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
