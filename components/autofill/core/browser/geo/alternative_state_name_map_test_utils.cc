// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"

#include "base/strings/utf_string_conversions.h"

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

AlternativeStateNameMap::StateName NormalizeAndConvertToUTF16(
    const std::string& text) {
  return AlternativeStateNameMap::NormalizeStateName(
      AlternativeStateNameMap::StateName(base::UTF8ToUTF16(text)));
}

void PopulateAlternativeStateNameMapForTesting(
    const std::string& country_code,
    const std::string& key,
    const std::vector<TestStateEntry>& test_state_entries) {
  for (const auto& test_state_entry : test_state_entries) {
    StateEntry state_entry;
    PopulateStateEntry(test_state_entry, &state_entry);
    std::vector<AlternativeStateNameMap::StateName> alternatives;
    alternatives.emplace_back(
        NormalizeAndConvertToUTF16(test_state_entry.canonical_name));
    AlternativeStateNameMap::CanonicalStateName canonical_state_name;
    if (!alternatives.empty()) {
      canonical_state_name = AlternativeStateNameMap::CanonicalStateName(
          alternatives.back().value());
    }

    for (const auto& abbr : test_state_entry.abbreviations)
      alternatives.emplace_back(NormalizeAndConvertToUTF16(abbr));
    for (const auto& alternative_name : test_state_entry.alternative_names) {
      alternatives.emplace_back(NormalizeAndConvertToUTF16(alternative_name));
    }

    AlternativeStateNameMap::GetInstance()->AddEntry(
        AlternativeStateNameMap::CountryCode(country_code),
        AlternativeStateNameMap::StateName(base::UTF8ToUTF16(key)), state_entry,
        alternatives, canonical_state_name);
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
