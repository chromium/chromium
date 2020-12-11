// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_STARTUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_STARTUP_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

namespace base {
class ListValue;
}

namespace settings {

class OnStartupHandler : public SettingsPageUIHandler,
                         public extensions::ExtensionRegistryObserver {
 public:
  static const char kOnStartupNtpExtensionEventName[];

  explicit OnStartupHandler(Profile* profile);
  ~OnStartupHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OnStartupHandlerTest, HandleGetNtpExtension);
  FRIEND_TEST_ALL_PREFIXES(OnStartupHandlerTest,
                           HandleValidateStartupPage_Valid);
  FRIEND_TEST_ALL_PREFIXES(OnStartupHandlerTest,
                           HandleValidateStartupPage_Invalid);

  // Info for extension controlling the NTP or empty value.
  std::unique_ptr<base::Value> GetNtpExtension();

  // Handler for the "getNtpExtension" message. No arguments.
  void HandleGetNtpExtension(const base::ListValue* /*args*/);

  // Handles the "validateStartupPage" message. Passed a URL that might be a
  // valid startup page.
  void HandleValidateStartupPage(const base::ListValue* args);

  // extensions::ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const extensions::Extension* extension) override;

  // Listen to extension unloaded notifications.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(OnStartupHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ON_STARTUP_HANDLER_H_
