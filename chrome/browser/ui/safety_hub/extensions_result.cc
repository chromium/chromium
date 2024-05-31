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

namespace {
constexpr extensions::PrefMap kPrefAcknowledgeSafetyCheckWarning = {
    "ack_safety_check_warning", extensions::PrefType::kBool,
    extensions::PrefScope::kExtensionSpecific};

// Return true if an extension should be reviewed in the Safety Hub
// Extension review panel. Any extensions that fall into one of the
// following categories will return true.
// -- Malware extensions
// -- Extensions with a CWS policy violation
// -- Extensions that have been unpublished
// -- Extensions marked as unwanted
// -- Offstore extensions
// -- Developer has not disclosed privacy practices
bool ShouldExtensionBeReviewed(
    const extensions::Extension& extension,
    Profile* profile,
    const extensions::ExtensionPrefs* extension_prefs,
    const extensions::CWSInfoService* extension_info_service,
    bool consider_unpublished_only) {
  bool warning_acked = false;
  extension_prefs->ReadPrefAsBoolean(
      extension.id(), kPrefAcknowledgeSafetyCheckWarning, &warning_acked);
  bool is_extension = extension.is_extension() || extension.is_shared_module();
  bool is_non_visible_extension =
      extensions::Manifest::IsPolicyLocation(extension.location()) ||
      extensions::Manifest::IsComponentLocation(extension.location());
  // If the user has previously acknowledged the warning on this
  // extension and chosen to keep it, we will not show an additional
  // Safety Hub warning. We also will not show warnings on Chrome apps
  // or extensions that are not visible to the user.
  if (warning_acked || !is_extension || is_non_visible_extension) {
    return false;
  }
  std::optional<extensions::CWSInfoService::CWSInfo> extension_info =
      extension_info_service->GetCWSInfo(extension);
  // When only considering extensions that have been unpublished for a long
  // time, discard any other review reason. Note that extensions can have
  // multiple review reasons, and we're only returning the "main" one.
  if (consider_unpublished_only) {
    if (extension_info.has_value() && extension_info->unpublished_long_ago) {
      return true;
    }
    return false;
  }

  // Check the various categories to see if the extension needs to be reviewed.
  // Note that the order of the checks does not matter here since we only care
  // about a review/no-review decision and not the specific category that
  // requires the review.
  if (extension_info.has_value() && extension_info->is_present) {
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
    if (base::FeatureList::IsEnabled(
            features::kSafetyHubExtensionsNoPrivacyPracticesTrigger) &&
        extension_info->no_privacy_practice) {
      return true;
    }
  }

  // If an extension appears on the blocklist, that extension will be
  // marked for review. Currently, only malware, policy violation, and
  // potentially unwanted software blocklist states are marked for review.
  extensions::BitMapBlocklistState blocklist_state =
      extensions::blocklist_prefs::GetExtensionBlocklistState(extension.id(),
                                                              extension_prefs);
  switch (blocklist_state) {
    case extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE:
    case extensions::BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION:
      return true;
    case extensions::BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED:
      if (base::FeatureList::IsEnabled(
              features::kSafetyHubExtensionsUwSTrigger)) {
        return true;
      }
      break;
    case extensions::BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY:
    case extensions::BitMapBlocklistState::NOT_BLOCKLISTED:
      // no-op.
      break;
  }

  if (base::FeatureList::IsEnabled(
          features::kSafetyHubExtensionsOffStoreTrigger)) {
    // There is a chance that extensions installed by the command line
    // will not follow normal extension behavior for installing and
    // uninstalling. To avoid confusing the user, the Safety Hub
    // will not show command line extensions.
    if (extension.location() !=
        extensions::mojom::ManifestLocation::kCommandLine) {
      // Check to see if the extension is an offstore extension.
      if (extensions::Manifest::IsUnpackedLocation(extension.location())) {
        // Only consider offstore extensions if developer mode is disabled.
        bool dev_mode =
            profile->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode);
        return !dev_mode;
      }
      extensions::ExtensionManagement* extension_management =
          extensions::ExtensionManagementFactory::GetForBrowserContext(profile);
      if (!extension_management->UpdatesFromWebstore(extension)) {
        // Extension does not update from the webstore.
        return true;
      }

      if (extension_info.has_value() && !extension_info->is_present) {
        // Handles the edge case where Chrome thinks that the extension is
        // updating from the webstore but CWS has no knowledge of the extension.
        return true;
      }
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
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult& SafetyHubExtensionsResult::operator=(
    const SafetyHubExtensionsResult&) = default;
SafetyHubExtensionsResult::~SafetyHubExtensionsResult() = default;

// static
std::optional<std::unique_ptr<SafetyHubService::Result>>
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
      if (!ShouldExtensionBeReviewed(*extension.get(), profile, extension_prefs,
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

void SafetyHubExtensionsResult::OnExtensionPrefsUpdated(
    const std::string& extension_id,
    Profile* profile,
    const extensions::CWSInfoService* extension_info_service) {
  auto extension_ptr = triggering_extensions_.find(extension_id);
  if (extension_ptr != triggering_extensions_.end()) {
    extensions::ExtensionPrefs* extension_prefs =
        extensions::ExtensionPrefsFactory::GetForBrowserContext(profile);
    bool warning_acked = false;
    extension_prefs->ReadPrefAsBoolean(
        extension_id, kPrefAcknowledgeSafetyCheckWarning, &warning_acked);
    if (warning_acked) {
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
