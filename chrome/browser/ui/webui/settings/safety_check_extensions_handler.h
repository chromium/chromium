// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class CWSInfoService;
class SafetyCheckExtensionsHandlerTest;
class ExtensionPrefs;
class ExtensionRegistry;
}  // namespace extensions

namespace settings {

// Settings page UI handler that checks for any extensions that trigger
// a review by the safety check.
class SafetyCheckExtensionsHandler
    : public settings::SettingsPageUIHandler,
      public extensions::ExtensionPrefsObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit SafetyCheckExtensionsHandler(Profile* profile);
  ~SafetyCheckExtensionsHandler() override;

  void SetCWSInfoServiceForTest(extensions::CWSInfoService* cws_info_service);

  void SetTriggeringExtensionsForTest(extensions::ExtensionId extension_id);

 private:
  friend class extensions::SafetyCheckExtensionsHandlerTest;

  // Checks if a extension has any safety hub triggers and returns a bool.
  bool CheckExtensionForTrigger(const extensions::Extension& extension);

  // Calculate the number of extensions that need to be reviewed by the
  // user.
  void HandleGetNumberOfExtensionsThatNeedReview(const base::Value::List& args);

  // Let listeners know that the number of extensions that need
  // review may have changed.
  void UpdateNumberOfExtensionsThatNeedReview();

  // Return the number of extensions that should be reviewed by the user.
  // There are currently three triggers the `SafetyCheckExtensionsHandler`
  // tracks:
  // -- Extension Malware Violation
  // -- Extension Policy Violation
  // -- Extension Unpublished by the developer
  int GetNumberOfExtensionsThatNeedReview();

  // ExtensionPrefsObserver implementation to track changes to extensions.
  void OnExtensionPrefsUpdated(const std::string& extension_id) override;

  // ExtensionRegistryObserver implementation to track changes to extensions.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // SettingsPageUIHandler implementation.
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // A set of extension id's that have triggered a safety hub review, but
  // have not had a decision made by a user.
  std::set<extensions::ExtensionId> triggering_extensions_;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<extensions::CWSInfoService> cws_info_service_ = nullptr;
  // Listen to the extension prefs and the extension registry for when
  // prefs are unloaded or changed or an extension is removed.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<extensions::ExtensionPrefs,
                          extensions::ExtensionPrefsObserver>
      prefs_observation_{this};
  base::WeakPtrFactory<SafetyCheckExtensionsHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_EXTENSIONS_HANDLER_H_
