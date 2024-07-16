// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/extensions_result.h"

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_safety_check_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace developer = extensions::api::developer_private;

SafetyHubExtensionsResult::SafetyHubExtensionsResult(
    std::set<extensions::ExtensionId> triggering_extensions,
    bool is_unpublished_extensions_only)
    : triggering_extensions_(triggering_extensions),
      is_unpublished_extensions_only_(is_unpublished_extensions_only) {}

SafetyHubExtensionsResult::SafetyHubExtensionsResult(
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult& SafetyHubExtensionsResult::operator=(
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult::~SafetyHubExtensionsResult() = default;

// static
std::optional<std::unique_ptr<SafetyHubService::Result>>
SafetyHubExtensionsResult::GetResult(
    Profile* profile,
    bool only_unpublished_extensions = false) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  std::set<extensions::ExtensionId> triggering_extensions;
  const extensions::ExtensionSet all_installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();
  // If `only_unpublished_extensions` is true, GetSafetyCheckWarningReason
  // will ignore other extension warning types and only consider
  // unpublished extensions.
  for (const auto& extension : all_installed_extensions) {
    developer::SafetyCheckWarningReason warning_reason =
        extensions::ExtensionSafetyCheckUtils::GetSafetyCheckWarningReason(
            *extension.get(), profile, only_unpublished_extensions);
    if (warning_reason != developer::SafetyCheckWarningReason::kNone) {
      triggering_extensions.insert(std::move(extension->id()));
    }
  }
  return std::make_unique<SafetyHubExtensionsResult>(
      triggering_extensions, only_unpublished_extensions);
}

std::unique_ptr<SafetyHubService::Result> SafetyHubExtensionsResult::Clone()
    const {
  return std::make_unique<SafetyHubExtensionsResult>(*this);
}

void SafetyHubExtensionsResult::OnExtensionPrefsUpdated(
    const std::string& extension_id,
    Profile* profile) {
  auto extension_ptr = triggering_extensions_.find(extension_id);
  if (extension_ptr != triggering_extensions_.end()) {
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::EVERYTHING);
    // If the extension is NULL it has been uninstalled and should be
    // removed from `triggering_extensions_`.
    if (!extension) {
      triggering_extensions_.erase(extension_ptr);
      return;
    }
    developer::SafetyCheckWarningReason warning_reason =
        extensions::ExtensionSafetyCheckUtils::GetSafetyCheckWarningReason(
            *extension, profile);
    if (warning_reason == developer::SafetyCheckWarningReason::kNone) {
      triggering_extensions_.erase(extension_ptr);
    }
  }
}

void SafetyHubExtensionsResult::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  auto extension_ptr = triggering_extensions_.find(extension->id());
  if (extension_ptr != triggering_extensions_.end()) {
    triggering_extensions_.erase(extension_ptr);
  }
}

base::Value::Dict SafetyHubExtensionsResult::ToDictValue() const {
  // Only results that contain extensions that have been unpublished for a long
  // time should be serialized.
  CHECK(is_unpublished_extensions_only_);
  base::Value::Dict result = BaseToDictValue();
  base::Value::List extensions_list;
  for (const auto& triggering_extension : triggering_extensions_) {
    extensions_list.Append(triggering_extension);
  }
  result.Set(safety_hub::kSafetyHubTriggeringExtensionIdsKey,
             std::move(extensions_list));
  return result;
}

bool SafetyHubExtensionsResult::IsTriggerForMenuNotification() const {
  // Only results that have unpublished extensions can result in a menu
  // notification.
  return is_unpublished_extensions_only_ && !triggering_extensions_.empty();
}

unsigned int SafetyHubExtensionsResult::GetNumTriggeringExtensions() const {
  return triggering_extensions_.size();
}

bool SafetyHubExtensionsResult::WarrantsNewMenuNotification(
    const base::Value::Dict& previous_result_dict) const {
  std::set<extensions::ExtensionId> previous_triggering_extensions;
  for (const base::Value& extension_id : *previous_result_dict.FindList(
           safety_hub::kSafetyHubTriggeringExtensionIdsKey)) {
    previous_triggering_extensions.insert(extension_id.GetString());
  }
  // Only results that are for unpublished extensions can result in a menu
  // notification.
  if (!is_unpublished_extensions_only_) {
    return false;
  }
  return !base::ranges::includes(previous_triggering_extensions,
                                 triggering_extensions_);
}

std::u16string SafetyHubExtensionsResult::GetNotificationString() const {
  CHECK(is_unpublished_extensions_only_);
  return l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MENU_NOTIFICATION,
      triggering_extensions_.size());
}

int SafetyHubExtensionsResult::GetNotificationCommandId() const {
  CHECK(is_unpublished_extensions_only_);
  return IDC_SAFETY_HUB_MANAGE_EXTENSIONS;
}

void SafetyHubExtensionsResult::ClearTriggeringExtensionsForTesting() {
  triggering_extensions_.clear();
}

void SafetyHubExtensionsResult::SetTriggeringExtensionForTesting(
    std::string extension_id) {
  triggering_extensions_.insert(extension_id);
}
