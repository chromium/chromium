// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace organizer {

// LINT.IfChange(OrganizerSection)
enum class SectionId {
  kCrossDeviceTabs,
  kOpenTabs,
  kRecentlyClosed,
  kTabGroups,
  kMaxValue = kTabGroups,
};
// LINT.ThenChange(//chrome/browser/ui/tabs/organizer/organizer_panel_controller.cc:OrganizerSection)

inline constexpr size_t kSectionCount =
    std::to_underlying(SectionId::kMaxValue) + 1;

// Persisted display state for an organizer panel section.
// Defaults match `SectionState` in organizer_panel.mojom.
struct SectionState {
  bool expanded = true;
  bool show_all = false;
};

// Persisted display state and ID for an organizer panel section.
struct SectionStateEntry {
  SectionId section_id;
  SectionState state;
};

// Registers Organizer Panel specific prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Converts between `SectionId` and its string representation.
std::string_view GetSectionIdString(SectionId section_id);
SectionId GetSectionIdFromString(std::string_view section_id);

// Returns the persisted display state for `section_id` from `prefs`.
SectionState GetSectionState(const PrefService& prefs, SectionId section_id);

// Returns persisted display state for all organizer sections.
std::array<SectionStateEntry, kSectionCount> GetAllSectionStates(
    const PrefService& prefs);

// Persists whether the section with `section_id` is expanded.
void SetSectionExpanded(PrefService& prefs,
                        SectionId section_id,
                        bool expanded);

// Persists whether the section with `section_id` shows all items.
void SetSectionShowAll(PrefService& prefs, SectionId section_id, bool show_all);

}  // namespace organizer

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_
