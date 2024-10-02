// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/whats_new_registry.h"

#include "base/containers/contains.h"
#include "base/values.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {
namespace {
constexpr int kRequestEntropyLimit = 15;
}  // namespace

using BrowserCommand = browser_command::mojom::Command;

bool WhatsNewModule::HasFeature() const {
  return feature_ != nullptr;
}

bool WhatsNewModule::HasActiveFeature() const {
  if (!HasFeature()) {
    return false;
  }
  return base::FeatureList::IsEnabled(*feature_) &&
         feature_->default_state == base::FEATURE_DISABLED_BY_DEFAULT;
}

bool WhatsNewModule::HasRolledFeature() const {
  if (!HasFeature()) {
    return false;
  }
  return feature_->default_state == base::FEATURE_ENABLED_BY_DEFAULT;
}

bool WhatsNewModule::IsFeatureEnabled() const {
  CHECK(HasFeature());
  return base::FeatureList::IsEnabled(*feature_);
}

const char* WhatsNewModule::GetFeatureName() const {
  CHECK(feature_);
  return feature_->name;
}

WhatsNewEdition::WhatsNewEdition(const base::Feature& feature,
                                 std::string owner,
                                 std::vector<BrowserCommand> browser_commands)
    : feature_(feature), owner_(owner), browser_commands_(browser_commands) {}
WhatsNewEdition::WhatsNewEdition(WhatsNewEdition&& other) = default;
WhatsNewEdition::~WhatsNewEdition() = default;

bool WhatsNewEdition::IsFeatureEnabled() const {
  return base::FeatureList::IsEnabled(*feature_);
}

const char* WhatsNewEdition::GetFeatureName() const {
  return feature_->name;
}

void WhatsNewRegistry::RegisterModule(WhatsNewModule module) {
  if (module.HasFeature() && module.IsFeatureEnabled()) {
    storage_service_->SetModuleEnabled(module.GetFeatureName());
  }
  modules_.emplace_back(std::move(module));
}

void WhatsNewRegistry::RegisterEdition(WhatsNewEdition edition) {
  editions_.emplace_back(std::move(edition));
}

const std::vector<BrowserCommand> WhatsNewRegistry::GetActiveCommands() const {
  std::vector<BrowserCommand> commands;
  for (const WhatsNewModule& module : modules_) {
    if (module.browser_command().has_value()) {
      // Modules without a feature are default-enabled.
      const bool module_is_default_enabled = !module.HasFeature();
      // If the module is tied to a feature, ensure the feature is available.
      const bool module_is_available =
          module.HasActiveFeature() || module.HasRolledFeature();
      if (module_is_default_enabled || module_is_available) {
        commands.emplace_back(module.browser_command().value());
      }
    }
  }
  for (const WhatsNewEdition& edition : editions_) {
    // The only requirement for an edition to show is that it is enabled.
    if (edition.IsFeatureEnabled()) {
      for (const auto browser_command : edition.browser_commands()) {
        commands.emplace_back(browser_command);
      }
    }
  }
  return commands;
}

const std::vector<std::string_view> WhatsNewRegistry::GetActiveFeatureNames()
    const {
  std::vector<std::string_view> feature_names;

  // Check if the current version is used for an edition.
  const auto current_edition_name =
      storage_service_->FindEditionForCurrentVersion();
  if (current_edition_name.has_value()) {
    // Request this edition again.
    feature_names.emplace_back(current_edition_name.value());
  } else {
    // Only request other unused editions if there was not one shown during
    // this version.
    for (const WhatsNewEdition& edition : editions_) {
      if (edition.IsFeatureEnabled() &&
          feature_names.size() < kRequestEntropyLimit &&
          !storage_service_->IsUsedEdition(edition.GetFeatureName())) {
        feature_names.emplace_back(edition.GetFeatureName());
      }
    }
  }

  // Add modules based on ordered list.
  const base::Value::List& module_names_in_order =
      storage_service_->ReadModuleData();
  for (const base::Value& module_value : module_names_in_order) {
    if (feature_names.size() >= kRequestEntropyLimit) {
      break;
    }
    const std::string& module_name = module_value.GetString();
    auto module = std::find_if(
        modules_.begin(), modules_.end(),
        [&module_name](WhatsNewModule const& module) {
          return module.HasFeature() ? module.GetFeatureName() == module_name
                                     : false;
        });
    if (module != modules_.end() && module->HasActiveFeature()) {
      feature_names.emplace_back(module->GetFeatureName());
    }
  }

  return feature_names;
}

const std::vector<std::string_view> WhatsNewRegistry::GetRolledFeatureNames()
    const {
  std::vector<std::string_view> feature_names;

  // Add modules based on ordered list.
  const base::Value::List& module_names_in_order =
      storage_service_->ReadModuleData();
  for (auto& module_value : module_names_in_order) {
    if (feature_names.size() >= kRequestEntropyLimit) {
      break;
    }
    auto module_name = module_value.GetString();
    auto module = std::find_if(
        modules_.begin(), modules_.end(),
        [&module_name](WhatsNewModule const& module) {
          return module.HasFeature() ? module.GetFeatureName() == module_name
                                     : false;
        });
    if (module != modules_.end() && module->HasRolledFeature()) {
      feature_names.emplace_back(module->GetFeatureName());
    }
  }

  return feature_names;
}

void WhatsNewRegistry::SetEditionUsed(std::string_view edition_name) {
  // Verify edition exists.
  auto edition = std::find_if(editions_.begin(), editions_.end(),
                              [&edition_name](WhatsNewEdition const& edition) {
                                return edition.GetFeatureName() == edition_name;
                              });
  if (edition != editions_.end()) {
    storage_service_->SetEditionUsed(edition_name);
  }
}

void WhatsNewRegistry::ClearUnregisteredModules() {
  for (auto& module_value : storage_service_->ReadModuleData()) {
    auto found_module = std::find_if(
        modules_.begin(), modules_.end(),
        [&module_value](WhatsNewModule const& module) {
          return module.HasFeature()
                     ? module.GetFeatureName() == module_value.GetString()
                     : false;
        });
    // If the stored module cannot be found in the current registered
    // modules, clear its data.
    if (found_module == modules_.end()) {
      storage_service_->ClearModule(module_value.GetString());
    }
  }
}

void WhatsNewRegistry::ClearUnregisteredEditions() {
  for (auto edition_value : storage_service_->ReadEditionData()) {
    auto found_edition =
        std::find_if(editions_.begin(), editions_.end(),
                     [&edition_value](WhatsNewEdition const& edition) {
                       return edition.GetFeatureName() == edition_value.first;
                     });
    // If the stored edition cannot be found in the current registered
    // editions, clear its data.
    if (found_edition == editions_.end()) {
      storage_service_->ClearEdition(edition_value.first);
    }
  }
}

void WhatsNewRegistry::ResetData() {
  storage_service_->Reset();
}

WhatsNewRegistry::WhatsNewRegistry(
    std::unique_ptr<WhatsNewStorageService> storage_service)
    : storage_service_(std::move(storage_service)) {}
WhatsNewRegistry::~WhatsNewRegistry() = default;

}  // namespace whats_new
