// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/extensions_result.h"

#include <memory>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr extensions::PrefMap kPrefAcknowledgeSafetyCheckWarning = {
    "ack_safety_check_warning", extensions::PrefType::kBool,
    extensions::PrefScope::kExtensionSpecific};

bool ShouldExtensionBeReviewed(
    const extensions::Extension& extension,
    const extensions::ExtensionPrefs* extension_prefs,
    const extensions::CWSInfoService* extension_info_service,
    bool consider_unpublished_only) {
  bool warning_acked = false;
  extension_prefs->ReadPrefAsBoolean(
      extension.id(), kPrefAcknowledgeSafetyCheckWarning, &warning_acked);
  bool is_extension = extension.is_extension() || extension.is_shared_module();
  // If the user has previously acknowledged the warning on this
  // extension and chosen to keep it, we will not show an additional
  // Safety Hub warning. We also will not show warnings on Chrome apps.
  if (warning_acked || !is_extension) {
    return false;
  }
  absl::optional<extensions::CWSInfoService::CWSInfo> extension_info =
      extension_info_service->GetCWSInfo(extension);
  if (extension_info.has_value() && extension_info->is_present) {
    // When only considering extensions that have been unpublished for a long
    // time, discard any other review reason. Note that extensions can have
    // multiple review reasons, and we're only returning the "main" one.
    if (consider_unpublished_only) {
      if (extension_info->unpublished_long_ago) {
        return true;
      }
      return false;
    }
    switch (extension_info->violation_type) {
      case extensions::CWSInfoService::CWSViolationType::kMalware:
      case extensions::CWSInfoService::CWSViolationType::kPolicy:
        return true;
      case extensions::CWSInfoService::CWSViolationType::kNone:
      case extensions::CWSInfoService::CWSViolationType::kMinorPolicy:
      case extensions::CWSInfoService::CWSViolationType::kUnknown:
        if (extension_info->unpublished_long_ago) {
          return true;
        }
        break;
    }
  }
  return false;
}
}  // namespace

SafetyHubExtensionsResult::SafetyHubExtensionsResult(
    std::set<extensions::ExtensionId> triggering_extensions,
    bool is_unpublished_extensions_only)
    : triggering_extensions_(triggering_extensions),
      is_unpublished_extensions_only_(is_unpublished_extensions_only) {}

SafetyHubExtensionsResult::SafetyHubExtensionsResult(
    const base::Value::Dict& dict) {
  for (const base::Value& extension_id :
       *dict.FindList(safety_hub::kSafetyHubTriggeringExtensionIdsKey)) {
    triggering_extensions_.insert(extension_id.GetString());
  }
  // Only results that contain unpublished extensions should be created with
  // this constructor.
  is_unpublished_extensions_only_ = true;
}

SafetyHubExtensionsResult::SafetyHubExtensionsResult(
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult& SafetyHubExtensionsResult::operator=(
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult::~SafetyHubExtensionsResult() = default;

// static
absl::optional<std::unique_ptr<SafetyHubService::Result>>
SafetyHubExtensionsResult::GetResult(
    const extensions::CWSInfoService* extension_info_service,
    Profile* profile,
    bool only_unpublished_extensions = false) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefsFactory::GetForBrowserContext(profile);
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  std::set<extensions::ExtensionId> triggering_extensions;
  if (base::FeatureList::IsEnabled(extensions::kCWSInfoService)) {
    const extensions::ExtensionSet all_installed_extensions =
        extension_registry->GenerateInstalledExtensionsSet();
    for (const auto& extension : all_installed_extensions) {
      // Check if the extension is installed by a policy.
      if (extensions::Manifest::IsPolicyLocation(extension->location())) {
        continue;
      }

      if (!ShouldExtensionBeReviewed(*extension.get(), extension_prefs,
                                     extension_info_service,
                                     only_unpublished_extensions)) {
        continue;
      }
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
    const Result& previousResult) const {
  const auto& previous =
      static_cast<const SafetyHubExtensionsResult&>(previousResult);
  // Only results that are for unpublished extensions can result in a menu
  // notification.
  if (!is_unpublished_extensions_only_ ||
      !previous.is_unpublished_extensions_only_) {
    return false;
  }
  return !base::ranges::includes(previous.triggering_extensions_,
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
  return IDC_MANAGE_EXTENSIONS;
}
