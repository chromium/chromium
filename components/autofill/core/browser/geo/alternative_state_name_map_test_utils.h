// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_

#include "components/autofill/core/browser/geo/alternative_state_name_map.h"
#include "components/autofill/core/browser/proto/states.pb.h"

namespace autofill {

namespace test {

namespace internal {
template <typename = void>
struct TestStateEntry {
  std::string canonical_name = "Bavaria";
  std::vector<std::string> abbreviations = {"BY"};
  std::vector<std::string> alternative_names = {"Bayern"};
};
}  // namespace internal

using TestStateEntry = internal::TestStateEntry<>;

// Populates |state_entry| with the data in |test_state_entry|.
void PopulateStateEntry(const TestStateEntry& test_state_entry,
                        StateEntry* state_entry);

// Clears the map for testing purposes.
void ClearAlternativeStateNameMapForTesting();

// Normalizes the text using |AlternativeStateNameMap::NormalizeStateName()|.
AlternativeStateNameMap::StateName NormalizeAndConvertToUTF16(
    const std::string& text);

// Inserts a StateEntry instance into AlternativeStateNameMap for testing.
void PopulateAlternativeStateNameMapForTesting(
    const std::string& country_code = "DE",
    const std::string& key = "Bavaria",
    const std::vector<TestStateEntry>& test_state_entries = {TestStateEntry()});

// Returns a StateEntry instance serialized as string.
std::string CreateStatesProtoAsString(
    const std::string& country_code = "DE",
    const TestStateEntry& test_state_entry = TestStateEntry());

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_
