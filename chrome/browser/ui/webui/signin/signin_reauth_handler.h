// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

// WebUI message handler for the signin reauth dialog.
class SigninReauthHandler : public content::WebUIMessageHandler,
                            public SigninReauthViewController::Observer {
 public:
  // Creates a SigninReauthHandler for the |controller|.
  SigninReauthHandler(SigninReauthViewController* controller,
                      base::flat_map<std::string, int> string_to_grd_id_map);
  ~SigninReauthHandler() override;

  SigninReauthHandler(const SigninReauthHandler&) = delete;
  SigninReauthHandler& operator=(const SigninReauthHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;

  // SigninReauthViewController::Observer:
  void OnReauthControllerDestroyed() override;
  void OnGaiaReauthTypeDetermined(
      SigninReauthViewController::GaiaReauthType reauth_type) override;

 protected:
  // Handles "initialize" message from the page. No arguments.
  virtual void HandleInitialize(const base::Value::List& args);

  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete the
  // reauth flow.
  virtual void HandleConfirm(const base::Value::List& args);

  // Handles "cancel" message from the page. No arguments. This message is sent
  // when the user cancels the reauth flow.
  virtual void HandleCancel(const base::Value::List& args);

 private:
  sync_pb::UserConsentTypes::AccountPasswordsConsent BuildConsent(
      const base::Value::List& args) const;

  // May be null if |controller_| gets destroyed earlier than |this|.
  raw_ptr<SigninReauthViewController> controller_;

  base::ScopedObservation<SigninReauthViewController,
                          SigninReauthViewController::Observer>
      controller_observation_{this};

  // Mapping between strings displayed in the UI corresponding to this handler
  // and their respective GRD IDs.
  base::flat_map<std::string, int> string_to_grd_id_map_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_REAUTH_HANDLER_H_
