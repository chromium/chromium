// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map.h"

namespace autofill {

namespace test {

void PopulateStateEntry(const TestStateEntry& test_state_entry,
                        StateEntry* state_entry) {
  state_entry->set_canonical_name(test_state_entry.canonical_name);
  for (const auto& abbr : test_state_entry.abbreviations)
    state_entry->add_abbreviations(abbr);
  for (const auto& alternative_name : test_state_entry.alternative_names)
    state_entry->add_alternative_names(alternative_name);
}

void ClearAlternativeStateNameMapForTesting() {
  AlternativeStateNameMap::GetInstance()
      ->ClearAlternativeStateNameMapForTesting();
}

void PopulateAlternativeStateNameMapForTesting(
    const std::string& country_code,
    const std::string& key,
    const std::vector<TestStateEntry>& test_state_entries) {
  for (const auto& test_state_entry : test_state_entries) {
    StateEntry state_entry;
    PopulateStateEntry(test_state_entry, &state_entry);
    std::vector<AlternativeStateNameMap::StateName> alternatives;
    AlternativeStateNameMap::CanonicalStateName canonical_state_name =
        AlternativeStateNameMap::CanonicalStateName(
            base::ASCIIToUTF16(test_state_entry.canonical_name));
    alternatives.emplace_back(
        AlternativeStateNameMap::StateName(canonical_state_name.value()));
    for (const auto& abbr : test_state_entry.abbreviations)
      alternatives.emplace_back(
          AlternativeStateNameMap::StateName(base::ASCIIToUTF16(abbr)));
    for (const auto& alternative_name : test_state_entry.alternative_names)
      alternatives.emplace_back(AlternativeStateNameMap::StateName(
          base::ASCIIToUTF16(alternative_name)));

    AlternativeStateNameMap::GetInstance()->AddEntry(
        AlternativeStateNameMap::CountryCode(country_code),
        AlternativeStateNameMap::StateName(base::ASCIIToUTF16(key)),
        state_entry, alternatives, &canonical_state_name);
  }
}

std::string CreateStatesProtoAsString(const std::string& country_code,
                                      const TestStateEntry& test_state_entry) {
  StatesInCountry states_data;
  states_data.set_country_code(std::move(country_code));
  StateEntry* entry = states_data.add_states();
  PopulateStateEntry(test_state_entry, entry);

  std::string serialized_output;
  bool proto_is_serialized = states_data.SerializeToString(&serialized_output);
  DCHECK(proto_is_serialized);
  return serialized_output;
}

}  // namespace test
}  // namespace autofill
