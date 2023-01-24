// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_ui_message_handler.h"

class IntroHandler : public content::WebUIMessageHandler {
 public:
  explicit IntroHandler(base::RepeatingCallback<void(bool sign_in)> callback,
                        bool is_device_managed);

  IntroHandler(const IntroHandler&) = delete;
  IntroHandler& operator=(const IntroHandler&) = delete;

  ~IntroHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;

 private:
  // Handles "continueWithAccount" message from the page. No arguments.
  // This message is sent when the user confirms that they want to sign in to
  // Chrome.
  void HandleContinueWithAccount(const base::Value::List& args);

  // Handles "continueWithoutAccount" message from the page. No arguments.
  // This message is sent when the user declines signing in to Chrome.
  void HandleContinueWithoutAccount(const base::Value::List& args);

  // Handles "initializeMainView" message from the page. No arguments.
  // This message is sent when the view is created.
  void HandleInitializeMainView(const base::Value::List& args);

  // Fires the `managed-device-disclaimer-updated` event with the disclaimer
  // that will be caught and handled in the ts file.
  void FireManagedDisclaimerUpdate(std::string disclaimer);

  const base::RepeatingCallback<void(bool sign_in)> callback_;
  const bool is_device_managed_ = false;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<policy::CloudPolicyStore::Observer> policy_store_observer_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
