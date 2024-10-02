// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_STORAGE_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_STORAGE_SERVICE_H_

#include "base/values.h"

namespace whats_new {

// Virtual interface for supplying the WhatsNewRegistry with a store.
// Keeps track of the order in which WhatsNewModules have been enabled
// as well as during which milestone a WhatsNewEdition was shown to the user.
class WhatsNewStorageService {
 public:
  WhatsNewStorageService() = default;
  virtual ~WhatsNewStorageService() = default;

  // Disallow copy and assign.
  WhatsNewStorageService(const WhatsNewStorageService&) = delete;
  WhatsNewStorageService& operator=(const WhatsNewStorageService&) = delete;

  // Read-only access.
  virtual const base::Value::List& ReadModuleData() const = 0;
  virtual const base::Value::Dict& ReadEditionData() const = 0;

  // Get the version this edition was used. Return nullopt if unused.
  virtual std::optional<int> GetUsedVersion(
      std::string_view edition_name) const = 0;

  // Find the name of the edition used for the current version, if any.
  virtual std::optional<std::string_view> FindEditionForCurrentVersion()
      const = 0;

  // Find the module's position in the queue of enabled modules.
  // Returns -1 if the module is not in the queue.
  virtual int GetModuleQueuePosition(std::string_view module_name) const = 0;

  // Returns whether an edition has ever been used for version.
  virtual bool IsUsedEdition(std::string_view edition_name) const = 0;

  // Add a module to the ordered list of enabled modules.
  virtual void SetModuleEnabled(std::string_view module_name) = 0;

  // Clear module from stored data.
  virtual void ClearModule(std::string_view module_name) = 0;

  // Set a "used version" for an edition.
  virtual void SetEditionUsed(std::string_view edition_name) = 0;

  // Clear edition from stored data.
  virtual void ClearEdition(std::string_view edition_name) = 0;

  // Reset all stored data for manual testing.
  // This should only be called from the internal testing page.
  virtual void Reset() = 0;
};

}  // namespace whats_new

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_STORAGE_SERVICE_H_
