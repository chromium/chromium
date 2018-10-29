// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace network {
class SharedURLLoaderFactory;
}

// Implementation for the inline login WebUI handler on desktop Chrome. Once
// CrOS migrates to the same webview approach as desktop Chrome, much of the
// code in this class should move to its base class |InlineLoginHandler|.
class InlineLoginHandlerImpl : public InlineLoginHandler,
                               public content::WebContentsObserver {
 public:
  InlineLoginHandlerImpl();
  ~InlineLoginHandlerImpl() override;

  using InlineLoginHandler::web_ui;
  using InlineLoginHandler::CloseDialogFromJavascript;

  base::WeakPtr<InlineLoginHandlerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  Browser* GetDesktopBrowser();
  void SyncStarterCallback(OneClickSigninSyncStarter::SyncSetupResult result);
  // Closes the current tab and shows the account management view of the avatar
  // bubble if |show_account_management| is true.
  void CloseTab(bool show_account_management);
  void HandleLoginError(const std::string& error_msg,
                        const base::string16& email);

 private:
  // InlineLoginHandler overrides:
  void SetExtraInitParams(base::DictionaryValue& params) override;
  void CompleteLogin(const std::string& email,
                     const std::string& password,
                     const std::string& gaia_id,
                     const std::string& auth_code,
                     bool skip_for_now,
                     bool trusted,
                     bool trusted_found,
                     bool choose_what_to_sync) override;

  // This struct exists to pass paramters to the FinishCompleteLogin() method,
  // since the base::Bind() call does not support this many template args.
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
                              bool choose_what_to_sync,
                              bool is_force_sign_in_with_usermanager);
    FinishCompleteLoginParams(const FinishCompleteLoginParams& other);
    ~FinishCompleteLoginParams();

    // Pointer to WebUI handler.  May be nullptr.
    InlineLoginHandlerImpl* handler;
    // The isolate storage partition containing sign in cookies.
    content::StoragePartition* partition;
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
    // True if the user wants to configure sync before signing in.
    bool choose_what_to_sync;
    // True if user signing in with UserManager when force-sign-in policy is
    // enabled.
    bool is_force_sign_in_with_usermanager;
  };

  static void FinishCompleteLogin(const FinishCompleteLoginParams& params,
                                  Profile* profile,
                                  Profile::CreateStatus status);

  // content::WebContentsObserver implementation:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // True if the user has navigated to untrusted domains during the signin
  // process.
  bool confirm_untrusted_signin_;

  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginHandlerImpl);
};

// Handles details of signing the user in with SigninManager and turning on
// sync after InlineLoginHandlerImpl has acquired the auth tokens from GAIA.
// This is a separate class from InlineLoginHandlerImpl because the full signin
// process is asynchronous and can outlive the signin UI.
// InlineLoginHandlerImpl is destryed once the UI is closed.
class InlineSigninHelper : public GaiaAuthConsumer {
 public:
  InlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      Profile::CreateStatus create_status,
      const GURL& current_url,
      const std::string& email,
      const std::string& gaia_id,
      const std::string& password,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id,
      bool choose_what_to_sync,
      bool confirm_untrusted_signin,
      bool is_force_sign_in_with_usermanager);
  ~InlineSigninHelper() override;

 protected:
  GaiaAuthFetcher* GetGaiaAuthFetcherForTest() { return &gaia_auth_fetcher_; }

 private:
  // Handles cross account sign in error. If the supplied |email| does not match
  // the last signed in email of the current profile, then Chrome will show a
  // confirmation dialog before starting sync. It returns true if there is a
  // cross account error, and false otherwise.
  bool HandleCrossAccountError(
      const std::string& refresh_token,
      OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
      OneClickSigninSyncStarter::StartSyncMode start_mode);

  // Callback used with ConfirmEmailDialogDelegate.
  void ConfirmEmailAction(
      content::WebContents* web_contents,
      const std::string& refresh_token,
      OneClickSigninSyncStarter::ConfirmationRequired confirmation_required,
      OneClickSigninSyncStarter::StartSyncMode start_mode,
      SigninEmailConfirmationDialog::Action action);

  // Overridden from GaiaAuthConsumer.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error)
      override;

  void OnClientOAuthSuccessAndBrowserOpened(const ClientOAuthResult& result,
                                            Profile* profile,
                                            Profile::CreateStatus status);

  // Creates the sync starter.  Virtual for tests. Call to exchange oauth code
  // for tokens.
  virtual void CreateSyncStarter(
      Browser* browser,
      const GURL& current_url,
      const std::string& refresh_token,
      OneClickSigninSyncStarter::ProfileMode profile_mode,
      OneClickSigninSyncStarter::StartSyncMode start_mode,
      OneClickSigninSyncStarter::ConfirmationRequired confirmation_required);

  GaiaAuthFetcher gaia_auth_fetcher_;
  base::WeakPtr<InlineLoginHandlerImpl> handler_;
  Profile* profile_;
  Profile::CreateStatus create_status_;
  GURL current_url_;
  std::string email_;
  std::string gaia_id_;
  std::string password_;
  std::string auth_code_;
  bool choose_what_to_sync_;
  bool confirm_untrusted_signin_;
  bool is_force_sign_in_with_usermanager_;

  DISALLOW_COPY_AND_ASSIGN(InlineSigninHelper);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
