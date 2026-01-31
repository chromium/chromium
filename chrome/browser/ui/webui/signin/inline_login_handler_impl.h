// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_id.h"

namespace network {
class SharedURLLoaderFactory;
}

class Profile;
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

  using InlineLoginHandler::CloseDialogFromJavascript;
  using InlineLoginHandler::web_ui;

  base::WeakPtr<InlineLoginHandlerImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void HandleLoginError(const SigninUIError& error);

  // Calls the javascript function 'sendLSTFetchResults' with the given
  // base::Value result. This value will be passed to another
  // WebUiMessageHandler which needs to handle the 'lstFetchResults' message
  // with a single argument that is the dictionary of values relevant for the
  // sign in of the user.
  void SendLSTFetchResultsMessage(const base::Value& arg);

 private:
  // InlineLoginHandler overrides:
  void SetExtraInitParams(base::DictValue& params) override;
  void CompleteLogin(const CompleteLoginParams& params) override;

  base::WeakPtrFactory<InlineLoginHandlerImpl> weak_factory_{this};
};

// Handles details of signing the user in after InlineLoginHandlerImpl has
// acquired the auth tokens from GAIA. This is a separate class from
// `InlineLoginHandlerImpl` because the full signin process is asynchronous and
// can outlive the signin UI. `InlineLoginHandlerImpl` is destroyed once the UI
// is closed.
class InlineSigninHelper : public GaiaAuthConsumer {
 public:
  InlineSigninHelper(
      base::WeakPtr<InlineLoginHandlerImpl> handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const GURL& current_url,
      const std::string& email,
      const GaiaId& gaia_id,
      const std::string& password,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id);

  InlineSigninHelper(const InlineSigninHelper&) = delete;
  InlineSigninHelper& operator=(const InlineSigninHelper&) = delete;

  ~InlineSigninHelper() override;

 private:
  // Overridden from GaiaAuthConsumer.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  GaiaAuthFetcher gaia_auth_fetcher_;
  base::WeakPtr<InlineLoginHandlerImpl> handler_;
  raw_ptr<Profile> profile_;
  const GURL current_url_;
  const std::string email_;
  const GaiaId gaia_id_;
  const std::string password_;
  const std::string auth_code_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_IMPL_H_
