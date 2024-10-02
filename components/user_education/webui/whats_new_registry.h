// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_WHATS_NEW_REGISTRY_H_

#include "components/user_education/webui/whats_new_storage_service.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {

using BrowserCommand = browser_command::mojom::Command;

// What's New modules represent sections of content on the What's New
// page. These are meant to contain the Feature they describe, the ownership
// of the module, and the browser command that it triggers, if any.
//
// To connect the rollout of your Feature to your WhatsNewModule,
// supple a base::Feature when creating a module. This will tell the
// embedded page when the user has enabled this Feature that the content
// may be shown.
//
// Content on the What's New page that is released to 100% Stable before
// a milestone launches does not need to register a WhatsNewModule. The
// content will always be shown. Regardless, do remember to create metrics
// variants for these modules using the module name you agreed upon
// with frizzle-team@google.com. However, if this module triggers
// a browser command, it still needs to be created and registered, just
// without a base::Feature.
//
// Metrics:
// When registering a module, make sure to add UserAction and Histogram
// variants. Creating these are enforced by the registrar unit tests.
//
// * For a WhatsNewModule with a Feature, the metric name should be the
//      same as the name of the base::Feature.
// * For a WhatsNewModule without a Feature, the metric name should follow
//      the same pattern, but can be any string that uniquely identifies
//      this content.
class WhatsNewModule {
 public:
  ~WhatsNewModule() = default;

  // Creates a WhatsNewModule tied to the rollout of a base::Feature.
  //
  // This module must include a base::Feature and an owner string, but
  // can optionally pass a browser command.
  WhatsNewModule(const base::Feature& feature,
                 std::string owner,
                 std::optional<BrowserCommand> browser_command = std::nullopt)
      : feature_(&feature),
        metric_name_(""),
        owner_(owner),
        browser_command_(browser_command) {}

  // Creates a default-enabled WhatsNewModule in order to enable
  // a browser command on the embedded page.
  //
  // Default-enabled modules do not reference a base::Feature, but must
  // include a metric string, an owner string and a browser command.
  WhatsNewModule(std::string metric_name,
                 std::string owner,
                 std::optional<BrowserCommand> browser_command)
      : feature_(nullptr),
        metric_name_(metric_name),
        owner_(owner),
        browser_command_(browser_command) {}

  std::optional<BrowserCommand> browser_command() const {
    return browser_command_;
  }

  std::string metric_name() const {
    if (HasFeature()) {
      return GetFeatureName();
    }
    return metric_name_;
  }

  // Return true if the module has a feature, i.e. is not default-enabled.
  bool HasFeature() const;

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

 private:
  raw_ptr<const base::Feature> feature_ = nullptr;
  std::string metric_name_;
  std::string owner_;
  std::optional<BrowserCommand> browser_command_;
};

// What's New editions represent an entire What's New page with content
// relevant to a single feature or set of features. Editions are always
// tied to a base::Feature, but this Feature may be enabled by default.
//
// As with modules, remember to add user action and histogram variants
// with the same name as the base::Feature for your edition.
class WhatsNewEdition {
 public:
  WhatsNewEdition(const base::Feature& feature,
                  std::string owner,
                  std::vector<BrowserCommand> browser_commands = {});
  WhatsNewEdition(WhatsNewEdition&&);
  WhatsNewEdition& operator=(WhatsNewEdition&&);
  ~WhatsNewEdition();

  std::vector<BrowserCommand> browser_commands() const {
    return browser_commands_;
  }

  std::string metric_name() const { return GetFeatureName(); }

  // Return true if the feature is enabled.
  bool IsFeatureEnabled() const;

  // Get the name of the feature for this module.
  const char* GetFeatureName() const;

 private:
  raw_ref<const base::Feature> feature_;
  std::string owner_;
  std::vector<BrowserCommand> browser_commands_;
};

// Stores module and edition data used to display the What's New page,
// customized to the feature's a user has enabled.
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
  void SetEditionUsed(std::string_view edition);

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
