// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

namespace extensions {
class Extension;
}

class Profile;

namespace settings {

// Handle OS font list and font preference settings.
class FontHandler : public SettingsPageUIHandler,
                    public extensions::ExtensionRegistryObserver {
 public:
  explicit FontHandler(content::WebUI* webui);
  ~FontHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

 private:
  // Handler for script asking for font information.
  void HandleFetchFontsData(const base::ListValue* args);

  // Listen for changes to whether the advanced font extension is available.
  // An initial update will be sent when observation begins.
  void HandleObserveAdvancedFontExtensionAvailable(const base::ListValue* args);

  // Open the advanced font settings page.
  void HandleOpenAdvancedFontSettings(const base::ListValue* args);

  // Callback to handle fonts loading.
  void FontListHasLoaded(std::string callback_id,
                         std::unique_ptr<base::ListValue> list);

  const extensions::Extension* GetAdvancedFontSettingsExtension();

  void NotifyAdvancedFontSettingsAvailability();

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  Profile* profile_;  // Weak pointer.

  base::WeakPtrFactory<FontHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FontHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
