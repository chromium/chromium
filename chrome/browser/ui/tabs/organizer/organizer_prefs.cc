// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom-shared.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace organizer {
namespace {

constexpr std::string_view kCrossDeviceTabsSectionId = "cross-device-tabs";
constexpr std::string_view kOpenTabsSectionId = "open-tabs";
constexpr std::string_view kRecentlyClosedSectionId = "recently-closed";
constexpr std::string_view kTabGroupsSectionId = "tab-groups";

std::string_view GetSectionIdString(
    organizer_panel::mojom::SectionId section_id) {
  switch (section_id) {
    case organizer_panel::mojom::SectionId::kCrossDeviceTabs:
      return kCrossDeviceTabsSectionId;
    case organizer_panel::mojom::SectionId::kOpenTabs:
      return kOpenTabsSectionId;
    case organizer_panel::mojom::SectionId::kRecentlyClosed:
      return kRecentlyClosedSectionId;
    case organizer_panel::mojom::SectionId::kTabGroups:
      return kTabGroupsSectionId;
  }
}

SectionState ReadSectionState(const base::DictValue& expanded_dict,
                              const base::DictValue& show_all_dict,
                              organizer_panel::mojom::SectionId section_id) {
  const std::string_view section_key = GetSectionIdString(section_id);
  SectionState state;
  if (std::optional<bool> expanded = expanded_dict.FindBool(section_key)) {
    state.expanded = *expanded;
  }
  if (std::optional<bool> show_all = show_all_dict.FindBool(section_key)) {
    state.show_all = *show_all;
  }
  return state;
}

}  // namespace

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kOrganizerPanelEntrypointEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kOrganizerPanelSectionsExpanded);
  registry->RegisterDictionaryPref(prefs::kOrganizerPanelSectionsShowAll);
}

SectionState GetSectionState(const PrefService& prefs,
                             organizer_panel::mojom::SectionId section_id) {
  return ReadSectionState(prefs.GetDict(prefs::kOrganizerPanelSectionsExpanded),
                          prefs.GetDict(prefs::kOrganizerPanelSectionsShowAll),
                          section_id);
}

std::array<SectionStateEntry, kSectionCount> GetAllSectionStates(
    const PrefService& prefs) {
  const base::DictValue& expanded_dict =
      prefs.GetDict(prefs::kOrganizerPanelSectionsExpanded);
  const base::DictValue& show_all_dict =
      prefs.GetDict(prefs::kOrganizerPanelSectionsShowAll);
  std::array<SectionStateEntry, kSectionCount> entries;
  for (size_t i = 0; i < kSectionCount; ++i) {
    const auto section_id = static_cast<organizer_panel::mojom::SectionId>(i);
    entries[i] = {
        .section_id = section_id,
        .state = ReadSectionState(expanded_dict, show_all_dict, section_id),
    };
  }
  return entries;
}

void SetSectionExpanded(PrefService& prefs,
                        organizer_panel::mojom::SectionId section_id,
                        bool expanded) {
  ScopedDictPrefUpdate update(&prefs, prefs::kOrganizerPanelSectionsExpanded);
  update->Set(GetSectionIdString(section_id), expanded);
}

void SetSectionShowAll(PrefService& prefs,
                       organizer_panel::mojom::SectionId section_id,
                       bool show_all) {
  ScopedDictPrefUpdate update(&prefs, prefs::kOrganizerPanelSectionsShowAll);
  update->Set(GetSectionIdString(section_id), show_all);
}

}  // namespace organizer
