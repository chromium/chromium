// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/history/core/browser/history_types.h"
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
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AtMemoryPersistedStateManager state_manager_{/*history_service=*/nullptr};
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

  EXPECT_EQ(
      state_manager().GetStateForField(other_field_id(), OtherFieldOrigin()),
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

// Tests that accessing the field origin when no field is active crashes with a
// CHECK failure.
TEST_F(AtMemoryPersistedStateManagerTest,
       FieldOriginCrashesWhenNoFieldIsActive) {
  EXPECT_CHECK_DEATH(state_manager().field_origin());
}

// Tests that accepting a suggestion resets the persisted search state.
TEST_F(AtMemoryPersistedStateManagerTest,
       OnSuggestionAcceptedResetsSearchState) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));

  EXPECT_CHECK_DEATH(state_manager().field_origin());
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
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

// Tests that non-SPII previously filled suggestions expire and are
// auto-destructed after the default TTL.
TEST_F(AtMemoryPersistedStateManagerTest,
       NonSpiiSuggestionsExpireAfterDefaultTtl) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s(u"Address", SuggestionType::kAtMemorySearchResult);
  s.payload =
      Suggestion::AtMemoryPayload(u"123 Main St", MemoryDataType::kAddressFull);

  state_manager().OnSuggestionAccepted(s);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward to 1 minute before the default TTL.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(1));
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward by 1 more minute (default TTL expires).
  task_environment().FastForwardBy(base::Minutes(1));
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that sensitive personal information (SPII) previously filled
// suggestions expire and are auto-destructed after the SPII TTL.
TEST_F(AtMemoryPersistedStateManagerTest, SpiiSuggestionsExpireAfterSpiiTtl) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s(u"Passport", SuggestionType::kAtMemorySearchResult);
  s.payload =
      Suggestion::AtMemoryPayload(u"12345678", MemoryDataType::kPassportNumber);

  state_manager().OnSuggestionAccepted(s);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward to 1 second before the SPII TTL.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kSpiiTimeToLive - base::Seconds(1));
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward by 1 more second (SPII TTL expires).
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that suggestions with SPII child metadata expire after the SPII TTL.
TEST_F(AtMemoryPersistedStateManagerTest,
       SpiiSuggestionWithChildSpiiExpiresAfterSpiiTtl) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion primary_suggestion(u"Primary",
                                SuggestionType::kAtMemorySearchResult);
  Suggestion child_suggestion(u"12345678",
                              SuggestionType::kAtMemorySearchResult);
  child_suggestion.payload =
      Suggestion::AtMemoryPayload(u"12345678", MemoryDataType::kPassportNumber);
  primary_suggestion.children = {child_suggestion};

  state_manager().OnSuggestionAccepted(primary_suggestion);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward to 1 second before the SPII TTL.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kSpiiTimeToLive - base::Seconds(1));
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward by 1 more second (SPII TTL expires).
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that non-SPII and SPII suggestions expire according to their respective
// TTLs independently.
TEST_F(AtMemoryPersistedStateManagerTest,
       MixedSuggestionsExpireAccordingToRespectiveTtl) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion non_spii(u"Non-SPII", SuggestionType::kAtMemorySearchResult);
  non_spii.payload =
      Suggestion::AtMemoryPayload(u"123 Main St", MemoryDataType::kAddressFull);
  Suggestion spii(u"SPII", SuggestionType::kAtMemorySearchResult);
  spii.payload =
      Suggestion::AtMemoryPayload(u"12345678", MemoryDataType::kPassportNumber);

  state_manager().OnSuggestionAccepted(non_spii);
  state_manager().OnSuggestionAccepted(spii);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);

  // Fast forward to SPII TTL: SPII expires, Non-SPII remains.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kSpiiTimeToLive);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Non-SPII");

  // Fast forward remaining time to default TTL: Non-SPII expires.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive -
      AtMemoryPersistedStateManager::kSpiiTimeToLive);
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that when an SPII suggestion is accepted before a non-SPII suggestion,
// both expire according to their respective TTLs.
TEST_F(AtMemoryPersistedStateManagerTest,
       SpiiAcceptedBeforeNonSpiiExpiresCorrectly) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion spii(u"SPII", SuggestionType::kAtMemorySearchResult);
  spii.payload =
      Suggestion::AtMemoryPayload(u"12345678", MemoryDataType::kPassportNumber);
  Suggestion non_spii(u"Non-SPII", SuggestionType::kAtMemorySearchResult);
  non_spii.payload =
      Suggestion::AtMemoryPayload(u"123 Main St", MemoryDataType::kAddressFull);

  state_manager().OnSuggestionAccepted(spii);
  state_manager().OnSuggestionAccepted(non_spii);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);

  // Fast forward to SPII TTL: SPII expires, Non-SPII remains.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kSpiiTimeToLive);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions()[0].main_text.value,
            u"Non-SPII");

  // Fast forward remaining time to default TTL: Non-SPII expires.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive -
      AtMemoryPersistedStateManager::kSpiiTimeToLive);
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that re-accepting an existing suggestion refreshes its TTL.
TEST_F(AtMemoryPersistedStateManagerTest, ReAcceptingSuggestionRefreshesTtl) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s(u"Address", SuggestionType::kAtMemorySearchResult);
  s.payload =
      Suggestion::AtMemoryPayload(u"123 Main St", MemoryDataType::kAddressFull);

  state_manager().OnSuggestionAccepted(s);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  const base::TimeDelta partial_duration =
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10);

  // Fast forward by partial duration (less than default TTL).
  task_environment().FastForwardBy(partial_duration);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Re-accept the suggestion, which should refresh its TTL.
  state_manager().OnSuggestionAccepted(s);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward by another partial duration.
  task_environment().FastForwardBy(partial_duration);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Fast forward remaining time to reach full TTL since re-accept.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - partial_duration);
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that history deletion clears persisted suggestions and resets the
// state.
TEST_F(AtMemoryPersistedStateManagerTest,
       HistoryDeletionClearsPersistedSuggestions) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().StopSearching();
  state_manager().OnSuggestionsChanged(suggestions);

  ASSERT_TRUE(state_manager().GetStateForField(field_id(), FieldOrigin()));

  state_manager().OnHistoryDeletions(
      /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_CHECK_DEATH(state_manager().field_origin());
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
}

// Tests that history deletion clears ongoing in-flight search state.
TEST_F(AtMemoryPersistedStateManagerTest,
       HistoryDeletionClearsOngoingSearchState) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"ongoing_query");
  EXPECT_TRUE(state_manager().IsSearching());

  state_manager().OnHistoryDeletions(
      /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_FALSE(state_manager().IsSearching());
  EXPECT_CHECK_DEATH(state_manager().field_origin());
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
}

// Tests that history deletion clears previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       HistoryDeletionClearsPreviouslyFilledSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  state_manager().OnSuggestionAccepted(s1);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  state_manager().OnHistoryDeletions(
      /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that persisted search state expires and is reset after the inactivity
// TTL (30 minutes).
TEST_F(AtMemoryPersistedStateManagerTest, StateExpiresAfterTtl) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().StopSearching();
  state_manager().OnSuggestionsChanged(suggestions);

  // Before TTL expires, state is intact.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Seconds(1));
  const std::optional<AtMemorySearchState>& active_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  ASSERT_TRUE(active_state.has_value());
  EXPECT_EQ(active_state->filter, u"address");
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());

  // Once TTL expires, state is reset.
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
}

// Tests that state mutations (filter changes, receiving suggestions, submitting
// queries) restart the inactivity TTL timer.
TEST_F(AtMemoryPersistedStateManagerTest, MutationsResetTtl) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  // Advance by partial duration (less than default TTL).
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10));

  // 1. OnFilterChanged: modify query, resetting TTL timer.
  state_manager().OnFilterChanged(u"new address");

  // Advance to the total default TTL.
  task_environment().FastForwardBy(base::Minutes(10));
  EXPECT_TRUE(
      state_manager().GetStateForField(field_id(), FieldOrigin()).has_value());

  // 2. OnSuggestionsChanged: receive suggestions, resetting TTL timer.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionsChanged(suggestions);

  // Advance another partial duration.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10));
  EXPECT_TRUE(
      state_manager().GetStateForField(field_id(), FieldOrigin()).has_value());

  // 3. OnFilterSubmitted: submit new query, resetting TTL timer.
  state_manager().OnFilterSubmitted(u"submitted address");

  // Advance another partial duration.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10));
  const std::optional<AtMemorySearchState>& active_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  ASSERT_TRUE(active_state.has_value());
  EXPECT_EQ(active_state->filter, u"submitted address");
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());

  // Advance remaining duration to cross the default TTL deadline.
  task_environment().FastForwardBy(base::Minutes(10));
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()),
            std::nullopt);
}

}  // namespace
}  // namespace autofill
