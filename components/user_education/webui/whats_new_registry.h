// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_REGISTRY_H_

#include "components/user_education/webui/whats_new_storage_service.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {

using BrowserCommand = browser_command::mojom::Command;

class WhatsNewModule {
 public:
  WhatsNewModule(const base::Feature* feature,
                 std::string owner,
                 std::optional<BrowserCommand> browser_command = std::nullopt)
      : feature_(feature), owner_(owner), browser_command_(browser_command) {
    CHECK(feature);
  }
  WhatsNewModule(WhatsNewModule&& other) noexcept = default;
  WhatsNewModule& operator=(WhatsNewModule&& other) noexcept = default;
  ~WhatsNewModule() = default;

  std::optional<BrowserCommand> browser_command() const {
    return browser_command_;
  }

  // Return true if the feature is enabled, but not by default.
  // This indicates a feature is in the process of rolling out.
  bool HasActiveFeature() const;

  // Return true if the feature has been enabled by default.
  // This indicates the feature has recently rolled out to all users.
  bool HasRolledFeature() const;

  // Return true if the feature is enabled.
  bool IsFeatureEnabled() const;

  // Get the name of the feature for this module.
  const char* GetFeatureName() const;

  // Returns true if the module can appear on the What's New Page.
  bool IsAvailable() const;

 private:
  raw_ptr<const base::Feature> feature_ = nullptr;
  std::string owner_;
  std::optional<BrowserCommand> browser_command_;
};

class WhatsNewEdition : public WhatsNewModule {
 public:
  WhatsNewEdition(const base::Feature* feature, std::string owner)
      : WhatsNewModule(feature, owner, std::nullopt) {}
};

class WhatsNewRegistry {
 public:
  explicit WhatsNewRegistry(
      std::unique_ptr<WhatsNewStorageService> storage_service);
  ~WhatsNewRegistry();

  // Register a module to be shown on the What's New Page.
  void RegisterModule(WhatsNewModule module);

  // Register an edition of the What's New Page.
  void RegisterEdition(WhatsNewEdition edition);

  // Used to pass active browser commands to WhatsNewUI.
  const std::vector<BrowserCommand> GetActiveCommands() const;

  // Used to send enabled flags to server-side router.
  const std::vector<std::string_view> GetActiveFeatureNames() const;

  // Used to send enabled-by-default flags to server-side router.
  const std::vector<std::string_view> GetRolledFeatureNames() const;

  // Set a "used version" for an edition.
  void SetEditionUsed(const std::string_view edition);

  // Cleanup data from storage for housekeeping.
  void ClearUnregisteredModules();
  void ClearUnregisteredEditions();

  // Resets all stored data for manual testing.
  void ResetData();

  const WhatsNewStorageService* storage_service() const {
    return storage_service_.get();
  }

  const std::vector<WhatsNewModule>& modules() const { return modules_; }
  const std::vector<WhatsNewEdition>& editions() const { return editions_; }

 private:
  std::unique_ptr<WhatsNewStorageService> storage_service_;
  std::vector<WhatsNewModule> modules_;
  std::vector<WhatsNewEdition> editions_;
};

}  // namespace whats_new

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_REGISTRY_H_
