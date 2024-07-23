// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/whats_new_registry.h"

#include <vector>

#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace whats_new {
using BrowserCommand = browser_command::mojom::Command;

bool WhatsNewModule::HasActiveFeature() const {
  return base::FeatureList::IsEnabled(*feature_) &&
         feature_->default_state == base::FEATURE_DISABLED_BY_DEFAULT;
}

bool WhatsNewModule::HasRolledFeature() const {
  return feature_->default_state == base::FEATURE_ENABLED_BY_DEFAULT;
}

const char* WhatsNewModule::GetFeatureName() const {
  return feature_->name;
}

bool WhatsNewModule::IsAvailable() const {
  return HasActiveFeature() || HasRolledFeature();
}

void WhatsNewRegistry::RegisterModule(WhatsNewModule module) {
  modules_.emplace_back(std::move(module));
}

void WhatsNewRegistry::RegisterEdition(WhatsNewEdition edition) {
  editions_.emplace_back(std::move(edition));
}

const std::vector<BrowserCommand> WhatsNewRegistry::GetActiveCommands() const {
  std::vector<BrowserCommand> commands;
  base::ranges::for_each(modules_, [&commands](const WhatsNewModule& module) {
    auto browser_command = module.browser_command();
    if (module.IsAvailable() && browser_command.has_value()) {
      commands.emplace_back(browser_command.value());
    }
  });
  return commands;
}

const std::vector<std::string_view> WhatsNewRegistry::GetActiveFeatureNames()
    const {
  std::vector<std::string_view> feature_names;
  base::ranges::for_each(
      modules_, [&feature_names](const WhatsNewModule& module) {
        if (module.HasActiveFeature()) {
          feature_names.emplace_back(module.GetFeatureName());
        }
      });
  return feature_names;
}

const std::vector<std::string_view> WhatsNewRegistry::GetRolledFeatureNames()
    const {
  std::vector<std::string_view> feature_names;
  base::ranges::for_each(
      modules_, [&feature_names](const WhatsNewModule& module) {
        if (module.HasRolledFeature()) {
          feature_names.emplace_back(module.GetFeatureName());
        }
      });
  return feature_names;
}

WhatsNewRegistry::WhatsNewRegistry() = default;
WhatsNewRegistry::WhatsNewRegistry(WhatsNewRegistry&& other) noexcept = default;
WhatsNewRegistry::~WhatsNewRegistry() = default;

}  // namespace whats_new
