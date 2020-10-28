// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_

#include "base/optional.h"
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

// Creates a fake |StateEntry|.
void CreateFakeStateEntry(const TestStateEntry& test_state_entry,
                          StateEntry* state_entry);

// Clears the map for testing purposes.
void ClearAlternativeStateNameMapForTesting();

// Inserts a fake |StateEntry| object into AlternativeStateNameMap for testing.
void PopulateAlternativeStateNameMapForTesting(
    std::string country_code = "DE",
    std::string key = "Bavaria",
    std::vector<TestStateEntry> test_state_entries = {TestStateEntry()});

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_ALTERNATIVE_STATE_NAME_MAP_TEST_UTILS_H_
