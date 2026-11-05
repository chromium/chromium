// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace organizer {
namespace {

constexpr std::array<std::string_view, kSectionCount> kSectionIds = {
    kCrossDeviceTabsSectionId,
    kOpenTabsSectionId,
    kRecentlyClosedSectionId,
    kTabGroupsSectionId,
};

void CheckValidSectionId(std::string_view section_id) {
  CHECK(std::ranges::contains(kSectionIds, section_id))
      << "Unknown section ID \"" << section_id
      << "\". Please add new section IDs to kSectionIds.";
}

SectionState ReadSectionState(const base::DictValue& expanded_dict,
                              const base::DictValue& show_all_dict,
                              std::string_view section_id) {
  SectionState state;
  if (std::optional<bool> expanded = expanded_dict.FindBool(section_id)) {
    state.expanded = *expanded;
  }
  if (std::optional<bool> show_all = show_all_dict.FindBool(section_id)) {
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
                             std::string_view section_id) {
  CheckValidSectionId(section_id);
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
  for (size_t i = 0; i < kSectionIds.size(); ++i) {
    entries[i] = {
        .section_id = kSectionIds[i],
        .state = ReadSectionState(expanded_dict, show_all_dict, kSectionIds[i]),
    };
  }
  return entries;
}

void SetSectionExpanded(PrefService& prefs,
                        std::string_view section_id,
                        bool expanded) {
  CheckValidSectionId(section_id);
  ScopedDictPrefUpdate update(&prefs, prefs::kOrganizerPanelSectionsExpanded);
  update->Set(section_id, expanded);
}

void SetSectionShowAll(PrefService& prefs,
                       std::string_view section_id,
                       bool show_all) {
  CheckValidSectionId(section_id);
  ScopedDictPrefUpdate update(&prefs, prefs::kOrganizerPanelSectionsShowAll);
  update->Set(section_id, show_all);
}

}  // namespace organizer
