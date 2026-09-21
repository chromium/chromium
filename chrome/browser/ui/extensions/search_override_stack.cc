// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/search_override_stack.h"

#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/supports_user_data.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

constexpr char kStackStateHistogramName[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogStackState";
constexpr char kChainLengthHistogramName[] =
    "Extensions.SettingsOverridden."
    "SearchOverriddenDialogUnacknowledgedChainLength";
constexpr char kDialogResultHistogramPrefix[] =
    "Extensions.SettingsOverridden.SearchOverriddenDialogResult.";
constexpr char kProfileStackStateHistogramName[] =
    "Extensions.SettingsOverridden.DseExtensionStackState";

// Chain lengths at or above this land in the histogram's overflow bucket.
constexpr int kChainLengthExclusiveMax = 11;

constexpr char kRecordedMetricsDataKey[] = "search_override_stack_metrics";

// Tracks what was already recorded for the profile this session.
struct RecordedMetrics : public base::SupportsUserData::Data {
  // Controlling extensions the dialog stack metrics were recorded for.
  std::set<ExtensionId> recorded_ids;
  // Whether the profile's stack state was recorded.
  bool profile_stack_state_recorded = false;
};

RecordedMetrics& GetRecordedMetrics(Profile& profile) {
  auto* recorded = static_cast<RecordedMetrics*>(
      profile.GetUserData(kRecordedMetricsDataKey));
  if (!recorded) {
    auto new_recorded = std::make_unique<RecordedMetrics>();
    recorded = new_recorded.get();
    profile.SetUserData(kRecordedMetricsDataKey, std::move(new_recorded));
  }
  return *recorded;
}

// What stops the walk down the stack. Reverting can't get past either floor.
enum class Floor {
  kNone,
  kAcknowledged,
  kMustRemainEnabled,
};

Floor GetFloor(Profile& profile, const ExtensionId& extension_id) {
  // For historical reasons the search overridden dialog shares the
  // controlled-home dialog's acknowledgement preference, as
  // settings_overridden_params_providers.cc does; this is not a check of the
  // home dialog's own state.
  if (ExtensionSettingsOverriddenDialog::HasAcknowledgedExtension(
          profile, extension_id,
          ControlledHomeDialogController::kAcknowledgedPreference)) {
    return Floor::kAcknowledged;
  }
  const Extension* extension =
      ExtensionRegistry::Get(&profile)->enabled_extensions().GetByID(
          extension_id);
  if (extension &&
      ExtensionSystem::Get(&profile)->management_policy()->MustRemainEnabled(
          extension, /*error=*/nullptr)) {
    return Floor::kMustRemainEnabled;
  }
  return Floor::kNone;
}

// Must match the histogram's token variants in histograms.xml.
std::string_view GetStateVariant(SearchOverrideStackState state) {
  switch (state) {
    case SearchOverrideStackState::kSingleOverride:
      return "SingleOverride";
    case SearchOverrideStackState::kAcknowledgedExtensionNext:
      return "AcknowledgedExtensionNext";
    case SearchOverrideStackState::
        kUnacknowledgedChainOverAcknowledgedExtension:
      return "UnacknowledgedChainOverAcknowledgedExtension";
    case SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault:
      return "UnacknowledgedChainOverNonExtensionDefault";
    case SearchOverrideStackState::kMustRemainEnabledExtensionNext:
      return "MustRemainEnabledExtensionNext";
    case SearchOverrideStackState::
        kUnacknowledgedChainOverMustRemainEnabledExtension:
      return "UnacknowledgedChainOverMustRemainEnabledExtension";
  }
}

}  // namespace

std::optional<SearchOverrideStackInfo> GetSearchOverrideStackInfo(
    Profile& profile,
    const ExtensionId& controlling_extension_id) {
  ExtensionPrefValueMap* value_map =
      ExtensionPrefValueMapFactory::GetForBrowserContext(&profile);
  if (!value_map) {
    return std::nullopt;
  }

  // Same preference GetSecondarySearchInfo() consults, so this stack matches
  // the previous choice the dialog offers.
  const ExtensionIdList stack = value_map->GetExtensionsSettingPrefByPrecedence(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  if (stack.empty() || stack.front() != controlling_extension_id) {
    return std::nullopt;
  }

  SearchOverrideStackInfo info;
  if (stack.size() == 1) {
    info.state = SearchOverrideStackState::kSingleOverride;
    return info;
  }

  Floor floor = Floor::kNone;
  for (size_t i = 1; i < stack.size(); ++i) {
    floor = GetFloor(profile, stack[i]);
    if (floor != Floor::kNone) {
      break;
    }
    ++info.unacknowledged_chain_length;
  }

  const bool has_chain = info.unacknowledged_chain_length > 0;
  switch (floor) {
    case Floor::kNone:
      // Implies a chain: the loop breaks at the first floor it finds.
      info.state =
          SearchOverrideStackState::kUnacknowledgedChainOverNonExtensionDefault;
      break;
    case Floor::kAcknowledged:
      info.state = has_chain
                       ? SearchOverrideStackState::
                             kUnacknowledgedChainOverAcknowledgedExtension
                       : SearchOverrideStackState::kAcknowledgedExtensionNext;
      break;
    case Floor::kMustRemainEnabled:
      info.state =
          has_chain ? SearchOverrideStackState::
                          kUnacknowledgedChainOverMustRemainEnabledExtension
                    : SearchOverrideStackState::kMustRemainEnabledExtensionNext;
      break;
  }
  return info;
}

void RecordSearchOverrideStackMetricsOnce(
    Profile& profile,
    const ExtensionId& controlling_extension_id,
    const SearchOverrideStackInfo& info) {
  if (!GetRecordedMetrics(profile)
           .recorded_ids.insert(controlling_extension_id)
           .second) {
    return;
  }

  base::UmaHistogramEnumeration(kStackStateHistogramName, info.state);
  base::UmaHistogramExactLinear(kChainLengthHistogramName,
                                info.unacknowledged_chain_length,
                                kChainLengthExclusiveMax);
}

void RecordSearchOverrideStackDialogResult(
    const SearchOverrideStackInfo& info,
    SettingsOverriddenDialogController::DialogResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kDialogResultHistogramPrefix, GetStateVariant(info.state)}),
      result);
}

void RecordDseExtensionStackStateOnce(Profile& profile) {
  if (profile.IsOffTheRecord()) {
    return;
  }
  RecordedMetrics& recorded = GetRecordedMetrics(profile);
  if (recorded.profile_stack_state_recorded) {
    return;
  }
  const Extension* extension = GetExtensionOverridingSearchEngine(&profile);
  if (!extension) {
    return;
  }
  // An extension in the stack doesn't control search if a policy does.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  if (!template_url_service ||
      !template_url_service->IsExtensionControlledDefaultSearch()) {
    return;
  }
  std::optional<SearchOverrideStackInfo> info =
      GetSearchOverrideStackInfo(profile, extension->id());
  if (!info) {
    return;
  }
  // Mark only once there is something to record, so a window opened before an
  // extension took over doesn't consume the sample.
  recorded.profile_stack_state_recorded = true;
  base::UmaHistogramEnumeration(kProfileStackStateHistogramName, info->state);
}

}  // namespace extensions
