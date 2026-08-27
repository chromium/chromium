// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

url::Origin FieldOrigin() {
  return url::Origin::Create(GURL("https://example.com"));
}

url::Origin OtherFieldOrigin() {
  return url::Origin::Create(GURL("https://other.com"));
}

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

// Tests that accessing a new field returns `std::nullopt`.
TEST_F(AtMemoryPersistedStateManagerTest, ReturnsNulloptForNewField) {
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that opening and closing without editing returns `std::nullopt`.
TEST_F(AtMemoryPersistedStateManagerTest, OpenCloseWithoutEditReturnsNullopt) {
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
  // Opening again on the same field without typing or searching returns
  // nullopt.
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that filter and retrieved suggestions are stored and restored for the
// same field.
TEST_F(AtMemoryPersistedStateManagerTest,
       StoresFilterAndSuggestionsForSameField) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  EXPECT_TRUE(state_manager().IsSearching());

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().StopSearching();
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemorySearchState>& restored_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  ASSERT_TRUE(restored_state.has_value());
  EXPECT_EQ(restored_state->filter, u"address");
  ASSERT_EQ(restored_state->suggestions.size(), 1u);
  EXPECT_EQ(restored_state->suggestions[0].main_text.value, u"123 Main St");
  EXPECT_FALSE(restored_state->is_searching);
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that accessing a different field resets the persisted search state.
TEST_F(AtMemoryPersistedStateManagerTest,
       ResetsStateWhenAccessingDifferentField) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_EQ(state_manager().GetStateForField(other_field_id(),
                                                    OtherFieldOrigin()),
            std::nullopt);
  EXPECT_EQ(state_manager().field_origin(), OtherFieldOrigin());
}

// Tests that changing the filter clears retrieved suggestions and search state.
TEST_F(AtMemoryPersistedStateManagerTest,
       OnFilterChangedClearsSuggestionsAndSearching) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"addr");
  EXPECT_TRUE(state_manager().IsSearching());

  state_manager().OnFilterChanged(u"add");
  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemorySearchState>& state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  ASSERT_TRUE(state.has_value());
  EXPECT_EQ(state->filter, u"add");
  EXPECT_TRUE(state->suggestions.empty());
  EXPECT_FALSE(state->is_searching);

  // Clearing the filter resets state so that next open returns nullopt.
  state_manager().OnFilterChanged(u"");
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
}

TEST_F(AtMemoryPersistedStateManagerTest,
       OnSuggestionAcceptedResetsSearchState) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));

  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that stopping an ongoing search clears incomplete suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       StopSearchingClearsIncompleteSuggestions) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"ongoing_query");
  EXPECT_TRUE(state_manager().IsSearching());

  std::vector<Suggestion> fetching_suggestions;
  fetching_suggestions.emplace_back(SuggestionType::kAtMemoryFetching);
  state_manager().OnSuggestionsChanged(fetching_suggestions);

  state_manager().StopSearching();
  EXPECT_FALSE(state_manager().IsSearching());

  const std::optional<AtMemorySearchState>& restored_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  ASSERT_TRUE(restored_state.has_value());
  EXPECT_EQ(restored_state->filter, u"ongoing_query");
  EXPECT_TRUE(restored_state->suggestions.empty());
  EXPECT_FALSE(restored_state->is_searching);
}

// Tests that `OnSuggestionAccepted` stores accepted suggestions in
// `previously_filled_suggestions` when `kAutofillAtMemoryPreviouslyFilled`
// is enabled.
TEST_F(AtMemoryPersistedStateManagerTest, StoresPreviouslyFilledSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  Suggestion s2(u"Suggestion 2", SuggestionType::kAtMemorySearchResult);

  state_manager().OnSuggestionAccepted(s1);
  state_manager().OnSuggestionAccepted(s2);

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Suggestion 1");
  EXPECT_EQ(state_manager().previously_filled_suggestions()[1].main_text.value,
            u"Suggestion 2");
}

// Tests that `OnSuggestionAccepted` does not store accepted suggestions in
// `previously_filled_suggestions` when `kAutofillAtMemoryPreviouslyFilled`
// is disabled.
TEST_F(AtMemoryPersistedStateManagerTest,
       DoesNotStorePreviouslyFilledSuggestionsWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillAtMemoryPreviouslyFilled);

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  state_manager().OnSuggestionAccepted(s1);

  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that accepting a primary suggestion stores it in
// `previously_filled_suggestions`.
TEST_F(AtMemoryPersistedStateManagerTest, RetrievedSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");

  Suggestion primary_suggestion(u"Passport",
                                SuggestionType::kAtMemorySearchResult);
  state_manager().OnSuggestionsChanged({primary_suggestion});

  state_manager().OnSuggestionAccepted(primary_suggestion);

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Passport");
}

// Tests that accepting a previously filled suggestion stores it in
// `previously_filled_suggestions`.
TEST_F(AtMemoryPersistedStateManagerTest, PreviouslyFilledSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion primary_suggestion(u"Passport",
                                SuggestionType::kAtMemorySearchResult);

  // Initially accept the primary suggestion so it enters
  // `previously_filled_suggestions`.
  state_manager().OnSuggestionAccepted(primary_suggestion);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Now, in 0-state on a field without active search, accept the previously
  // filled suggestion.
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnSuggestionAccepted(primary_suggestion);

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Passport");
}

// Tests that accepting a child of a retrieved suggestion gets stored as a new
// entry in `previously_filled_suggestions`.
TEST_F(AtMemoryPersistedStateManagerTest, ChildOfRetrievedSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");

  Suggestion primary_suggestion(u"Passport",
                                SuggestionType::kAtMemorySearchResult);
  Suggestion child_suggestion(u"12345678",
                              SuggestionType::kAtMemorySearchResult);
  primary_suggestion.children = {child_suggestion};

  state_manager().OnSuggestionsChanged({primary_suggestion});

  state_manager().OnSuggestionAccepted(child_suggestion);

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Passport");
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].children.size(),
            1u);
}

// Tests that accepting a child of a previously filled suggestion gets stored as
// a new entry in `previously_filled_suggestions`.
TEST_F(AtMemoryPersistedStateManagerTest, ChildOfPreviouslyFilledSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion primary_suggestion(u"Passport",
                                SuggestionType::kAtMemorySearchResult);
  Suggestion child_suggestion(u"12345678",
                              SuggestionType::kAtMemorySearchResult);
  primary_suggestion.children = {child_suggestion};

  // Initially accept the primary suggestion so it enters
  // `previously_filled_suggestions`.
  state_manager().OnSuggestionAccepted(primary_suggestion);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Now, in 0-state on a field without active search, accept the secondary
  // suggestion.
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnSuggestionAccepted(child_suggestion);

  // Verify that `previously_filled_suggestions` contains the deduplicated
  // primary suggestion (not `child_suggestion`).
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Passport");
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].children.size(),
            1u);
}

// Tests that accepting an already existing suggestion deduplicates and moves it
// to the most recently used position.
TEST_F(AtMemoryPersistedStateManagerTest, DeduplicatesAndPreservesMruOrder) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  Suggestion s2(u"Suggestion 2", SuggestionType::kAtMemorySearchResult);

  state_manager().OnSuggestionAccepted(s1);
  state_manager().OnSuggestionAccepted(s2);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Suggestion 1");
  EXPECT_EQ(state_manager().previously_filled_suggestions()[1].main_text.value,
            u"Suggestion 2");

  // Re-accept s1 to verify it is deduplicated and moved to the back (MRU).
  state_manager().OnSuggestionAccepted(s1);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Suggestion 2");
  EXPECT_EQ(state_manager().previously_filled_suggestions()[1].main_text.value,
            u"Suggestion 1");
}

// Tests that accepting more than `kMaxPreviouslyFilledSuggestions` evicts the
// oldest suggestion.
TEST_F(AtMemoryPersistedStateManagerTest, EvictsOldestWhenLimitReached) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  for (size_t i = 0;
       i < AtMemoryPersistedStateManager::kMaxPreviouslyFilledSuggestions;
       ++i) {
    state_manager().OnSuggestionAccepted(Suggestion(
        base::NumberToString16(i), SuggestionType::kAtMemorySearchResult));
  }
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(),
            AtMemoryPersistedStateManager::kMaxPreviouslyFilledSuggestions);
  EXPECT_EQ(
      state_manager().previously_filled_suggestions().front().main_text.value,
      u"0");
  EXPECT_EQ(
      state_manager().previously_filled_suggestions().back().main_text.value,
      u"19");

  // Accept a new suggestion when the limit is reached.
  state_manager().OnSuggestionAccepted(
      Suggestion(u"20", SuggestionType::kAtMemorySearchResult));

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(),
            AtMemoryPersistedStateManager::kMaxPreviouslyFilledSuggestions);
  // Oldest suggestion "0" should be evicted, so "1" is now the oldest.
  EXPECT_EQ(
      state_manager().previously_filled_suggestions().front().main_text.value,
      u"1");
  EXPECT_EQ(
      state_manager().previously_filled_suggestions().back().main_text.value,
      u"20");
}

}  // namespace
}  // namespace autofill
