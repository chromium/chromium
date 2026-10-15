// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_persisted_state_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager_test_api.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"
#include "components/autofill/core/browser/at_memory/at_memory_search_state.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/history/core/browser/history_types.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// Matches an `AtMemorySearchState` in the 0-state.
auto IsZeroState() {
  return AllOf(
      Field(&AtMemorySearchState::status, AtMemorySearchStatus::kZeroState),
      Field(&AtMemorySearchState::filter, IsEmpty()),
      Field(&AtMemorySearchState::suggestions, IsEmpty()));
}

url::Origin FieldOrigin() {
  return url::Origin::Create(GURL("https://example.com"));
}

url::Origin OtherFieldOrigin() {
  return url::Origin::Create(GURL("https://other.com"));
}

Suggestion CreateAddressSuggestion(const AutofillProfile& profile) {
  Suggestion suggestion(u"123 Main St", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"123 Main St",
                                      MemoryDataType::kAddressFull);
  payload.identifier = profile.guid();
  suggestion.payload = payload;
  return suggestion;
}

Suggestion CreateCreditCardSuggestion(const CreditCard& card) {
  Suggestion suggestion(u"4111", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"4111",
                                      MemoryDataType::kCreditCardNumber);
  payload.identifier = card.guid();
  suggestion.payload = payload;
  return suggestion;
}

Suggestion CreateEntitySuggestion(const EntityInstance& entity) {
  Suggestion suggestion(u"Passport", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"Passport",
                                      MemoryDataType::kPassportNumber);
  payload.identifier = entity.guid();
  suggestion.payload = payload;
  return suggestion;
}

Suggestion CreateLoyaltyCardSuggestion(const LoyaltyCard& card) {
  Suggestion suggestion(u"Loyalty Card", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"Loyalty Card",
                                      MemoryDataType::kLoyaltyMembershipId);
  payload.identifier = card.id();
  suggestion.payload = payload;
  return suggestion;
}

Suggestion CreateIbanSuggestion(const Iban& iban) {
  Suggestion suggestion(u"DE12345678901234567890",
                        SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"DE12345678901234567890",
                                      MemoryDataType::kIban);
  payload.identifier = Iban::Guid(iban.guid());
  suggestion.payload = payload;
  return suggestion;
}

class FakePersonalContextEligibilityService
    : public personal_context::PersonalContextEligibilityService {
 public:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool IsInitialized() const override { return true; }

  personal_context::PersonalContextEligibilityState GetEligibilityState()
      override {
    return state_;
  }

  bool IsEligibleForEncryption() const override {
    return state_ ==
           personal_context::PersonalContextEligibilityState::kEligible;
  }

  std::optional<personal_context::PersonalContextNonEligibilityReason>
  GetNonEligibilityReason() const override {
    return std::nullopt;
  }

  void SetEligibilityState(
      personal_context::PersonalContextEligibilityState state) {
    state_ = state;
    for (Observer& observer : observers_) {
      observer.OnEligibilityStateChanged(state);
    }
  }

  void DisableContextSource() {
    for (Observer& observer : observers_) {
      observer.OnContextSourceDisabled();
    }
  }

 private:
  personal_context::PersonalContextEligibilityState state_ =
      personal_context::PersonalContextEligibilityState::kEligible;
  base::ObserverList<Observer> observers_;
};

class AtMemoryPersistedStateManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  AtMemoryPersistedStateManagerTest() {
    feature_list_.InitAndEnableFeature(features::kAutofillAtMemory);
    task_environment_.AdvanceClock(test::kJune2017 + base::Days(100) -
                                   base::Time::Now());
  }

  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_personal_context_eligibility_service(
        &eligibility_service_);
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*pcontext_manager=*/nullptr,
            /*strike_database=*/nullptr,
            /*variation_country_code=*/GeoIpCountryCode("US")));
    webdata_helper_.WaitUntilIdle();
    // The prefs are already registered by `test::PrefServiceForTesting()`.
    pref_service().SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        true);
    // `autofill_driver(0)` plays the role of the primary main frame.
    CreateAutofillDriver();
    // Instantiate the tab's `AtMemoryManager` (and with it the
    // `AtMemoryPersistedStateManager` under test).
    ASSERT_TRUE(autofill_client().GetAtMemoryManager());
  }

  void TearDown() override { DestroyAutofillClient(); }

  // The `AtMemoryPersistedStateManager` owned by the tab's `AtMemoryManager`.
  AtMemoryPersistedStateManager& state_manager() {
    return CHECK_DEREF(
        test_api(CHECK_DEREF(autofill_client().GetAtMemoryManager()))
            .state_manager());
  }
  const FieldGlobalId& field_id() const { return field_id_; }
  const FieldGlobalId& other_field_id() const { return other_field_id_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }
  test::AutofillTestingPrefService& pref_service() {
    return CHECK_DEREF(autofill_client().GetPrefs());
  }
  FakePersonalContextEligibilityService& eligibility_service() {
    return eligibility_service_;
  }
  signin::IdentityTestEnvironment& identity_test_env() {
    return autofill_client().identity_test_environment();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  FakePersonalContextEligibilityService eligibility_service_;
  FieldGlobalId field_id_{test::MakeFieldGlobalId()};
  FieldGlobalId other_field_id_{test::MakeFieldGlobalId()};
};

// Tests that accessing a new field returns the 0-state.
TEST_F(AtMemoryPersistedStateManagerTest, ReturnsZeroStateForNewField) {
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that opening and closing without editing returns the 0-state.
TEST_F(AtMemoryPersistedStateManagerTest,
       OpenCloseWithoutEditReturnsZeroState) {
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  // Opening again on the same field without typing or searching returns the
  // 0-state.
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());
}

// Tests that suggestions shown in the 0-state (e.g. empty-query suggestions)
// are not persisted and the state stays in the 0-state.
TEST_F(AtMemoryPersistedStateManagerTest,
       DoesNotPersistSuggestionsInZeroState) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
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
  state_manager().StopSearching(AtMemoryQueryCompleted(false));
  state_manager().OnSuggestionsChanged(suggestions);

  EXPECT_FALSE(state_manager().IsSearching());

  const AtMemorySearchState& restored_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(restored_state.filter, u"address");
  EXPECT_THAT(restored_state.suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntry,
                                           u"123 Main St")));
  EXPECT_EQ(restored_state.status, AtMemorySearchStatus::kQueryModified);
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

  EXPECT_THAT(
      state_manager().GetStateForField(other_field_id(), OtherFieldOrigin()),
      IsZeroState());
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

  const AtMemorySearchState& state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(state.filter, u"add");
  EXPECT_TRUE(state.suggestions.empty());
  EXPECT_EQ(state.status, AtMemorySearchStatus::kQueryModified);

  // Clearing the filter resets the state to the 0-state.
  state_manager().OnFilterChanged(u"");
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
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
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
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

  state_manager().StopSearching(AtMemoryQueryCompleted(false));
  EXPECT_FALSE(state_manager().IsSearching());

  const AtMemorySearchState& restored_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(restored_state.filter, u"ongoing_query");
  EXPECT_TRUE(restored_state.suggestions.empty());
  EXPECT_EQ(restored_state.status, AtMemorySearchStatus::kQueryModified);
}

// Tests that `StopSearching()` with `AtMemoryQueryCompleted(true)` sets the
// status to `kResponseReceived`, and that changing or resubmitting the filter
// updates the status accordingly.
TEST_F(AtMemoryPersistedStateManagerTest,
       StopSearchingWithQueryCompletedSetsResponseReceived) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kSearching);

  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  EXPECT_FALSE(state_manager().IsSearching());
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kResponseReceived);

  state_manager().OnFilterSubmitted(u"address2");
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kSearching);

  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  state_manager().OnFilterChanged(u"address3");
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kQueryModified);
}

// Tests that submitting a filter clears the suggestions of a previous search,
// so that they are not mistaken for (partial) results of the new search.
TEST_F(AtMemoryPersistedStateManagerTest,
       OnFilterSubmittedClearsPreviousSuggestions) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  state_manager().OnSuggestionsChanged(
      {Suggestion(u"123 Main St", SuggestionType::kAtMemorySearchResult)});

  state_manager().OnFilterSubmitted(u"address");

  const AtMemorySearchState& state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(state.status, AtMemorySearchStatus::kSearching);
  EXPECT_EQ(state.filter, u"address");
  EXPECT_TRUE(state.suggestions.empty());
}

// Tests that cancelling a search via `StopSearching()` sets the status back to
// `kQueryModified`.
TEST_F(AtMemoryPersistedStateManagerTest,
       StopSearchingWithoutQueryCompletedSetsQueryModified) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().StopSearching(AtMemoryQueryCompleted(false));
  EXPECT_FALSE(state_manager().IsSearching());
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kQueryModified);
}

// Tests that `StopSearching()` is a no-op when no search is in progress.
TEST_F(AtMemoryPersistedStateManagerTest, StopSearchingWithoutSearchIsNoOp) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());

  state_manager().OnFilterChanged(u"address");
  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  EXPECT_EQ(state_manager().GetStateForField(field_id(), FieldOrigin()).status,
            AtMemorySearchStatus::kQueryModified);
}

// Tests that receiving suggestions while searching sets the status to
// `kPartialResponseReceived`, which still counts as searching.
TEST_F(AtMemoryPersistedStateManagerTest, PartialResultsSetStatus) {
  const std::vector<Suggestion> results = {
      Suggestion(u"123 Main St", SuggestionType::kAtMemorySearchResult)};
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  EXPECT_EQ(state_manager().status(), AtMemorySearchStatus::kSearching);

  state_manager().OnSuggestionsChanged(results);
  EXPECT_EQ(state_manager().status(),
            AtMemorySearchStatus::kPartialResponseReceived);
  EXPECT_TRUE(state_manager().IsSearching());

  // The final response was received.
  state_manager().StopSearching(AtMemoryQueryCompleted(true));
  state_manager().OnSuggestionsChanged(results);
  EXPECT_EQ(state_manager().status(), AtMemorySearchStatus::kResponseReceived);
  EXPECT_FALSE(state_manager().IsSearching());
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

  EXPECT_THAT(
      state_manager().previously_filled_suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 1"),
                  EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 2")));
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

  EXPECT_THAT(state_manager().previously_filled_suggestions(),
              ElementsAre(EqualsSuggestion(
                  SuggestionType::kAtMemorySearchResult, u"Passport")));
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

  EXPECT_THAT(state_manager().previously_filled_suggestions(),
              ElementsAre(EqualsSuggestion(
                  SuggestionType::kAtMemorySearchResult, u"Passport")));
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

  EXPECT_THAT(
      state_manager().previously_filled_suggestions(),
      ElementsAre(AllOf(
          EqualsSuggestion(SuggestionType::kAtMemorySearchResult, u"Passport"),
          Field(&Suggestion::children, SizeIs(1u)))));
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
  EXPECT_THAT(
      state_manager().previously_filled_suggestions(),
      ElementsAre(AllOf(
          EqualsSuggestion(SuggestionType::kAtMemorySearchResult, u"Passport"),
          Field(&Suggestion::children, SizeIs(1u)))));
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
  EXPECT_THAT(
      state_manager().previously_filled_suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 1"),
                  EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 2")));

  // Re-accept s1 to verify it is deduplicated and moved to the back (MRU).
  state_manager().OnSuggestionAccepted(s1);
  EXPECT_THAT(
      state_manager().previously_filled_suggestions(),
      ElementsAre(EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 2"),
                  EqualsSuggestion(SuggestionType::kAtMemorySearchResult,
                                   u"Suggestion 1")));
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
  state_manager().StopSearching(AtMemoryQueryCompleted(false));
  state_manager().OnSuggestionsChanged(suggestions);

  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));

  static_cast<history::HistoryServiceObserver&>(state_manager())
      .OnHistoryDeletions(
          /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_CHECK_DEATH(state_manager().field_origin());
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
}

// Tests that history deletion clears ongoing in-flight search state.
TEST_F(AtMemoryPersistedStateManagerTest,
       HistoryDeletionClearsOngoingSearchState) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"ongoing_query");
  EXPECT_TRUE(state_manager().IsSearching());

  static_cast<history::HistoryServiceObserver&>(state_manager())
      .OnHistoryDeletions(
          /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_FALSE(state_manager().IsSearching());
  EXPECT_CHECK_DEATH(state_manager().field_origin());
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
}

// Tests that history deletion clears previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       HistoryDeletionClearsPreviouslyFilledSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  state_manager().OnSuggestionAccepted(s1);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  static_cast<history::HistoryServiceObserver&>(state_manager())
      .OnHistoryDeletions(
          /*history_service=*/nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that automatic history expiration does not clear active search state
// or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       ScopedHistoryDeletionsDoNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion s1(u"Suggestion 1", SuggestionType::kAtMemorySearchResult);
  state_manager().OnSuggestionAccepted(s1);
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"ongoing_query");
  ASSERT_TRUE(state_manager().IsSearching());
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  const base::Time begin = base::Time::Now() - base::Hours(25);
  const base::Time end = base::Time::Now() - base::Hours(24);
  static_cast<history::HistoryServiceObserver&>(state_manager())
      .OnHistoryDeletions(
          /*history_service=*/nullptr,
          history::DeletionInfo(history::DeletionTimeRange(begin, end),
                                /*is_from_expiration=*/true,
                                /*deleted_rows=*/{},
                                /*favicon_urls=*/{},
                                /*restrict_urls=*/std::nullopt));

  EXPECT_TRUE(state_manager().IsSearching());
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Tests that signing out clears persisted search state and previously filled
// suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       SignOutClearsPersistedStateAndPreviouslyFilledSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  identity_test_env().MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);
  state_manager().OnSuggestionAccepted(
      Suggestion(u"Suggestion 1", SuggestionType::kAtMemorySearchResult));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  identity_test_env().ClearPrimaryAccount();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Tests that switching from one primary account to another clears persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       PrimaryAccountSwitchClearsPersistedStateAndPreviouslyFilledSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  identity_test_env().MakePrimaryAccountAvailable(
      "user1@example.com", signin::ConsentLevel::kSignin);
  state_manager().OnSuggestionAccepted(
      Suggestion(u"Suggestion 1", SuggestionType::kAtMemorySearchResult));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  identity_test_env().MakePrimaryAccountAvailable(
      "user2@example.com", signin::ConsentLevel::kSignin);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that persisted search state expires and is reset after the inactivity
// TTL (30 minutes).
TEST_F(AtMemoryPersistedStateManagerTest, StateExpiresAfterTtl) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().StopSearching(AtMemoryQueryCompleted(false));
  state_manager().OnSuggestionsChanged(suggestions);

  // Before TTL expires, state is intact.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Seconds(1));
  const AtMemorySearchState& active_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(active_state.filter, u"address");
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());

  // Once TTL expires, state is reset.
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
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
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));

  // 2. OnSuggestionsChanged: receive suggestions, resetting TTL timer.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"123 Main St", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionsChanged(suggestions);

  // Advance another partial duration.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10));
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));

  // 3. OnFilterSubmitted: submit new query, resetting TTL timer.
  state_manager().OnFilterSubmitted(u"submitted address");

  // Advance another partial duration.
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive - base::Minutes(10));
  const AtMemorySearchState& active_state =
      state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(active_state.filter, u"submitted address");
  EXPECT_EQ(state_manager().field_origin(), FieldOrigin());

  // Advance remaining duration to cross the default TTL deadline.
  task_environment().FastForwardBy(base::Minutes(10));
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
}

// Tests that disabling the Personal Context settings toggle clears both the
// active search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, SettingsToggleDisabledClearsState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  pref_service().SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that when `PersonalContextEligibilityService` reports that the user is
// no longer eligible, active search state and previously filled suggestions are
// cleared.
TEST_F(AtMemoryPersistedStateManagerTest,
       EligibilityStateChangedToDisabledClearsState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  eligibility_service().SetEligibilityState(
      personal_context::PersonalContextEligibilityState::kDisabledNotEligible);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that enabling settings or receiving an eligible state does not clear
// the active search state or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       EnablingPrefOrRemainingEligiblePreservesState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");

  pref_service().SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  eligibility_service().SetEligibilityState(
      personal_context::PersonalContextEligibilityState::kEligible);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that disabling a Personal Context source clears the search state and
// previously filled suggestions, even if the user remains eligible.
TEST_F(AtMemoryPersistedStateManagerTest, ContextSourceDisabledClearsState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  eligibility_service().DisableContextSource();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}
// Tests that adding an address profile does not clear persisted search state or
// previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, AddProfileDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile existing_profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      existing_profile);

  state_manager().OnSuggestionAccepted(
      CreateAddressSuggestion(existing_profile));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged(
      {CreateAddressSuggestion(existing_profile)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  AutofillProfile new_profile = test::GetFullProfile2();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      new_profile);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that recording usage of an address profile does not clear persisted
// search state or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, RecordUseOfProfileDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client().GetPersonalDataManager().address_data_manager().RecordUseOf(
      profile);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that removing an address profile prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       AddressProfileRemovedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile.guid());

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that pruning all partial results of an ongoing search sets the status
// back to `kSearching`.
TEST_F(AtMemoryPersistedStateManagerTest,
       PruningAllPartialResultsSetsStatusToSearching) {
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile)});
  ASSERT_EQ(state_manager().status(),
            AtMemorySearchStatus::kPartialResponseReceived);

  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile.guid());

  EXPECT_EQ(state_manager().status(), AtMemorySearchStatus::kSearching);
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
}

// Tests that modifying an address profile prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       AddressProfileModifiedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  task_environment().FastForwardBy(base::Seconds(1));
  profile.SetRawInfo(NAME_FULL, u"Modified Name");
  profile.usage_history().set_modification_date(base::Time::Now());
  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .UpdateProfile(profile);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that suggestions without a known GUID/identifier (e.g. CMS contact info
// suggestions with std::monostate) are not pruned when address data changes.
TEST_F(AtMemoryPersistedStateManagerTest,
       SuggestionWithoutGuidNotPrunedOnAddressDataChange) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  Suggestion suggestion(u"123 Main St", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"123 Main St",
                                      MemoryDataType::kAddressFull);
  // Identifier is default-initialized to std::monostate.
  suggestion.payload = payload;

  state_manager().OnSuggestionAccepted(suggestion);
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({suggestion});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile.guid());

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
}

// Tests that non-Autofill sourced suggestions (e.g. from Gmail) are not pruned
// when local Autofill address data changes.
TEST_F(AtMemoryPersistedStateManagerTest,
       NonAutofillSourcedSuggestionNotPrunedOnAddressDataChange) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  Suggestion suggestion(u"123 Main St", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload(u"123 Main St",
                                      MemoryDataType::kAddressFull);
  payload.identifier = profile.guid();
  payload.is_personal_context_sourced = true;
  suggestion.payload = payload;

  state_manager().OnSuggestionAccepted(suggestion);
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({suggestion});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile.guid());

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            1u);
  EXPECT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
}

// Tests that adding a credit card does not clear persisted search state or
// previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, AddCreditCardDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  CreditCard card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"card");
  state_manager().OnSuggestionsChanged({CreateCreditCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  CreditCard card2 = test::GetCreditCard2();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card2);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that recording usage of a credit card does not clear persisted search
// state or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, RecordUseOfCardDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  CreditCard card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"card");
  state_manager().OnSuggestionsChanged({CreateCreditCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .RecordUseOfCard(card);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that removing a credit card prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, CreditCardRemovedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  CreditCard card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"card");
  state_manager().OnSuggestionsChanged({CreateCreditCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .RemoveByGUID(card.guid());

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that modifying a credit card prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, CreditCardModifiedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  CreditCard card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"card");
  state_manager().OnSuggestionsChanged({CreateCreditCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  task_environment().FastForwardBy(base::Seconds(1));
  card.usage_history().set_modification_date(base::Time::Now());
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .UpdateCreditCard(card);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that adding an entity instance does not clear persisted search state or
// previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, AddEntityInstanceDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  EntityInstance entity = test::GetPassportEntityInstance();
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
  webdata_helper().WaitUntilIdle();

  state_manager().OnSuggestionAccepted(CreateEntitySuggestion(entity));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  state_manager().OnSuggestionsChanged({CreateEntitySuggestion(entity)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  EntityInstance entity2 = test::GetDriversLicenseEntityInstance();
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity2);
  webdata_helper().WaitUntilIdle();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that recording usage of an entity does not clear persisted search state
// or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, RecordUseOfEntityDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  EntityInstance entity = test::GetPassportEntityInstance();
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
  webdata_helper().WaitUntilIdle();

  state_manager().OnSuggestionAccepted(CreateEntitySuggestion(entity));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  state_manager().OnSuggestionsChanged({CreateEntitySuggestion(entity)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client().GetEntityDataManager()->RecordEntityUsed(entity.guid(),
                                                             base::Time::Now());
  webdata_helper().WaitUntilIdle();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that removing an entity instance prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       EntityInstanceRemovedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  EntityInstance entity = test::GetPassportEntityInstance();
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
  webdata_helper().WaitUntilIdle();

  state_manager().OnSuggestionAccepted(CreateEntitySuggestion(entity));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  state_manager().OnSuggestionsChanged({CreateEntitySuggestion(entity)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client().GetEntityDataManager()->RemoveEntityInstance(entity.guid());
  webdata_helper().WaitUntilIdle();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that modifying an entity instance prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       EntityInstanceModifiedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  EntityInstance entity = test::GetPassportEntityInstance();
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
  webdata_helper().WaitUntilIdle();

  state_manager().OnSuggestionAccepted(CreateEntitySuggestion(entity));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  state_manager().OnSuggestionsChanged({CreateEntitySuggestion(entity)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  task_environment().FastForwardBy(base::Seconds(1));
  EntityInstance updated_entity = test::GetPassportEntityInstance(
      {.guid = entity.guid().value(), .date_modified = base::Time::Now()});
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(
      updated_entity);
  webdata_helper().WaitUntilIdle();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that adding a loyalty card does not clear persisted search state or
// previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, AddLoyaltyCardDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  LoyaltyCard card = test::CreateLoyaltyCard();
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .AddLoyaltyCard(card);
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  state_manager().OnSuggestionAccepted(CreateLoyaltyCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"loyalty");
  state_manager().OnSuggestionsChanged({CreateLoyaltyCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  LoyaltyCard card2 = test::CreateLoyaltyCard();
  card2.set_id(ValuableId("loyalty_card_id_2"));
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .AddLoyaltyCard(card2);
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that recording usage of a loyalty card does not clear persisted search
// state or previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest,
       RecordUseOfLoyaltyCardDoesNotClearState) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  LoyaltyCard card = test::CreateLoyaltyCard();
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .AddLoyaltyCard(card);
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  state_manager().OnSuggestionAccepted(CreateLoyaltyCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"loyalty");
  state_manager().OnSuggestionsChanged({CreateLoyaltyCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client().GetValuablesDataManager()->RecordLoyaltyCardUsed(
      card.id(), base::Time::Now());
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_FALSE(state_manager().previously_filled_suggestions().empty());
}

// Tests that removing a loyalty card prunes suggestions from persisted
// search state and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, LoyaltyCardRemovedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  LoyaltyCard card = test::CreateLoyaltyCard();
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .AddLoyaltyCard(card);
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  state_manager().OnSuggestionAccepted(CreateLoyaltyCardSuggestion(card));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"loyalty");
  state_manager().OnSuggestionsChanged({CreateLoyaltyCardSuggestion(card)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .ClearLoyaltyCards();
  test_api(CHECK_DEREF(autofill_client().GetValuablesDataManager()))
      .NotifyObservers();

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that removing one item prunes only that item's suggestion while
// retaining suggestions for items that still exist, including suggestions of a
// different data type.
TEST_F(AtMemoryPersistedStateManagerTest,
       RemovingOneItemPrunesOnlyAffectedSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  CreditCard card = test::GetCreditCard();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile1);
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile2);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile1));
  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile2));
  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile1),
                                        CreateAddressSuggestion(profile2),
                                        CreateCreditCardSuggestion(card)});

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 3u);
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            3u);

  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile1.guid());

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            2u);
  EXPECT_EQ(
      std::get<std::string>(state_manager()
                                .GetStateForField(field_id(), FieldOrigin())
                                .suggestions[0]
                                .GetPayload<Suggestion::AtMemoryPayload>()
                                .identifier),
      profile2.guid());
  EXPECT_EQ(
      std::get<std::string>(state_manager()
                                .GetStateForField(field_id(), FieldOrigin())
                                .suggestions[1]
                                .GetPayload<Suggestion::AtMemoryPayload>()
                                .identifier),
      card.guid());
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);
  EXPECT_EQ(std::get<std::string>(state_manager()
                                      .previously_filled_suggestions()[0]
                                      .GetPayload<Suggestion::AtMemoryPayload>()
                                      .identifier),
            profile2.guid());
  EXPECT_EQ(std::get<std::string>(state_manager()
                                      .previously_filled_suggestions()[1]
                                      .GetPayload<Suggestion::AtMemoryPayload>()
                                      .identifier),
            card.guid());
}

// Tests that modifying one item prunes only that item's suggestion while
// retaining suggestions for items that were not modified, including suggestions
// of a different data type.
TEST_F(AtMemoryPersistedStateManagerTest,
       ModifyingOneItemPrunesOnlyAffectedSuggestion) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  AutofillProfile profile1 = test::GetFullProfile();
  AutofillProfile profile2 = test::GetFullProfile2();
  CreditCard card = test::GetCreditCard();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile1);
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile2);
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile1));
  state_manager().OnSuggestionAccepted(CreateAddressSuggestion(profile2));
  state_manager().OnSuggestionAccepted(CreateCreditCardSuggestion(card));

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"address");
  state_manager().OnSuggestionsChanged({CreateAddressSuggestion(profile1),
                                        CreateAddressSuggestion(profile2),
                                        CreateCreditCardSuggestion(card)});

  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 3u);
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            3u);

  task_environment().FastForwardBy(base::Seconds(1));
  profile1.SetRawInfo(NAME_FULL, u"Modified Name");
  profile1.usage_history().set_modification_date(base::Time::Now());
  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .UpdateProfile(profile1);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_EQ(state_manager()
                .GetStateForField(field_id(), FieldOrigin())
                .suggestions.size(),
            2u);
  EXPECT_EQ(
      std::get<std::string>(state_manager()
                                .GetStateForField(field_id(), FieldOrigin())
                                .suggestions[0]
                                .GetPayload<Suggestion::AtMemoryPayload>()
                                .identifier),
      profile2.guid());
  EXPECT_EQ(
      std::get<std::string>(state_manager()
                                .GetStateForField(field_id(), FieldOrigin())
                                .suggestions[1]
                                .GetPayload<Suggestion::AtMemoryPayload>()
                                .identifier),
      card.guid());
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 2u);
  EXPECT_EQ(std::get<std::string>(state_manager()
                                      .previously_filled_suggestions()[0]
                                      .GetPayload<Suggestion::AtMemoryPayload>()
                                      .identifier),
            profile2.guid());
  EXPECT_EQ(std::get<std::string>(state_manager()
                                      .previously_filled_suggestions()[1]
                                      .GetPayload<Suggestion::AtMemoryPayload>()
                                      .identifier),
            card.guid());
}

// Tests that removing an IBAN prunes suggestions from persisted search state
// and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, IbanRemovedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  Iban iban;
  iban.set_value(u"DE12345678901234567890");
  const std::string guid = autofill_client()
                               .GetPersonalDataManager()
                               .payments_data_manager()
                               .AddAsLocalIban(iban);
  iban.set_identifier(Iban::Guid(guid));

  state_manager().OnSuggestionAccepted(CreateIbanSuggestion(iban));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"iban");
  state_manager().OnSuggestionsChanged({CreateIbanSuggestion(iban)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .RemoveByGUID(guid);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that modifying an IBAN prunes suggestions from persisted search state
// and previously filled suggestions.
TEST_F(AtMemoryPersistedStateManagerTest, IbanModifiedPrunesSuggestions) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  Iban iban;
  iban.set_value(u"DE12345678901234567890");
  const std::string guid = autofill_client()
                               .GetPersonalDataManager()
                               .payments_data_manager()
                               .AddAsLocalIban(iban);
  iban.set_identifier(Iban::Guid(guid));

  state_manager().OnSuggestionAccepted(CreateIbanSuggestion(iban));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"iban");
  state_manager().OnSuggestionsChanged({CreateIbanSuggestion(iban)});
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  ASSERT_FALSE(state_manager().previously_filled_suggestions().empty());

  task_environment().FastForwardBy(base::Seconds(1));
  iban.usage_history().set_modification_date(base::Time::Now());
  autofill_client().GetPersonalDataManager().payments_data_manager().UpdateIban(
      iban);

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Not(IsZeroState()));
  EXPECT_TRUE(state_manager()
                  .GetStateForField(field_id(), FieldOrigin())
                  .suggestions.empty());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that the `on_reset_callback` passed to the constructor is called when
// the state is reset (e.g. by history deletion, settings change, account
// change, context source being disabled, or search state TTL expiration).
TEST_F(AtMemoryPersistedStateManagerTest, OnResetCallbackInvokedOnReset) {
  base::MockRepeatingClosure reset_callback;
  AtMemoryPersistedStateManager custom_state_manager(
      &autofill_client(), /*history_service=*/nullptr, reset_callback.Get());

  EXPECT_CALL(reset_callback, Run).Times(4);
  pref_service().SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);
  identity_test_env().MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);
  eligibility_service().DisableContextSource();

  // Expiring without suggestions does not invoke `on_reset_callback`.
  custom_state_manager.GetStateForField(field_id(), FieldOrigin());
  custom_state_manager.OnFilterSubmitted(u"address");
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive);

  // Expiring with non-empty suggestions invokes `on_reset_callback`.
  custom_state_manager.GetStateForField(field_id(), FieldOrigin());
  custom_state_manager.OnFilterSubmitted(u"address");
  custom_state_manager.OnSuggestionsChanged(
      {Suggestion(u"123 Main St", SuggestionType::kAddressEntry)});
  task_environment().FastForwardBy(
      AtMemoryPersistedStateManager::kDefaultTimeToLive);
}

// Tests that the `on_reset_callback` passed to the constructor is not called
// when an underlying data item is removed.
TEST_F(AtMemoryPersistedStateManagerTest,
       UnderlyingItemRemovedDoesNotInvokeOnResetCallback) {
  base::MockRepeatingClosure reset_callback;
  AtMemoryPersistedStateManager custom_state_manager(
      &autofill_client(), /*history_service=*/nullptr, reset_callback.Get());

  AutofillProfile profile = test::GetFullProfile();
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  custom_state_manager.GetStateForField(field_id(), FieldOrigin());
  custom_state_manager.OnFilterSubmitted(u"address");
  custom_state_manager.OnSuggestionsChanged({CreateAddressSuggestion(profile)});

  EXPECT_CALL(reset_callback, Run).Times(0);
  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveProfile(profile.guid());
  testing::Mock::VerifyAndClearExpectations(&reset_callback);
}

// Tests around resetting the state when the tab navigates. They reuse the
// fixture's `autofill_driver(0)`, which plays the role of the primary main
// frame.
using AtMemoryPersistedStateManagerNavigationTest =
    AtMemoryPersistedStateManagerTest;

// Tests that when the primary main frame driver transitions to kPendingReset
// (e.g. non-same-document navigation commits), the persisted search state is
// reset.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       ResetOnPrimaryMainFramePendingReset) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ResetAutofillDriver(autofill_driver(0));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
}

// Tests that when the primary main frame driver transitions to kPendingDeletion
// (e.g. frame swap or teardown), the persisted search state is reset.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       ResetOnPrimaryMainFramePendingDeletion) {
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  DeleteAutofillDriver(autofill_driver(0));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
}

// Tests that a subframe's navigation (kPendingReset) does NOT reset the
// persisted search state.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnSubframePendingReset) {
  CreateAutofillDriver();
  autofill_driver(1).SetParent(&autofill_driver(0));

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ResetAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that a subframe's deletion (kPendingDeletion) does NOT reset the
// persisted search state.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnSubframePendingDeletion) {
  CreateAutofillDriver();
  autofill_driver(1).SetParent(&autofill_driver(0));

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  DeleteAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that a primary main frame becoming inactive (e.g. because it enters the
// BFCache) does not by itself reset the persisted search state. The reset is
// triggered by the main frame that becomes active instead.
TEST_F(AtMemoryPersistedStateManagerNavigationTest, NoResetOnDeactivation) {
  ActivateAutofillDriver(autofill_driver(0));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  autofill_driver(0).SetIsActive(false);
  DeactivateAutofillDriver(autofill_driver(0));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that activating a prerendered page resets all state of the previously
// shown page. Prerender activations emit no `kPendingReset`; the drivers only
// transition between `kInactive` and `kActive` (see
// `ContentAutofillDriverFactoryTestLifecycleState.NavigatePrerendering`).
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       AllStateResetOnPrerenderActivation) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  ActivateAutofillDriver(autofill_driver(0));

  // Prerender a page. The prerendered main frame's `kPendingReset` must not
  // reset the state of the page that is shown.
  CreateAutofillDriver();
  autofill_driver(1).SetIsActive(false);
  ResetAutofillDriver(autofill_driver(1));

  state_manager().OnSuggestionAccepted(
      Suggestion(u"Passport #12345", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  // Activate the prerendered page.
  autofill_driver(1).SetIsActive(true);
  ActivateAutofillDriver(autofill_driver(1));
  autofill_driver(0).SetIsActive(false);
  DeactivateAutofillDriver(autofill_driver(0));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that restoring a page from the BFCache resets all state of the
// previously shown page. BFCache restores emit no `kPendingReset`; the drivers
// only transition between `kInactive` and `kActive` (see
// `ContentAutofillDriverFactoryTestLifecycleState.NavigateBFCached`).
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       AllStateResetOnBFCacheRestore) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};
  ActivateAutofillDriver(autofill_driver(0));

  // Navigate cross-document to a new page; the old page enters the BFCache.
  CreateAutofillDriver();
  ActivateAutofillDriver(autofill_driver(1));
  autofill_driver(0).SetIsActive(false);
  DeactivateAutofillDriver(autofill_driver(0));
  ResetAutofillDriver(autofill_driver(1));

  state_manager().OnSuggestionAccepted(
      Suggestion(u"Passport #12345", SuggestionType::kAddressEntry));
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  // Navigate back; the old page is restored from the BFCache.
  autofill_driver(0).SetIsActive(true);
  ActivateAutofillDriver(autofill_driver(0));
  autofill_driver(1).SetIsActive(false);
  DeactivateAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that a subframe becoming active (e.g. as part of a prerender
// activation, where the main frame resets the state) does not reset the
// persisted search state.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnSubframeActivation) {
  CreateAutofillDriver();
  autofill_driver(1).SetParent(&autofill_driver(0));

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ActivateAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that the main frame of an embedded frame tree (i.e. a <fencedframe> or
// a GuestView) becoming active does not reset the persisted search state.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnEmbeddedMainFrameActivation) {
  CreateAutofillDriver();
  autofill_driver(1).SetIsEmbedded(true);

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ActivateAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that a navigation in a main frame that is not *primary* - i.e. the main
// frame of a prerendered or BFCached page - does not reset the state of the
// page the user is currently looking at.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnNonPrimaryMainFrame) {
  CreateAutofillDriver();
  autofill_driver(1).SetIsActive(false);

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ResetAutofillDriver(autofill_driver(1));
  DeleteAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that a navigation in the main frame of an embedded frame tree (i.e. a
// <fencedframe> or a GuestView) does not reset the state.
TEST_F(AtMemoryPersistedStateManagerNavigationTest,
       NoResetOnEmbeddedMainFrame) {
  CreateAutofillDriver();
  autofill_driver(1).SetIsEmbedded(true);

  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ResetAutofillDriver(autofill_driver(1));
  DeleteAutofillDriver(autofill_driver(1));

  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));
}

// Tests that navigation resets all state including search state and previously
// filled suggestions.
TEST_F(AtMemoryPersistedStateManagerNavigationTest, AllStateResetOnNavigation) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAtMemoryPreviouslyFilled};

  Suggestion suggestion(u"Passport #12345", SuggestionType::kAddressEntry);
  state_manager().OnSuggestionAccepted(suggestion);
  ASSERT_EQ(state_manager().previously_filled_suggestions().size(), 1u);

  // Start a new search for the field.
  state_manager().GetStateForField(field_id(), FieldOrigin());
  state_manager().OnFilterSubmitted(u"passport");
  ASSERT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              Field(&AtMemorySearchState::filter, u"passport"));

  ResetAutofillDriver(autofill_driver(0));

  // Search state is reset on navigation.
  EXPECT_THAT(state_manager().GetStateForField(field_id(), FieldOrigin()),
              IsZeroState());

  // Previously filled suggestions are also cleared on navigation.
  EXPECT_TRUE(state_manager().previously_filled_suggestions().empty());
}

// Tests that `metrics_recorder` is preserved when accessing the same field,
// can be transferred via `TakeMetricsRecorder`, and is reset when switching
// fields or when state is cleared.
TEST_F(AtMemoryPersistedStateManagerTest, MetricsRecorderLifecycle) {
  auto make_recorder = [this]() {
    return std::make_unique<AtMemoryMetricsRecorder>(
        /*uploader_service=*/nullptr, /*ukm_recorder=*/nullptr,
        ukm::kInvalidSourceId, GURL("https://example.com"), u"Title",
        field_id(), FormSignature(1), FieldSignature(2));
  };

  state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(state_manager().metrics_recorder(), nullptr);

  auto recorder = make_recorder();
  AtMemoryMetricsRecorder* raw_recorder = recorder.get();
  state_manager().SetMetricsRecorder(std::move(recorder));
  EXPECT_EQ(state_manager().metrics_recorder(), raw_recorder);

  // Accessing the same field keeps `metrics_recorder`.
  state_manager().GetStateForField(field_id(), FieldOrigin());
  EXPECT_EQ(state_manager().metrics_recorder(), raw_recorder);

  // `TakeMetricsRecorder` transfers ownership out of `state_manager()`.
  std::unique_ptr<AtMemoryMetricsRecorder> taken =
      state_manager().TakeMetricsRecorder();
  EXPECT_EQ(taken.get(), raw_recorder);
  EXPECT_EQ(state_manager().metrics_recorder(), nullptr);

  // Setting a new recorder and switching to a different field resets it.
  state_manager().SetMetricsRecorder(make_recorder());
  ASSERT_NE(state_manager().metrics_recorder(), nullptr);
  state_manager().GetStateForField(other_field_id(), OtherFieldOrigin());
  EXPECT_EQ(state_manager().metrics_recorder(), nullptr);

  // Setting a new recorder and accepting a suggestion resets it.
  state_manager().SetMetricsRecorder(make_recorder());
  ASSERT_NE(state_manager().metrics_recorder(), nullptr);
  state_manager().OnSuggestionAccepted(
      Suggestion(u"123 Main St", SuggestionType::kAddressEntry));
  EXPECT_EQ(state_manager().metrics_recorder(), nullptr);
}

}  // namespace
}  // namespace autofill
