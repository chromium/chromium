// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

// Assuming a user can have maximum 500 profiles each containing a different
// state string in the worst case scenario.
constexpr int kMaxMapSize = 500;

// The characters to be removed from the state strings before the comparison.
constexpr char kCharsToStrip[] = ".- ";

}  // namespace

// static
AlternativeStateNameMap* AlternativeStateNameMap::GetInstance() {
  static base::NoDestructor<AlternativeStateNameMap>
      g_alternative_state_name_map;
  return g_alternative_state_name_map.get();
}

AlternativeStateNameMap::AlternativeStateNameMap() = default;

// static
AlternativeStateNameMap::StateName AlternativeStateNameMap::NormalizeStateName(
    const StateName& text) {
  base::string16 normalized_text;
  base::RemoveChars(text.value(), base::ASCIIToUTF16(kCharsToStrip),
                    &normalized_text);
  return StateName(normalized_text);
}

base::Optional<AlternativeStateNameMap::CanonicalStateName>
AlternativeStateNameMap::GetCanonicalStateName(
    const CountryCode& country_code,
    const StateName& state_name,
    bool is_state_name_normalized) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(alternative_state_name_map_sequence_checker_);
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

  auto it = localized_state_names_reverse_lookup_map_.find(
      {country_code, normalized_state_name});
  if (it != localized_state_names_reverse_lookup_map_.end())
    return it->second;

  return base::nullopt;
}

base::Optional<StateEntry> AlternativeStateNameMap::GetEntry(
    const CountryCode& country_code,
    const StateName& state_string_from_profile) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(alternative_state_name_map_sequence_checker_);

  StateName normalized_state_string_from_profile =
      NormalizeStateName(state_string_from_profile);
  base::Optional<CanonicalStateName> canonical_state_name =
      GetCanonicalStateName(country_code, normalized_state_string_from_profile,
                            /*is_state_name_normalized=*/true);

  if (!canonical_state_name) {
    canonical_state_name =
        CanonicalStateName(normalized_state_string_from_profile.value());
  }

  DCHECK(canonical_state_name);
  auto it = localized_state_names_map_.find(
      {country_code, canonical_state_name.value()});
  if (it != localized_state_names_map_.end())
    return it->second;

  return base::nullopt;
}

void AlternativeStateNameMap::AddEntry(
    const CountryCode& country_code,
    const StateName& normalized_state_value_from_profile,
    const StateEntry& state_entry,
    const std::vector<StateName>& normalized_alternative_state_names,
    CanonicalStateName* normalized_canonical_state_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(alternative_state_name_map_sequence_checker_);

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

  if (localized_state_names_map_.size() == kMaxMapSize ||
      GetCanonicalStateName(country_code, normalized_state_value_from_profile,
                            /*is_state_name_normalized=*/true)) {
    return;
  }

  if (normalized_canonical_state_name) {
    localized_state_names_map_[{
        country_code, *normalized_canonical_state_name}] = state_entry;
    for (const auto& alternative_name : normalized_alternative_state_names) {
      localized_state_names_reverse_lookup_map_[{
          country_code, alternative_name}] = *normalized_canonical_state_name;
    }
  } else {
    localized_state_names_map_[{
        country_code,
        CanonicalStateName(normalized_state_value_from_profile.value())}] =
        state_entry;
  }
}

bool AlternativeStateNameMap::IsLocalisedStateNamesMapEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(alternative_state_name_map_sequence_checker_);
  return localized_state_names_map_.empty();
}

void AlternativeStateNameMap::ClearAlternativeStateNameMap() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(alternative_state_name_map_sequence_checker_);
  localized_state_names_map_.clear();
  localized_state_names_reverse_lookup_map_.clear();
}

}  // namespace autofill
