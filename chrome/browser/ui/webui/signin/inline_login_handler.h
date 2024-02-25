// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/canonical_cookie.h"

namespace signin_metrics {
enum class AccessPoint;
}

extern const char kSignInPromoQueryKeyShowAccountManagement[];

// The base class handler for the inline login WebUI.
class InlineLoginHandler : public content::WebUIMessageHandler {
 public:
  InlineLoginHandler();

  InlineLoginHandler(const InlineLoginHandler&) = delete;
  InlineLoginHandler& operator=(const InlineLoginHandler&) = delete;

  ~InlineLoginHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Enum for gaia auth mode, must match AuthMode defined in
  // chrome/browser/resources/gaia_auth_host/authenticator.js.
  enum AuthMode {
    kDefaultAuthMode = 0,
    kOfflineAuthMode = 1,
    kDesktopAuthMode = 2
  };

  // Parameters passed to `CompleteLogin` method.
  struct CompleteLoginParams {
    CompleteLoginParams();
    CompleteLoginParams(const CompleteLoginParams&);
    CompleteLoginParams& operator=(const CompleteLoginParams&);
    ~CompleteLoginParams();

    std::string email;
    std::string password;
    std::string gaia_id;
    std::string auth_code;
    bool skip_for_now = false;
    bool trusted_value = false;
    bool trusted_found = false;
    // Whether the account should be available in ARC after addition. Used only
    // on Chrome OS.
    bool is_available_in_arc = false;
  };

  // Closes the dialog by calling the |inline.login.closeDialog| Javascript
  // function.
  // Does nothing if calling Javascript functions is not allowed.
  void CloseDialogFromJavascript();

 private:
  // JS callback to prepare for starting auth.
  void HandleInitializeMessage(const base::Value::List& args);

  // Continue to initialize the authenticator component. It calls
  // |SetExtraInitParams| to set extra init params.
  void ContinueHandleInitializeMessage();

  // JS callback to handle tasks after authenticator component loads.
  virtual void HandleAuthenticatorReadyMessage(const base::Value::List& args) {}

  // JS callback to complete login. It calls |CompleteLogin| to do the real
  // work.
  void HandleCompleteLoginMessage(const base::Value::List& args);

  // Called by HandleCompleteLoginMessage after it gets the GAIA URL's cookies
  // from the CookieManager.
  void HandleCompleteLoginMessageWithCookies(
      const base::Value::List& args,
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);

  // JS callback to switch the UI from a constrainted dialog to a full tab.
  void HandleSwitchToFullTabMessage(const base::Value::List& args);

  // Handles the web ui message sent when the window is closed from javascript.
  virtual void HandleDialogClose(const base::Value::List& args);

  virtual void SetExtraInitParams(base::Value::Dict& params) {}
  virtual void CompleteLogin(const CompleteLoginParams& params) = 0;

  base::WeakPtrFactory<InlineLoginHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
