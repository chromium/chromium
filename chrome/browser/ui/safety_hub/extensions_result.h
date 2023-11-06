// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_EXTENSIONS_RESULT_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_EXTENSIONS_RESULT_H_

#include <memory>

#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

// TODO(crbug.com/1443466): Reuse the result in Safety Check extensions handler.
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

  // Creates a Result based on the provided Dict. This should only be used for
  // results that only capture the unpublished extensions, to be used in the
  // menu notifications.
  explicit SafetyHubExtensionsResult(const base::Value::Dict& dict);

  SafetyHubExtensionsResult(const SafetyHubExtensionsResult&);
  SafetyHubExtensionsResult& operator=(const SafetyHubExtensionsResult&);

  ~SafetyHubExtensionsResult() override;

  // Gets a result containing all the extensions that should be reviewed. The
  // parameter only_unpublished_extensions indicates whether only extensions
  // that have been unpublished for a long time should be considered.
  static absl::optional<std::unique_ptr<SafetyHubService::Result>> GetResult(
      const extensions::CWSInfoService* extension_info_service,
      Profile* profile,
      bool only_unpublished_extensions);

  // Returns the number of extensions that need review according to the result.
  unsigned int GetNumTriggeringExtensions() const;

  // SafetyHubService::Result implementation

  std::unique_ptr<SafetyHubService::Result> Clone() const override;

  base::Value::Dict ToDictValue() const override;

  bool IsTriggerForMenuNotification() const override;

  bool WarrantsNewMenuNotification(const Result& previousResult) const override;

  std::u16string GetNotificationString() const override;

  int GetNotificationCommandId() const override;

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
