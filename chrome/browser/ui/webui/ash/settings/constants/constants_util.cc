// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/constants/constants_util.h"

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"

namespace ash::settings {
namespace {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

template <typename T>
std::vector<T> All() {
  int32_t min_value = static_cast<int32_t>(T::kMinValue);
  int32_t max_value = static_cast<int32_t>(T::kMaxValue);

  std::vector<T> all;
  for (int32_t i = min_value; i <= max_value; ++i) {
    T current = static_cast<T>(i);

    // Not every value between the min and max values is valid:
    // (1) We use a numbering scheme which purposely skips some values for the
    //     Subpage and Setting enums.
    // (2) Some values are deprecated and removed.
    if (chromeos::settings::mojom::IsKnownEnumValue(current)) {
      all.push_back(current);
    }
  }

  return all;
}

void IncludeRevampSectionsOnly(std::vector<mojom::Section>& sections) {
  auto isOldSection = [](mojom::Section section) {
    // TODO(b/292678609) Gradually add checks here to filter out old Sections
    // from the set of available Sections. An old Section can be filtered out
    // once it has been fully incorporated into the new revamp Section.
    return section == mojom::Section::kDateAndTime ||
           section == mojom::Section::kLanguagesAndInput ||
           section == mojom::Section::kReset ||
           section == mojom::Section::kSearchAndAssistant;
  };
  sections.erase(std::remove_if(sections.begin(), sections.end(), isOldSection),
                 sections.end());
}

void RemoveRevampSections(std::vector<mojom::Section>& sections) {
  auto isRevampSection = [](mojom::Section section) {
    return section == mojom::Section::kSystemPreferences;
  };
  sections.erase(
      std::remove_if(sections.begin(), sections.end(), isRevampSection),
      sections.end());
}

}  // namespace

const std::vector<mojom::Section>& AllSections() {
  static const base::NoDestructor<std::vector<mojom::Section>> all_sections([] {
    std::vector<mojom::Section> sections = All<mojom::Section>();
    if (ash::features::IsOsSettingsRevampWayfindingEnabled()) {
      IncludeRevampSectionsOnly(sections);
    } else {
      RemoveRevampSections(sections);
    }
    return sections;
  }());

  return *all_sections;
}

const std::vector<mojom::Subpage>& AllSubpages() {
  static const base::NoDestructor<std::vector<mojom::Subpage>> all_subpages(
      All<mojom::Subpage>());
  return *all_subpages;
}

const std::vector<mojom::Setting>& AllSettings() {
  static const base::NoDestructor<std::vector<mojom::Setting>> all_settings(
      All<mojom::Setting>());
  return *all_settings;
}

}  // namespace ash::settings
