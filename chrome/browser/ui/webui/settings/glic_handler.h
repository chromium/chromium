// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/web_ui.h"

namespace settings {

class GlicHandler : public SettingsPageUIHandler {
 public:
  GlicHandler();

  GlicHandler(const GlicHandler&) = delete;
  GlicHandler& operator=(const GlicHandler&) = delete;

  ~GlicHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void SetWebUIForTesting(content::WebUI* web_ui);

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest, UpdateShortcutSuspension);
  FRIEND_TEST_ALL_PREFIXES(GlicHandlerBrowserTest, UpdateGlicShortcut);

  // Updates settings based on the OS launcher enabled state.
  void HandleSetGlicOsLauncherEnabled(const base::Value::List& args);

  // Sends to the settings page the last saved shortcut.
  void HandleGetGlicShortcut(const base::Value::List& args);

  // Updates the registered glic hotkey with the one provided in `args`.
  void HandleSetGlicShortcut(const base::Value::List& args);

  // Updates the GlobalAcceleratorListener to suspend/unsuspend listening for
  // accelerator input based on `args`.
  void HandleSetShortcutSuspensionState(const base::Value::List& args);

  // Sends the last saved glic focus toggle shortcut to the settings page.
  void HandleGetGlicFocusToggleShortcut(const base::Value::List& args);

  // Updates the glic focus toggle hotkey with the one provided in
  // `args`.
  void HandleSetGlicFocusToggleShortcut(const base::Value::List& args);

  // Sends the client whether glic is disallowed by the admin or not.
  void HandleGetGlicDisallowedByAdmin(const base::Value::List& args);

  // Notifies the client whether glic is disallowed by their administrator,
  // either on request or because it changed.
  void FireOnGlicDisallowedByAdminChanged();

  // Callback for when the ActorKeyedService notifies of a capability change.
  void OnWebActuationCapabilityChanged(bool can_act_on_web);

  // Used to listen to changes in web actuation capability status.
  base::CallbackListSubscription web_actuation_subscription_;

  // Used to listen to changes in glic enabling status.
  std::unique_ptr<base::CallbackListSubscription> glic_enabling_subscription_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_GLIC_HANDLER_H_
