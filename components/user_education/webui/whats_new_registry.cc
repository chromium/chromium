// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/whats_new_registry.h"

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
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

const std::string WhatsNewModule::GetCustomization() const {
  if (!feature_) {
    return "";
  }
  std::string customization = base::GetFieldTrialParamValueByFeature(
      *feature_, whats_new::kCustomizationParam);
  return customization;
}

WhatsNewEdition::WhatsNewEdition(const base::Feature& feature,
                                 std::string owner,
                                 std::vector<BrowserCommand> browser_commands)
    : feature_(feature),
      unique_name_(feature.name),
      browser_commands_(browser_commands) {}

WhatsNewEdition::WhatsNewEdition(WhatsNewEdition&& other) = default;
WhatsNewEdition::~WhatsNewEdition() = default;

bool WhatsNewEdition::IsFeatureEnabled() const {
  return base::FeatureList::IsEnabled(*feature_);
}

const char* WhatsNewEdition::GetFeatureName() const {
  return feature_->name;
}

const std::string WhatsNewEdition::GetCustomization() const {
  std::string customization = base::GetFieldTrialParamValueByFeature(
      *feature_, whats_new::kCustomizationParam);
  return customization;
}

const std::optional<std::string> WhatsNewEdition::GetSurvey() const {
  std::string survey = base::GetFieldTrialParamValueByFeature(
      *feature_, whats_new::kSurveyParam);
  if (survey.empty()) {
    return std::nullopt;
  }
  return survey;
}

void WhatsNewRegistry::RegisterModule(WhatsNewModule module) {
  CHECK(!modules_.contains(module.unique_name()));
  if (module.HasFeature() && module.IsFeatureEnabled()) {
    storage_service_->SetModuleEnabled(module.GetFeatureName());
  }
  modules_.emplace(module.unique_name(), std::move(module));
}

void WhatsNewRegistry::RegisterEdition(WhatsNewEdition edition) {
  CHECK(!editions_.contains(edition.unique_name()));
  editions_.emplace(edition.unique_name(), std::move(edition));
}

const std::vector<BrowserCommand> WhatsNewRegistry::GetActiveCommands() const {
  std::vector<BrowserCommand> commands;
  for (const auto& [key, module] : modules_) {
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
  for (const auto& [key, edition] : editions_) {
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
    // An edition is tied to this milestone. Request this edition again.
    feature_names.emplace_back(*current_edition_name);
  } else if (!storage_service_->WasVersionPageUsedForCurrentMilestone()) {
    // Only request other unused editions if no other page was shown
    // during this milestone (version page or other edition page).
    for (const auto& [key, edition] : editions_) {
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
    auto found_module = modules_.find(module_value.GetString());
    if (found_module != modules_.end() &&
        found_module->second.HasActiveFeature()) {
      feature_names.emplace_back(found_module->second.GetFeatureName());
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
    auto found_module = modules_.find(module_value.GetString());
    if (found_module != modules_.end() &&
        found_module->second.HasRolledFeature()) {
      feature_names.emplace_back(found_module->second.GetFeatureName());
    }
  }

  return feature_names;
}

const std::vector<std::string> WhatsNewRegistry::GetCustomizations() const {
  std::vector<std::string> customizations;
  const base::Value::List& module_names_in_order =
      storage_service_->ReadModuleData();
  for (const base::Value& module_value : module_names_in_order) {
    auto found_module = modules_.find(module_value.GetString());
    if (found_module != modules_.end()) {
      auto module = found_module->second;

      // Modules without a feature are default-enabled.
      const bool module_is_default_enabled = !module.HasFeature();
      // If the module is tied to a feature, ensure the feature is available.
      const bool module_is_available =
          module.HasActiveFeature() || module.HasRolledFeature();
      if (module_is_default_enabled || module_is_available) {
        std::string customization = module.GetCustomization();
        if (!customization.empty()) {
          customizations.emplace_back(customization);
        }
      }
    }
  }
  for (const auto& [key, edition] : editions_) {
    // The only requirement for an edition to show is that it is enabled.
    if (edition.IsFeatureEnabled()) {
      const auto customization = edition.GetCustomization();
      if (!customization.empty()) {
        customizations.emplace_back(customization);
      }
    }
  }
  return customizations;
}

const std::optional<std::string> WhatsNewRegistry::GetActiveEditionSurvey()
    const {
  // Check if the current version is used for an edition.
  const auto current_edition_name =
      storage_service_->FindEditionForCurrentVersion();
  if (current_edition_name.has_value()) {
    auto found_edition = editions_.find(std::string(*current_edition_name));
    if (found_edition != editions_.end()) {
      return found_edition->second.GetSurvey();
    }
  } else {
    // Only request other unused editions if there was not one shown during
    // this version.
    for (const auto& [key, edition] : editions_) {
      if (edition.IsFeatureEnabled() &&
          !storage_service_->IsUsedEdition(edition.GetFeatureName())) {
        const auto survey = edition.GetSurvey();
        if (survey.has_value()) {
          return survey;
        }
      }
    }
  }

  return std::nullopt;
}

void WhatsNewRegistry::SetEditionUsed(std::string edition_name) const {
  // Verify edition exists.
  auto found_edition = editions_.find(edition_name);
  if (found_edition != editions_.end()) {
    storage_service_->SetEditionUsed(edition_name);
  }
}

void WhatsNewRegistry::SetVersionUsed() const {
  storage_service_->SetVersionUsed();
}

void WhatsNewRegistry::ClearUnregisteredModules() const {
  std::set<std::string_view> modules_to_clear;
  for (auto& module_value : storage_service_->ReadModuleData()) {
    auto found_module = modules_.find(module_value.GetString());
    // If the stored module cannot be found in the current registered
    // modules, clear its data.
    if (found_module == modules_.end()) {
      modules_to_clear.emplace(module_value.GetString());
    }
  }
  storage_service_->ClearModules(std::move(modules_to_clear));
}

void WhatsNewRegistry::ClearUnregisteredEditions() const {
  std::set<std::string_view> editions_to_clear;
  for (auto edition_value : storage_service_->ReadEditionData()) {
    auto found_edition = editions_.find(edition_value.first);
    // If the stored edition cannot be found in the current registered
    // editions, clear its data.
    if (found_edition == editions_.end()) {
      editions_to_clear.emplace(edition_value.first);
    }
  }
  storage_service_->ClearEditions(std::move(editions_to_clear));
}

void WhatsNewRegistry::ResetData() const {
  storage_service_->Reset();
}

WhatsNewRegistry::WhatsNewRegistry(
    std::unique_ptr<WhatsNewStorageService> storage_service)
    : storage_service_(std::move(storage_service)) {}
WhatsNewRegistry::~WhatsNewRegistry() = default;

}  // namespace whats_new
