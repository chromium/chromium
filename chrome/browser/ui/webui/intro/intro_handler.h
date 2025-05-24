// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error "This file should only be included if DICE support."
#endif

enum class IntroChoice;
enum class DefaultBrowserChoice;

class IntroHandler : public content::WebUIMessageHandler {
 public:
  explicit IntroHandler(
      base::RepeatingCallback<void(IntroChoice)> intro_callback,
      base::OnceCallback<void(DefaultBrowserChoice)> default_browser_callback,
      bool is_device_managed,
      std::string_view source_name);

  IntroHandler(const IntroHandler&) = delete;
  IntroHandler& operator=(const IntroHandler&) = delete;

  ~IntroHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void ResetIntroButtons();
  void ResetDefaultBrowserButtons();

  // This updates the strings displayed in the set as default page of the first
  // run experience to indicate that it will also pin Chrome to the taskbar.
  void SetCanPinToTaskbar(bool can_pin);

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

  // Handles "setAsDefaultBrowser" message from the page. No arguments.
  // This message is sent when the user confirms that they want to set Chrome as
  // their default browser.
  void HandleSetAsDefaultBrowser(const base::Value::List& args);

  // Handles "skipDefaultBrowser" message from the page. No arguments.
  // This message is sent when the user skips the prompt to set Chrome as their
  // default browser.
  void HandleSkipDefaultBrowser(const base::Value::List& args);

  // Fires the `managed-device-disclaimer-updated` event with the disclaimer
  // that will be caught and handled in the ts file.
  void FireManagedDisclaimerUpdate(std::string disclaimer);

  const base::RepeatingCallback<void(IntroChoice)> intro_callback_;
  base::OnceCallback<void(DefaultBrowserChoice)> default_browser_callback_;
  const bool is_device_managed_ = false;
  std::unique_ptr<policy::CloudPolicyStore::Observer> policy_store_observer_;

  // Name of the WebUIDataSource to update.
  std::string source_name_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_HANDLER_H_
