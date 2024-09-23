// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_EXTENSIONS_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_EXTENSIONS_RESULT_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"

class Profile;

class SafetyHubExtensionsResult : public SafetyHubService::Result {
 public:
  SafetyHubExtensionsResult() = delete;

  // Creates a SafetyHubExtensionsResult with the provided triggering
  // extensions. The is_unpublished_extensions_only parameter should indicate
  // whether these extensions are only those that have been unpublished for a
  // long time, or any extension that is available for review in Safety Hub.
  explicit SafetyHubExtensionsResult(
      std::set<extensions::ExtensionId> triggering_extensions,
      bool is_unpublished_extensions_only);

  SafetyHubExtensionsResult(const SafetyHubExtensionsResult&);
  SafetyHubExtensionsResult& operator=(const SafetyHubExtensionsResult&);

  ~SafetyHubExtensionsResult() override;

  // Gets a result containing all the extensions that should be reviewed. The
  // parameter only_unpublished_extensions indicates whether only extensions
  // that have been unpublished for a long time should be considered.
  static std::optional<std::unique_ptr<SafetyHubService::Result>> GetResult(
      Profile* profile,
      bool only_unpublished_extensions);

  // Returns the number of extensions that need review according to the result.
  unsigned int GetNumTriggeringExtensions() const;

  // Updates the `triggering_extensions_` if an extension is kept.
  void OnExtensionPrefsUpdated(const std::string& extension_id,
                               Profile* profile);

  // Updates the `triggering_extensions_` if an extension is uninstalled.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason);

  // SafetyHubService::Result implementation
  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(
      const base::Value::Dict& previous_result_dict) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

  // Testing function to manipulate the `triggering_extensions_` dictionary.
  void ClearTriggeringExtensionsForTesting();
  void SetTriggeringExtensionForTesting(std::string extension_id);

 private:
  // A set of extension id's that have triggered a Safety Hub review, but
  // have not had a decision made by a user. When generating a result for menu
  // notifications, this will only hold the extensions that have been
  // unpublished for a long time.
  std::set<extensions::ExtensionId> triggering_extensions_;

  // Captures whether this result is for unpublished extensions only.
  bool is_unpublished_extensions_only_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_EXTENSIONS_RESULT_H_
