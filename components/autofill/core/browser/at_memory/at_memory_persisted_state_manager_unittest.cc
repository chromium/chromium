// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

class AtMemoryPersistedStateManagerTest : public testing::Test {
 public:
  AtMemoryPersistedStateManager& state_manager() { return state_manager_; }
  const FieldGlobalId& field_id() const { return field_id_; }
  const FieldGlobalId& other_field_id() const { return other_field_id_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AtMemoryPersistedStateManager state_manager_;
  FieldGlobalId field_id_{test::MakeFieldGlobalId()};
  FieldGlobalId other_field_id_{test::MakeFieldGlobalId()};
};

TEST_F(AtMemoryPersistedStateManagerTest, ReturnsNulloptForNewField) {
  EXPECT_EQ(state_manager().GetInitialStateForField(field_id()), std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest, OpenCloseWithoutEditReturnsNullopt) {
  EXPECT_EQ(state_manager().GetInitialStateForField(field_id()), std::nullopt);
  // Opening again on the same field without typing or searching returns
  // nullopt.
  EXPECT_EQ(state_manager().GetInitialStateForField(field_id()), std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest,
       StoresFilterAndSuggestionsForSameField) {
  state_manager().GetInitialStateForField(field_id());
  state_manager().OnFilterSubmitted(u"address");
  EXPECT_TRUE(state_manager().IsSearching());

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().StopSearching();
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemoryManagerState>& restored_state =
      state_manager().GetInitialStateForField(field_id());
  ASSERT_TRUE(restored_state.has_value());
  EXPECT_EQ(restored_state->filter, u"address");
  ASSERT_EQ(restored_state->suggestions.size(), 1u);
  EXPECT_EQ(restored_state->suggestions[0].main_text.value, u"123 Main St");
  EXPECT_FALSE(restored_state->is_searching);
}

TEST_F(AtMemoryPersistedStateManagerTest,
       ResetsStateWhenAccessingDifferentField) {
  state_manager().GetInitialStateForField(field_id());
  state_manager().OnFilterSubmitted(u"address");

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_EQ(state_manager().GetInitialStateForField(other_field_id()),
            std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest,
       OnFilterChangedClearsSuggestionsAndSearching) {
  state_manager().GetInitialStateForField(field_id());
  state_manager().OnFilterSubmitted(u"addr");
  EXPECT_TRUE(state_manager().IsSearching());

  state_manager().OnFilterChanged(u"add");
  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemoryManagerState>& state =
      state_manager().GetInitialStateForField(field_id());
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->filter, u"add");
  EXPECT_TRUE(state->suggestions.empty());
  EXPECT_FALSE(state->is_searching);

  // Clearing the filter resets state so that next open returns nullopt.
  state_manager().OnFilterChanged(u"");
  EXPECT_EQ(state_manager().GetInitialStateForField(field_id()), std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest, OnSuggestionAcceptedResetsState) {
  state_manager().GetInitialStateForField(field_id());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionAccepted();

  EXPECT_EQ(state_manager().GetInitialStateForField(field_id()), std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest,
       StopSearchingClearsIncompleteSuggestions) {
  state_manager().GetInitialStateForField(field_id());
  state_manager().OnFilterSubmitted(u"ongoing_query");
  EXPECT_TRUE(state_manager().IsSearching());

  std::vector<Suggestion> fetching_suggestions;
  fetching_suggestions.emplace_back(SuggestionType::kAtMemoryFetching);
  state_manager().OnSuggestionsChanged(fetching_suggestions);

  state_manager().StopSearching();
  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemoryManagerState>& restored_state =
      state_manager().GetInitialStateForField(field_id());
  ASSERT_TRUE(restored_state.has_value());
  EXPECT_EQ(restored_state->filter, u"ongoing_query");
  EXPECT_TRUE(restored_state->suggestions.empty());
  EXPECT_FALSE(restored_state->is_searching);
}

}  // namespace
}  // namespace autofill
