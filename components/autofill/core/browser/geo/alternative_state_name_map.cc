// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

// Assuming a user can have maximum 500 profiles each containing a different
// state string in the worst case scenario.
constexpr int kMaxMapSize = 500;

// The characters to be removed from the state strings before the comparison.
constexpr char16_t kCharsToStrip[] = u".- ";

}  // namespace

// static
AlternativeStateNameMap* AlternativeStateNameMap::GetInstance() {
  static base::NoDestructor<AlternativeStateNameMap>
      g_alternative_state_name_map;
  return g_alternative_state_name_map.get();
}

// static
AlternativeStateNameMap::StateName AlternativeStateNameMap::NormalizeStateName(
    const StateName& text) {
  std::u16string normalized_text;
  base::RemoveChars(text.value(), kCharsToStrip, &normalized_text);
  return StateName(normalized_text);
}

// static
std::optional<AlternativeStateNameMap::CanonicalStateName>
AlternativeStateNameMap::GetCanonicalStateName(
    const std::string& country_code,
    const std::u16string& state_name) {
  return AlternativeStateNameMap::GetInstance()->GetCanonicalStateName(
      AlternativeStateNameMap::CountryCode(country_code),
      AlternativeStateNameMap::StateName(state_name));
}

AlternativeStateNameMap::AlternativeStateNameMap() = default;

std::optional<AlternativeStateNameMap::CanonicalStateName>
AlternativeStateNameMap::GetCanonicalStateName(
    const CountryCode& country_code,
    const StateName& state_name,
    bool is_state_name_normalized) const {
  // Example:
  //  Entries in |localized_state_names_map_| are:
  //    ("DE", "Bavaria") -> {
  //                           "canonical_name": "Bayern",
  //                           "abbreviations": "BY",
  //                           "alternative_names": "Bavaria"
  //                         }
  //  Entries in |localized_state_names_reverse_lookup_map_| are:
  //    ("DE", "Bayern") -> "Bayern"
  //    ("DE", "BY") -> "Bayern"
  //    ("DE", "Bavaria") -> "Bayern"
  //  then, AlternativeStateNameMap::GetCanonicalStateName("DE", "Bayern") =
  //        AlternativeStateNameMap::GetCanonicalStateName("DE", "BY") =
  //        AlternativeStateNameMap::GetCanonicalStateName("DE", "Bavaria") =
  //        CanonicalStateName("Bayern")
  StateName normalized_state_name = state_name;
  if (!is_state_name_normalized)
    normalized_state_name = NormalizeStateName(state_name);

  base::AutoLock lock(lock_);
  auto it = localized_state_names_reverse_lookup_map_.find(
      {country_code, normalized_state_name});
  if (it != localized_state_names_reverse_lookup_map_.end())
    return it->second;

  return std::nullopt;
}

std::optional<StateEntry> AlternativeStateNameMap::GetEntry(
    const CountryCode& country_code,
    const StateName& state_string_from_profile) const {
  StateName normalized_state_string_from_profile =
      NormalizeStateName(state_string_from_profile);
  std::optional<CanonicalStateName> canonical_state_name =
      GetCanonicalStateName(country_code, normalized_state_string_from_profile,
                            /*is_state_name_normalized=*/true);

  if (canonical_state_name) {
    base::AutoLock lock(lock_);
    auto it = localized_state_names_map_.find(
        {country_code, canonical_state_name.value()});
    if (it != localized_state_names_map_.end())
      return it->second;
  }

  return std::nullopt;
}

void AlternativeStateNameMap::AddEntry(
    const CountryCode& country_code,
    const StateName& normalized_state_value_from_profile,
    const StateEntry& state_entry,
    const std::vector<StateName>& normalized_alternative_state_names,
    const CanonicalStateName& normalized_canonical_state_name) {
  // Example:
  // AddEntry("DE", "Bavaria", {
  //                              "canonical_name": "Bayern",
  //                              "abbreviations": "BY",
  //                              "alternative_names": "Bavaria"
  //                            }, {"Bavaria", "BY", "Bayern"}, "Bayern")
  // Then entry added to |localized_state_names_map_| is:
  //    ("DE", "Bayern") -> {
  //                           "canonical_name": "Bayern",
  //                           "abbreviations": "BY",
  //                           "alternative_names": "Bavaria"
  //                         }
  //  Entries added to |localized_state_names_reverse_lookup_map_| are:
  //    ("DE", "Bayern") -> "Bayern"
  //    ("DE", "BY") -> "Bayern"
  //    ("DE", "Bavaria") -> "Bayern"
  if (GetCanonicalStateName(country_code, normalized_state_value_from_profile,
                            /*is_state_name_normalized=*/true)) {
    return;
  }

  base::AutoLock lock(lock_);
  if (localized_state_names_map_.size() == kMaxMapSize) {
    return;
  }
  localized_state_names_map_[{country_code, normalized_canonical_state_name}] =
      state_entry;
  for (const auto& alternative_name : normalized_alternative_state_names) {
    localized_state_names_reverse_lookup_map_[{
        country_code, alternative_name}] = normalized_canonical_state_name;
  }
}

bool AlternativeStateNameMap::IsLocalisedStateNamesMapEmpty() const {
  base::AutoLock lock(lock_);
  return localized_state_names_map_.empty();
}

void AlternativeStateNameMap::ClearAlternativeStateNameMap() {
  base::AutoLock lock(lock_);
  localized_state_names_map_.clear();
  localized_state_names_reverse_lookup_map_.clear();
}

}  // namespace autofill
