// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_manager.h"

#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_manager_test_api.h"
#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder_test_api.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/personal_context/core/mock_personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;

constexpr size_t kVisibleSuffixLength = 4;
constexpr std::u16string_view kDots = u"\u2022\u2060\u2006\u2060";

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ShowAutofillAiFetchEntityFailureNotification,
              (),
              (override));
  MOCK_METHOD(void,
              HideSuggestions,
              (SuggestionHidingReason, std::optional<FillingProduct>),
              (override));

  // Overridden to simulate policy-based blocking using profile preferences.
  // This allows `AtMemoryManagerPolicyTest` and `AtMemoryManagerPrefTest`
  // to toggle policy states without complex mock expectations.
  bool IsAutofillTypeBlockedByPolicy(
      const GURL& url,
      AutofillClient::AutofillPolicyDataCategory category) const override {
    if (category == AutofillClient::AutofillPolicyDataCategory::kPayments) {
      if (!GetPrefs()->GetBoolean(prefs::kAutofillCreditCardEnabled)) {
        return true;
      }
    }
    return TestAutofillClient::IsAutofillTypeBlockedByPolicy(url, category);
  }
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  using TestBrowserAutofillManager::TestBrowserAutofillManager;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(void,
              FillOrPreviewField,
              (mojom::ActionPersistence action_persistence,
               mojom::FieldActionType action_type,
               const FormGlobalId& form_id,
               const FieldGlobalId& field_id,
               const std::u16string& value,
               FillingProduct filling_product,
               std::optional<FieldType> field_type_used),
              (override));
};

class MockAutofillAiAccessManager : public AutofillAiAccessManager {
 public:
  explicit MockAutofillAiAccessManager(BrowserAutofillManager* manager)
      : AutofillAiAccessManager(manager) {}
  ~MockAutofillAiAccessManager() override = default;

  MOCK_METHOD(bool,
              FetchEntityInstance,
              (EntityInstance entity,
               bool will_fill_sensitive_info,
               OnEntityInstanceFetchedCallback callback),
              (override));
};

class AtMemoryManagerTest : public Test,
                            public WithTestAutofillClientDriverManager<
                                NiceMock<MockAutofillClient>,
                                TestAutofillDriver,
                                NiceMock<MockBrowserAutofillManager>> {
 public:
  AtMemoryManagerTest() {
    // AutofillAiWalletPrivatePasses is default enabled on most platforms and
    // affects how sensitive attributes are masked.
    feature_list_.InitWithFeatures(
        {features::kAutofillAtMemory, features::kAutofillAiWalletPrivatePasses},
        {});
  }

  void SetUp() override {
    InitAutofillClient();
    mock_personal_context_service_ = std::make_unique<
        NiceMock<personal_context::MockPersonalContextEligibilityService>>();
    ON_CALL(*mock_personal_context_service_, GetEligibilityState())
        .WillByDefault(Return(
            personal_context::PersonalContextEligibilityState::kEligible));
    autofill_client().set_personal_context_eligibility_service(
        mock_personal_context_service_.get());
    autofill_client().GetPrefs()->registry()->RegisterIntegerPref(
        optimization_guide::prefs::kGeminiSettings,
        static_cast<int>(
            optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
    autofill_client().GetPrefs()->SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        true);
    autofill_client().GetPrefs()->SetInteger(
        optimization_guide::prefs::kGeminiSettings,
        static_cast<int>(
            optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
    auto mock_query_service =
        std::make_unique<NiceMock<MockAtMemoryQueryService>>();
    mock_query_service_ptr_ = mock_query_service.get();
    autofill_client().set_at_memory_query_service(
        std::move(mock_query_service));

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

    CreateAutofillDriver();
  }

  void TearDown() override {
    mock_query_service_ptr_ = nullptr;
    DestroyAutofillClient();
  }

  void AddOrUpdateEntityInstance(const EntityInstance& entity) {
    autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
    webdata_helper().WaitUntilIdle();
  }

  void MockQueryResultsAndExpectCallback(
      std::u16string_view query,
      MemorySearchStatus status,
      std::vector<MemorySearchResult> entries,
      std::vector<Suggestion>& final_suggestions) {
    InSequence s;
    EXPECT_CALL(update_callback_,
                Run(ElementsAre(Field("type", &Suggestion::type,
                                      SuggestionType::kAtMemoryFetching)),
                    AutofillSuggestionTriggerSource::kAtMemoryTriggerString));
    EXPECT_CALL(mock_query_service(), Query(query, _, _, _))
        .WillOnce([status, entries = std::move(entries)](
                      std::u16string_view query, const GURL& url,
                      std::u16string_view title,
                      base::RepeatingCallback<void(MemorySearchResults)>
                          callback) mutable {
          callback.Run(MemorySearchResults(status, std::move(entries)));
        });
    EXPECT_CALL(update_callback_,
                Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
        .WillOnce(SaveArg<0>(&final_suggestions));
  }

  // Creates a test form, calls `AutofillManager::OnFormsSeen` with it and
  // returns a pair of the form's id and its first field's id.
  std::pair<FormGlobalId, FieldGlobalId> SeeForm() {
    const FormData form = test::CreateTestPersonalInformationFormData();
    autofill_manager().AddSeenForm(
        form, std::vector<FieldType>(form.fields().size(), UNKNOWN_TYPE));
    return {form.global_id(), form.fields()[0].global_id()};
  }

  AtMemoryManager& manager() { return autofill_manager().GetAtMemoryManager(); }

  MockAtMemoryQueryService& mock_query_service() {
    return *mock_query_service_ptr_;
  }

  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

  std::pair<FormGlobalId, FieldGlobalId> SeeFormAndShowPopup(
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          parent_suggestion_metadata = std::nullopt,
      bool is_context_secure = true,
      ukm::SourceId ukm_source_id = ukm::kInvalidSourceId) {
    auto [form_id, field_id] = SeeForm();
    manager().OnPopupShown(form_id, field_id, trigger_source,
                           parent_suggestion_metadata, is_context_secure,
                           update_callback_.Get(), ukm_source_id);
    return {form_id, field_id};
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<MockAtMemoryQueryService> mock_query_service_ptr_ = nullptr;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<
      NiceMock<personal_context::MockPersonalContextEligibilityService>>
      mock_personal_context_service_;
};

// Matches a Suggestion of type `kAtMemorySearchResult` with the given
// `memory_data_type` and matching children suggestions.
Matcher<Suggestion> EqualsAtMemorySuggestion(
    MemoryDataType memory_data_type,
    Matcher<std::vector<Suggestion>> children_matcher) {
  return AllOf(
      EqualsSuggestion(SuggestionType::kAtMemorySearchResult),
      ResultOf(
          [](const Suggestion& s) {
            return s.GetPayload<Suggestion::AtMemoryPayload>().memory_data_type;
          },
          memory_data_type),
      Field(&Suggestion::children, children_matcher));
}

// Matches a Suggestion with a "Manage enhanced autofill" footer, the given
// `memory_data_type` and matching children suggestions.
template <typename... Matchers>
Matcher<Suggestion> EqualsSuggestionWithManageEnhancedAutofillFooter(
    MemoryDataType memory_data_type,
    Matchers&&... matchers) {
  auto attribution_matcher = AllOf(
      EqualsSuggestion(SuggestionType::kAtMemorySourceAttribution),
      Field(
          &Suggestion::minor_texts,
          ElementsAre(Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_AT_MEMORY_SOURCE_ATTRIBUTION_PERSONAL_INTELLIGENCE)))),
      Field(&Suggestion::acceptability,
            Suggestion::Acceptability::kSelectableButUnacceptable));

  if constexpr (sizeof...(matchers) == 0) {
    return EqualsAtMemorySuggestion(
        memory_data_type,
        ElementsAre(attribution_matcher,
                    EqualsSuggestion(SuggestionType::kSeparator),
                    EqualsSuggestion(SuggestionType::kManageEnhancedAutofill)));
  } else {
    return EqualsAtMemorySuggestion(
        memory_data_type,
        ElementsAre(std::forward<Matchers>(matchers)...,
                    EqualsSuggestion(SuggestionType::kSeparator),
                    attribution_matcher,
                    EqualsSuggestion(SuggestionType::kSeparator),
                    EqualsSuggestion(SuggestionType::kManageEnhancedAutofill)));
  }
}

// Matches a Suggestion with the given `memory_data_type` and a single footer
// suggestion to manage address settings.
Matcher<Suggestion> EqualsSuggestionWithManageAddressFooter(
    MemoryDataType memory_data_type) {
  return EqualsAtMemorySuggestion(
      memory_data_type,
      ElementsAre(EqualsSuggestion(SuggestionType::kManageAddress)));
}

// Tests that OnFilterChanged with a non-empty filter generates the search
// affordance suggestion and does NOT trigger QueryService::Query.
TEST_F(AtMemoryManagerTest, OnFilterChanged_GeneratesSearchAffordance) {
  SeeFormAndShowPopup();

  EXPECT_CALL(mock_query_service(), Query).Times(0);

  // Expect that OnFilterChanged triggers the callback with the single
  // affordance suggestion.
  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"query");

  ASSERT_GE(suggestions.size(), 1u);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kAtMemorySearchAffordance);
  EXPECT_EQ(suggestions[0].main_text.value, u"query");
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kSpark);
  EXPECT_FALSE(suggestions[0].is_loading);
  ASSERT_EQ(suggestions[0].labels.size(), 1u);
  ASSERT_EQ(suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(suggestions[0].labels[0][0].value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_SUBTITLE));
}

// Tests that `OnFilterChanged` when offline generates the no connection
// suggestion.
TEST_F(AtMemoryManagerTest,
       OnFilterChanged_Offline_GeneratesNoConnectionSuggestion) {
  net::test::ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  SeeFormAndShowPopup();

  autofill_client().set_should_show_personal_context_at_memory_notice(false);

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"query");

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(EqualsSuggestion(SuggestionType::kAtMemoryNoConnection,
                                 u"query", Suggestion::Icon::kSadTab,
                                 {{Suggestion::Text(l10n_util::GetStringUTF16(
                                     IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION))}}),
                Field(&Suggestion::acceptability,
                      Suggestion::Acceptability::kUnselectableAndUnacceptable)),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsSuggestion(SuggestionType::kAtMemoryAiDisclosure)));
}

// Tests that OnFilterChanged with a non-empty filter generates an AI disclosure
// on Desktop when personal context has already been shown/acknowledged.
TEST_F(AtMemoryManagerTest, OnFilterChanged_GeneratesDisclosureWhenEnabled) {
  SeeFormAndShowPopup();

  autofill_client().set_should_show_personal_context_at_memory_notice(false);

  EXPECT_CALL(
      update_callback_,
      Run(ElementsAre(
              EqualsSuggestion(SuggestionType::kAtMemorySearchAffordance),
              EqualsSuggestion(SuggestionType::kSeparator),
              EqualsSuggestion(SuggestionType::kAtMemoryAiDisclosure)),
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnFilterChanged(u"query");
}

// Tests that OnFilterChanged with a non-empty filter does not generate an AI
// disclosure on Desktop when the personal context notice is pending.
TEST_F(AtMemoryManagerTest, OnFilterChanged_NoDisclosureWhenNoticePending) {
  SeeFormAndShowPopup();

  autofill_client().set_should_show_personal_context_at_memory_notice(true);

  EXPECT_CALL(update_callback_,
              Run(Not(Contains(Field("type", &Suggestion::type,
                                     SuggestionType::kAtMemoryAiDisclosure))),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnFilterChanged(u"query");
}

// Tests that OnFilterChanged with an empty filter clears all suggestions.
TEST_F(AtMemoryManagerTest, OnFilterChanged_EmptyFilterClearsSuggestions) {
  SeeFormAndShowPopup();

  EXPECT_CALL(
      update_callback_,
      Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnFilterChanged(u"");
}

// Tests that OnSearchSubmitted triggers full search, shows the fetching
// suggestion, and successfully updates suggestions with the results once they
// arrive.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_TriggersQueryServiceAndClearsSuggestions) {
  SeeFormAndShowPopup();

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(SaveArg<3>(&search_callback));

  // Expect that executing the query immediately shows the fetching suggestion.
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(Field("type", &Suggestion::type,
                                    SuggestionType::kAtMemoryFetching)),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnSearchSubmitted(u"query");

  // Simulate search results returning from the query service.
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  // Expect that when search results arrive, suggestions are updated.
  std::vector<Suggestion> final_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&final_suggestions));

  search_callback.Run(std::move(results));

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"Full Address");
}

// Tests that when a search result has an empty type name and no metadata, the
// generated suggestion has no labels.
TEST_F(AtMemoryManagerTest, OnSearchSubmitted_SchemalessResultHasEmptyLabels) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kUnknown, u"", u"Some Value");

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"Some Value");
  EXPECT_TRUE(final_suggestions[0].labels.empty());
}

// Tests that when a search result has
// `MemoryDataType::kUnknown`, the generated suggestion
// uses the entry's type name for the label.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_UnknownTypeWithTypeName_UsesTypeNameInLabel) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kUnknown, u"Custom Type", u"Some Value");

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"Some Value");
  ASSERT_EQ(final_suggestions[0].labels.size(), 1u);
  ASSERT_EQ(final_suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(final_suggestions[0].labels[0][0].value, u"Custom Type");
}

// Tests that Autofill-sourced data displays ONLY the local settings manage link
// (e.g. kManageAddress) and NOT the "Manage enhanced autofill" footer.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_AutofillSource_ShowsLocalManageFooter) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(MemoryDataType::kAddressFull, u"Address",
                           u"Full Address");
  entry.sources.emplace_back(MemoryEntrySourceType::kAutofill);
  entries.push_back(std::move(entry));

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_THAT(final_suggestions,
              ElementsAre(EqualsSuggestionWithManageAddressFooter(
                  MemoryDataType::kAddressFull)));
}

TEST_F(AtMemoryManagerTest, OnSearchSubmitted_AutofillSource_Flight_Footer) {

  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(MemoryDataType::kFlightReservationFlightNumber,
                           u"Label", u"Value");
  entry.sources.emplace_back(MemoryEntrySourceType::kAutofill);
  entries.push_back(std::move(entry));

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_THAT(
      final_suggestions,
      ElementsAre(EqualsAtMemorySuggestion(
          MemoryDataType::kFlightReservationFlightNumber,
          ElementsAre(EqualsSuggestion(
              SuggestionType::kManageAutofillAiTravel,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_AI_MANAGE_TRAVEL_SUGGESTION_MAIN_TEXT))))));
}

// Tests that Autofill-sourced data of unknown type does not display any manage
// information footer suggestion.
TEST_F(AtMemoryManagerTest, OnSearchSubmitted_AutofillSource_Unknown_NoFooter) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(MemoryDataType::kUnknown, u"Label", u"Value");
  entry.sources.emplace_back(MemoryEntrySourceType::kAutofill);
  entries.push_back(std::move(entry));

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_THAT(final_suggestions, ElementsAre(EqualsAtMemorySuggestion(
                                     MemoryDataType::kUnknown, IsEmpty())));
}

// Tests that Personal Context-sourced data (e.g. from Gmail) displays the
// attribution info, separator, and the "Manage enhanced autofill" footer (but
// not local settings).
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_AISource_ShowsManageEnhancedAutofillFooter) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(MemoryDataType::kAddressFull, u"Address",
                           u"Full Address");
  entry.sources.emplace_back(MemoryEntrySourceType::kGmail);
  entries.push_back(std::move(entry));

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_THAT(final_suggestions,
              ElementsAre(EqualsSuggestionWithManageEnhancedAutofillFooter(
                  MemoryDataType::kAddressFull)));
}

// Tests that data with no source defaults to displaying the Gemini attribution
// and "Manage enhanced autofill" footer.
TEST_F(
    AtMemoryManagerTest,
    OnSearchSubmitted_NoSource_ShowsGeminiAttributionAndManageEnhancedAutofillFooter) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");

  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_THAT(final_suggestions,
              ElementsAre(EqualsSuggestionWithManageEnhancedAutofillFooter(
                  MemoryDataType::kAddressFull)));
}

// Tests that when the user is offline, the manager displays the no connection
// suggestion.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_QueryServiceReturnsNoConnectionFailure) {
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kNoConnectionFailure,
                                    /*entries=*/{}, final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemoryNoConnection);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"query");
  EXPECT_EQ(final_suggestions[0].labels[0][0].value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION));
  EXPECT_EQ(final_suggestions[0].acceptability,
            Suggestion::Acceptability::kUnselectableAndUnacceptable);
}

// Tests that when filling an attribute (e.g. Passport Number), the manager
// fetches the unmasked entity instance from AutofillAiAccessManager and fills
// the unmasked attribute value correctly.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_AttributeSuccess) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport",
                             u"some text");
    entry.identifier = passport.guid().value();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  // Configure the mock access manager.
  auto mock_ai_access_manager =
      std::make_unique<NiceMock<MockAutofillAiAccessManager>>(
          &autofill_manager());
  MockAutofillAiAccessManager* mock_ai_access_manager_ptr =
      mock_ai_access_manager.get();
  test_api(autofill_manager())
      .set_autofill_ai_access_manager(std::move(mock_ai_access_manager));

  base::optional_ref<const AttributeInstance> passport_attribute =
      passport.attribute(AttributeType(AttributeTypeName::kPassportNumber));
  ASSERT_TRUE(passport_attribute.has_value());

  int64_t initial_use_count = passport.use_count();
  task_environment_.FastForwardBy(base::Seconds(60));

  {
    InSequence seq;
    EXPECT_CALL(
        *mock_ai_access_manager_ptr,
        FetchEntityInstance(passport, /*will_fill_sensitive_info=*/true, _))
        .WillOnce([&](EntityInstance entity, bool will_fill,
                      AutofillAiAccessManager::OnEntityInstanceFetchedCallback
                          callback) {
          std::move(callback).Run(entity, /*reauth_attempted=*/false);
          return true;
        });
    EXPECT_CALL(autofill_client(),
                HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                std::optional(FillingProduct::kAtMemory)));
    EXPECT_CALL(
        autofill_manager(),
        FillOrPreviewField(mojom::ActionPersistence::kFill,
                           mojom::FieldActionType::kReplaceAtMemoryTrigger, _,
                           _, passport_attribute->GetCompleteRawInfo(),
                           FillingProduct::kAtMemory, _));
  }

  EXPECT_EQ(manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                                form_id, field_id,
                                                final_suggestions[0]),
            IsAsync(true));

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);

  base::optional_ref<const EntityInstance> updated_entity =
      autofill_client().GetEntityDataManager()->GetEntityInstance(
          passport.guid());
  ASSERT_TRUE(updated_entity.has_value());
  EXPECT_EQ(updated_entity->use_count(), initial_use_count + 1);
}



// Tests that when filling a sensitive Personal Context entry, the
// `AtMemoryQueryService` authenticates, fetches the unmasked value from
// `AtMemoryQueryService`, and fills it.
TEST_F(AtMemoryManagerTest, FillSensitivePersonalContextData_Success) {
  base::HistogramTester histogram_tester;
  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport",
                             u"1234");
    entry.identifier = "personal-context-guid";
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kGmail)};
    entry.metadata_list.emplace_back(MemoryDataType::kPassportExpirationDate,
                                     u"Expiration Date", u"2030-01-01");
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");

  ASSERT_FALSE(final_suggestions.empty());

  {
    InSequence seq;
    EXPECT_CALL(
        mock_query_service(),
        AuthenticateAndFetchPiiEntity(
            Ref(autofill_client()),
            GetAuthenticationMessage(
                autofill_client().GetLastCommittedPrimaryMainFrameOrigin()),
            std::u16string_view(u"1234"), MemoryDataType::kPassportNumber, _,
            _))
        .WillOnce([&](const AutofillClient& client,
                      const std::u16string& auth_message,
                      std::u16string_view masked_value,
                      MemoryDataType data_type,
                      base::span<const EntryMetadata> metadata_list,
                      AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback
                          callback) {
          ASSERT_EQ(metadata_list.size(), 1u);
          EXPECT_EQ(metadata_list[0].type,
                    MemoryDataType::kPassportExpirationDate);
          EXPECT_EQ(metadata_list[0].value, u"2030-01-01");
          std::move(callback).Run(u"unmasked_passport_1234");
        });

    EXPECT_CALL(autofill_client(),
                HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                std::optional(FillingProduct::kAtMemory)));
    EXPECT_CALL(autofill_manager(),
                FillOrPreviewField(
                    mojom::ActionPersistence::kFill,
                    mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
                    field_id, std::u16string(u"unmasked_passport_1234"),
                    FillingProduct::kAtMemory, std::optional<FieldType>()));
  }

  EXPECT_EQ(manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                                form_id, field_id,
                                                final_suggestions[0]),
            IsAsync(true));

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
}

// Tests that when fetching sensitive Personal Context data is pending
// asynchronously and the user clicks away (hides popup), the field is still
// filled when the fetch completes and metrics are recorded correctly.
TEST_F(AtMemoryManagerTest,
       FillSensitivePersonalContextData_PendingFetch_UserClicksAway) {
  base::HistogramTester histogram_tester;
  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport",
                             u"1234");
    entry.identifier = "personal-context-guid";
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kGmail)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_FALSE(final_suggestions.empty());

  AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback captured_callback;
  {
    InSequence seq;
    EXPECT_CALL(
        mock_query_service(),
        AuthenticateAndFetchPiiEntity(
            Ref(autofill_client()),
            GetAuthenticationMessage(
                autofill_client().GetLastCommittedPrimaryMainFrameOrigin()),
            std::u16string_view(u"1234"), MemoryDataType::kPassportNumber, _,
            _))
        .WillOnce([&](const AutofillClient& client,
                      const std::u16string& auth_message,
                      std::u16string_view masked_value,
                      MemoryDataType data_type,
                      base::span<const EntryMetadata> metadata_list,
                      AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback
                          callback) {
          manager().OnPopupHidden();
          captured_callback = std::move(callback);
        });

    EXPECT_CALL(autofill_client(),
                HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                std::optional(FillingProduct::kAtMemory)));
    EXPECT_CALL(autofill_manager(),
                FillOrPreviewField(
                    mojom::ActionPersistence::kFill,
                    mojom::FieldActionType::kReplaceAtMemoryTrigger, form_id,
                    field_id, std::u16string(u"unmasked_passport_1234"),
                    FillingProduct::kAtMemory, std::optional<FieldType>()));
  }

  EXPECT_EQ(manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                                form_id, field_id,
                                                final_suggestions[0]),
            IsAsync(true));

  std::move(captured_callback).Run(u"unmasked_passport_1234");

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
}

// Tests that when fetching the unmasked Personal Context value fails, the
// manager does not fill any value.
TEST_F(AtMemoryManagerTest, FillSensitivePersonalContextData_FetchFailed) {
  base::HistogramTester histogram_tester;
  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport",
                             u"1234");
    entry.identifier = "personal-context-guid";
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kGmail)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");

  EXPECT_CALL(
      mock_query_service(),
      AuthenticateAndFetchPiiEntity(
          Ref(autofill_client()),
          GetAuthenticationMessage(
              autofill_client().GetLastCommittedPrimaryMainFrameOrigin()),
          std::u16string_view(u"1234"), MemoryDataType::kPassportNumber, _, _))
      .WillOnce(RunOnceCallback<5>(base::unexpected(
          AtMemoryQueryService::SpiiRetrievalFailureReason::kFetchFailed)));

  EXPECT_CALL(autofill_manager(), FillOrPreviewField).Times(0);

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.FetchPersonalContextPiiData.FailureReason",
      AtMemoryQueryService::SpiiRetrievalFailureReason::kFetchFailed, 1);
}

// Tests that when fetching the unmasked entity instance fails, the manager
// triggers the fetch failure notification and does not fill any value.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_FetchFailed) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport",
                             u"some text");
    entry.identifier = passport.guid().value();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  auto mock_ai_access_manager =
      std::make_unique<NiceMock<MockAutofillAiAccessManager>>(
          &autofill_manager());
  MockAutofillAiAccessManager* mock_ai_access_manager_ptr =
      mock_ai_access_manager.get();
  test_api(autofill_manager())
      .set_autofill_ai_access_manager(std::move(mock_ai_access_manager));

  EXPECT_CALL(*mock_ai_access_manager_ptr,
              FetchEntityInstance(passport, true, _))
      .WillOnce([&](EntityInstance entity, bool will_fill,
                    AutofillAiAccessManager::OnEntityInstanceFetchedCallback
                        callback) {
        std::move(callback).Run(
            base::unexpected(
                AutofillAiAccessManager::FailureReason::kFetchFailed),
            /*reauth_attempted=*/false);
        return true;
      });

  EXPECT_CALL(autofill_client(),
              ShowAutofillAiFetchEntityFailureNotification());
  EXPECT_CALL(autofill_manager(), FillOrPreviewField).Times(0);

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      false, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Latency.FetchPii.AutofillAi", 0);
}

// Tests that when filling a sensitive Credit Card, the manager fetches the
// unmasked card from CreditCardAccessManager, fills it, and records card use.
TEST_F(AtMemoryManagerTest, FillCreditCard_Success) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetCreditCard();
  card.set_guid(test::MakeGuid(1));
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  autofill_client()
      .GetPaymentsAutofillClient()
      ->set_multiple_request_payments_network_interface(
          std::make_unique<
              payments::MockMultipleRequestPaymentsNetworkInterface>(
              autofill_client().GetURLLoaderFactory(),
              *autofill_client().GetIdentityManager()));

  const CreditCard* added_card = autofill_client()
                                     .GetPersonalDataManager()
                                     .payments_data_manager()
                                     .GetCreditCardByGUID(test::MakeGuid(1));
  ASSERT_TRUE(added_card);
  size_t initial_use_count = added_card->usage_history().use_count();

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kCreditCardNumber, u"Card",
                             u"some text");
    entry.identifier = card.guid();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         card.number(), FillingProduct::kAtMemory, _));

  task_environment_.FastForwardBy(base::Seconds(60));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Latency.FetchPii.CreditCard", 1);

  const CreditCard* updated_card = autofill_client()
                                       .GetPersonalDataManager()
                                       .payments_data_manager()
                                       .GetCreditCardByGUID(test::MakeGuid(1));
  ASSERT_TRUE(updated_card);
  EXPECT_EQ(updated_card->usage_history().use_count(), initial_use_count + 1);
}

// Tests that fetching an unmasked IBAN asynchronously returns `IsAsync(true)`,
// hides suggestions when the fetch completes, fills the field, and records
// metrics.
TEST_F(AtMemoryManagerTest, FillIban_Success) {
  base::HistogramTester histogram_tester;
  Iban iban = test::GetLocalIban();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddIbanForTest(std::make_unique<Iban>(iban));

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kIban, u"IBAN", u"some text");
    entry.identifier = iban.guid();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  MockIbanAccessManager* mock_iban_access_manager =
      autofill_client().GetPaymentsAutofillClient()->GetIbanAccessManager();

  IbanAccessManager::OnIbanFetchedCallback fetch_callback;
  {
    InSequence seq;
    EXPECT_CALL(*mock_iban_access_manager, FetchValue)
        .WillOnce([&](const Suggestion::Payload& payload,
                      IbanAccessManager::OnIbanFetchedCallback callback) {
          fetch_callback = std::move(callback);
          return IsAsync(true);
        });

    EXPECT_CALL(autofill_client(),
                HideSuggestions(SuggestionHidingReason::kAcceptSuggestion,
                                std::optional(FillingProduct::kAtMemory)));
    EXPECT_CALL(
        autofill_manager(),
        FillOrPreviewField(mojom::ActionPersistence::kFill,
                           mojom::FieldActionType::kReplaceAtMemoryTrigger, _,
                           _, iban.value(), FillingProduct::kAtMemory, _));
  }

  EXPECT_EQ(manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                                form_id, field_id,
                                                final_suggestions[0]),
            IsAsync(true));

  std::move(fetch_callback).Run(iban.value());

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Latency.FetchPii.Iban",
                                    1);
}

// Tests that SPII entries and metadata are filtered out from the search
// results when the context is insecure.
TEST_F(AtMemoryManagerTest, FiltersSpiiInInsecureContext) {
  SeeFormAndShowPopup(AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                      /*parent_suggestion_metadata=*/std::nullopt,
                      /*is_context_secure=*/false);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(SaveArg<3>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<MemorySearchResult> entries;
  // Non-SPII entry.
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  // SPII entry.
  entries.emplace_back(MemoryDataType::kPassportNumber, u"IBAN", u"1234");

  // Non-SPII entry with mixed metadata.
  MemorySearchResult mixed_entry(MemoryDataType::kPhone, u"Phone", u"123");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kPhone, u"Phone meta",
                                         u"123");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                         u"IBAN meta", u"1234");
  entries.push_back(std::move(mixed_entry));

  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  search_callback.Run(std::move(results));

  EXPECT_THAT(resulting_suggestions,
              ElementsAre(EqualsSuggestionWithManageEnhancedAutofillFooter(
                              MemoryDataType::kAddressFull),
                          EqualsSuggestionWithManageEnhancedAutofillFooter(
                              MemoryDataType::kPhone,
                              EqualsAtMemorySuggestion(
                                  MemoryDataType::kPhone,
                                  /*children_matcher=*/IsEmpty()))));
}

// Tests that SPII entries and metadata are filtered out from the search
// results when the device does not support OS reauth, even in a secure context.
TEST_F(AtMemoryManagerTest, FiltersSpiiWhenDeviceReauthNotSupported) {
  autofill_client().set_supports_device_reauth(false);

  // The search results delivered by the server.
  std::vector<MemorySearchResult> entries;
  // Non-SPII entry.
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  // SPII entry.
  entries.emplace_back(MemoryDataType::kIban, u"IBAN", u"1234");
  // Non-SPII entry with mixed metadata.
  MemorySearchResult mixed_entry(MemoryDataType::kDriversLicenseName, u"Name",
                                 u"John");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kDriversLicenseState,
                                         u"State", u"CA");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kDriversLicenseNumber,
                                         u"Number", u"56789");
  entries.push_back(std::move(mixed_entry));

  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  SeeFormAndShowPopup();

  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(RunOnceCallback<3>(std::move(results)));

  InSequence s;
  // Executing the query immediately shows the fetching suggestion before
  // returning search results.
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(Field("type", &Suggestion::type,
                                    SuggestionType::kAtMemoryFetching)),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(EqualsSuggestionWithManageEnhancedAutofillFooter(
                                  MemoryDataType::kAddressFull),
                              EqualsSuggestionWithManageEnhancedAutofillFooter(
                                  MemoryDataType::kDriversLicenseName,
                                  EqualsAtMemorySuggestion(
                                      MemoryDataType::kDriversLicenseState,
                                      /*children_matcher=*/IsEmpty()))),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnSearchSubmitted(u"query");
}

// Tests that SPII entries are retained in the search results when the device
// does not support OS reauth, but the debug feature is enabled.
TEST_F(AtMemoryManagerTest,
       KeepsSpiiWhenDeviceReauthNotSupportedWithDebugFlag) {
  base::test::ScopedFeatureList debug_features(
      features::debug::kAtMemoryNoDeviceReauthCheck);
  autofill_client().set_supports_device_reauth(false);

  MemorySearchResults results(
      MemorySearchStatus::kFinalResponseSuccess,
      {MemorySearchResult(MemoryDataType::kIban, u"IBAN", u"1234")});

  SeeFormAndShowPopup();

  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(RunOnceCallback<3>(std::move(results)));

  InSequence s;
  // Executing the query immediately shows the fetching suggestion before
  // returning search results.
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(Field("type", &Suggestion::type,
                                    SuggestionType::kAtMemoryFetching)),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(EqualsSuggestionWithManageEnhancedAutofillFooter(
                      MemoryDataType::kIban)),
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  manager().OnSearchSubmitted(u"query");
}

// Tests that SPII entries and metadata are retained in the search results
// when the context is secure.
TEST_F(AtMemoryManagerTest, KeepsSpiiInSecureContext) {
  SeeFormAndShowPopup();

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(SaveArg<3>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<MemorySearchResult> entries;
  // Non-SPII entry.
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  // SPII entry.
  entries.emplace_back(MemoryDataType::kPassportNumber, u"IBAN", u"1234");

  // Non-SPII entry with mixed metadata.
  MemorySearchResult mixed_entry(MemoryDataType::kPhone, u"Phone", u"123");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kPhone, u"Phone meta",
                                         u"123");
  mixed_entry.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                         u"IBAN meta", u"1234");
  entries.push_back(std::move(mixed_entry));

  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  search_callback.Run(std::move(results));

  EXPECT_THAT(
      resulting_suggestions,
      ElementsAre(
          EqualsSuggestionWithManageEnhancedAutofillFooter(
              MemoryDataType::kAddressFull),
          EqualsSuggestionWithManageEnhancedAutofillFooter(
              MemoryDataType::kPassportNumber),
          EqualsSuggestionWithManageEnhancedAutofillFooter(
              MemoryDataType::kPhone,
              EqualsAtMemorySuggestion(MemoryDataType::kPhone,
                                       /*children_matcher=*/IsEmpty()),
              EqualsAtMemorySuggestion(MemoryDataType::kPassportNumber,
                                       /*children_matcher=*/IsEmpty()))));
}

struct AtMemoryManagerFilterTestCase {
  MemoryDataType type;
  std::u16string type_name;
  std::u16string value;
  MemoryEntrySourceType source;
  bool should_be_kept;
};

class AtMemoryManagerPolicyTest
    : public AtMemoryManagerTest,
      public WithParamInterface<AtMemoryManagerFilterTestCase> {};

class AtMemoryManagerPrefTest
    : public AtMemoryManagerTest,
      public WithParamInterface<AtMemoryManagerFilterTestCase> {};

// Tests that suggestions are filtered out when blocked by policy.
TEST_P(AtMemoryManagerPolicyTest, RespectsEnterprisePolicy) {
  SeeFormAndShowPopup();

  // Block payments and identity docs.
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kPayments, true);
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kIdentityDocs, true);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(SaveArg<3>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(GetParam().type, GetParam().type_name,
                           GetParam().value);
  entry.sources.emplace_back(GetParam().source);
  entries.push_back(std::move(entry));

  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  search_callback.Run(std::move(results));

  if (GetParam().should_be_kept) {
    EXPECT_THAT(resulting_suggestions,
                ElementsAre(EqualsAtMemorySuggestion(GetParam().type, _)));
    ASSERT_EQ(resulting_suggestions.size(), 1u);
    EXPECT_EQ(resulting_suggestions[0].main_text.value, GetParam().value);
  } else {
    ASSERT_EQ(resulting_suggestions.size(), 1u);
    EXPECT_EQ(resulting_suggestions[0].type,
              SuggestionType::kAtMemorySearchResult);
    EXPECT_EQ(resulting_suggestions[0].acceptability,
              Suggestion::Acceptability::kSelectableButUnacceptable);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemoryManagerPolicyTest,
    Values(AtMemoryManagerFilterTestCase{MemoryDataType::kAddressFull,
                                         u"Address", u"Full Address",
                                         MemoryEntrySourceType::kAutofill,
                                         true},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kCreditCardNumber, u"Credit Card",
               base::StrCat({kDots, kDots, kDots, kDots, u"1111"}),
               MemoryEntrySourceType::kAutofill, false},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kCreditCardNumber, u"Credit Card",
               base::StrCat({kDots, kDots, kDots, kDots, u"2222"}),
               MemoryEntrySourceType::kGmail, true},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kPassportNumber, u"Passport",
               base::StrCat({kDots, kDots, kDots, kDots, u"1234"}),
               MemoryEntrySourceType::kAutofill, false},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kPassportNumber, u"Passport",
               base::StrCat({kDots, kDots, kDots, kDots, u"5678"}),
               MemoryEntrySourceType::kGmail, true}));

// Tests that credit card suggestions are filtered out when the credit card
// autofill preference is disabled.
TEST_P(AtMemoryManagerPrefTest, FiltersOutCreditCardsWhenPrefDisabled) {
  SeeFormAndShowPopup();

  // Disable credit card autofill preference.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(SaveArg<3>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<MemorySearchResult> entries;
  MemorySearchResult entry(GetParam().type, GetParam().type_name,
                           GetParam().value);
  entry.sources.emplace_back(GetParam().source);
  entries.push_back(std::move(entry));

  MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                              std::move(entries));

  search_callback.Run(std::move(results));

  if (GetParam().should_be_kept) {
    EXPECT_THAT(resulting_suggestions,
                ElementsAre(EqualsAtMemorySuggestion(GetParam().type, _)));
    ASSERT_EQ(resulting_suggestions.size(), 1u);
    EXPECT_EQ(resulting_suggestions[0].main_text.value, GetParam().value);
  } else {
    ASSERT_EQ(resulting_suggestions.size(), 1u);
    EXPECT_EQ(resulting_suggestions[0].type,
              SuggestionType::kAtMemorySearchResult);
    EXPECT_EQ(resulting_suggestions[0].acceptability,
              Suggestion::Acceptability::kSelectableButUnacceptable);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemoryManagerPrefTest,
    Values(AtMemoryManagerFilterTestCase{MemoryDataType::kAddressFull,
                                         u"Address", u"Full Address",
                                         MemoryEntrySourceType::kAutofill,
                                         true},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kCreditCardNumber, u"Credit Card",
               base::StrCat({kDots, kDots, kDots, kDots, u"1111"}),
               MemoryEntrySourceType::kAutofill, false},
           AtMemoryManagerFilterTestCase{
               MemoryDataType::kCreditCardNumber, u"Credit Card",
               base::StrCat({kDots, kDots, kDots, kDots, u"2222"}),
               MemoryEntrySourceType::kGmail, true}));

// Tests that non-SPII data fills correctly and records the funnel metrics.
TEST_F(AtMemoryManagerTest, FillNonSensitiveData_Success) {
  base::HistogramTester histogram_tester;
  auto [form_id, field_id] = SeeFormAndShowPopup();

  AutofillProfile profile = test::GetFullProfile();
  profile.set_guid(test::MakeGuid(1));
  autofill_client().GetPersonalDataManager().address_data_manager().AddProfile(
      profile);

  const AutofillProfile* added_profile =
      autofill_client()
          .GetPersonalDataManager()
          .address_data_manager()
          .GetProfileByGUID(test::MakeGuid(1));
  ASSERT_TRUE(added_profile);
  uint64_t initial_use_count = added_profile->usage_history().use_count();


  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kNameFull, u"Name", u"John Doe");
    entry.identifier = profile.guid();
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  std::u16string expected_value = u"John Doe";
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         expected_value, FillingProduct::kAtMemory, _));

  task_environment_.FastForwardBy(base::Seconds(60));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);

  const AutofillProfile* updated_profile =
      autofill_client()
          .GetPersonalDataManager()
          .address_data_manager()
          .GetProfileByGUID(test::MakeGuid(1));
  EXPECT_EQ(updated_profile->usage_history().use_count(),
            initial_use_count + 1);
}

// Tests that funnel metrics are recorded correctly even if multiple are shown.
TEST_F(AtMemoryManagerTest, FillOverlappingPopups) {
  base::HistogramTester histogram_tester;

  // 1. Show Popup 1.
  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kIban, u"IBAN", u"some text");
    entry.identifier = "12345678-1234-1234-1234-123456789012";
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  MockIbanAccessManager* mock_iban_access_manager =
      autofill_client().GetPaymentsAutofillClient()->GetIbanAccessManager();

  IbanAccessManager::OnIbanFetchedCallback fetch_callback;
  EXPECT_CALL(*mock_iban_access_manager, FetchValue)
      .WillOnce([&](const Suggestion::Payload& payload,
                    IbanAccessManager::OnIbanFetchedCallback callback) {
        fetch_callback = std::move(callback);
        return IsAsync(true);
      });

  // 2. Accept async suggestion on Popup 1.
  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  // 3. Hide Popup 1.
  manager().OnPopupHidden();

  // At this stage:
  // - Popup 1's displayed is logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.SearchBarDisplayed"),
      BucketsAre(
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1)));
  // - QuerySubmitted, SuggestionAccepted, SuggestionFilled, TimeToFetchUnmasked
  // are not logged yet because Popup 1's async fill is still pending.
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.QuerySubmitted", 0);
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.SuggestionFilled", 0);
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Latency.FetchPii.Iban",
                                    0);

  // 4. Show Popup 2 (overlapping with the pending async fill of Popup 1).
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_2;
  manager().OnPopupShown(form_id, field_id,
                         AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_2.Get(),
                         ukm::kInvalidSourceId);

  // 5. Hide Popup 2 (without accepting suggestions).
  manager().OnPopupHidden();

  // Verify Popup 2 logged its displayed, query submitted, suggestion accepted:
  // - PopupDisplayed should have context menu trigger as well now.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.SearchBarDisplayed"),
      BucketsAre(
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1),
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1)));
  // - QuerySubmitted should have one sample (false, from Popup 2).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.QuerySubmitted"),
      BucketsAre(Bucket(false, 1)));
  // - SuggestionAccepted should not have any samples yet (Popup 2 did not
  //   submit a query, and Popup 1 is still pending).
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);

  // 6. Complete the async fill for Popup 1.
  std::move(fetch_callback).Run(u"ES12345678901234567890");

  // Now, Popup 1's metrics should also be logged:
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.QuerySubmitted"),
      BucketsAre(Bucket(false, 1), Bucket(true, 1)));
  // - SuggestionAccepted should have one true (from Popup 1).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.SuggestionAccepted"),
      BucketsAre(Bucket(true, 1)));
  // - SuggestionFilled should be logged as true.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.SuggestionFilled"),
      BucketsAre(Bucket(true, 1)));
  // - TimeToFetchUnmasked should be logged.
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Latency.FetchPii.Iban",
                                    1);
}

// Tests that the personal context notice is appended when the user needs to see
// the notice.
TEST_F(AtMemoryManagerTest, PersonalContext_AppendsNoticeSuggestion) {
  autofill_client().set_should_show_personal_context_at_memory_notice(true);

  SeeFormAndShowPopup();

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

  ASSERT_EQ(1u, suggestions.size());
  EXPECT_EQ(SuggestionType::kPersonalContextNotice, suggestions[0].type);
  EXPECT_EQ(Suggestion::FiltrationPolicy::kStatic,
            suggestions[0].filtration_policy);
}

// Tests that before search results are returned (when only the search
// affordance suggestion to start a query is shown), the personal context notice
// is appended at the end (after the search affordance suggestion).
TEST_F(AtMemoryManagerTest,
       PersonalContext_NoticePositioning_SearchAffordance) {
  autofill_client().set_should_show_personal_context_at_memory_notice(true);
  SeeFormAndShowPopup();

  // Set up expectation for `update_callback_` when the filter text changes.
  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&suggestions));

  // Simulate user typing in the search bar to show the search affordance.
  manager().OnFilterChanged(u"query");

  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(SuggestionType::kAtMemorySearchAffordance, suggestions[0].type);
  EXPECT_EQ(SuggestionType::kSeparator, suggestions[1].type);
  EXPECT_EQ(SuggestionType::kPersonalContextNotice, suggestions[2].type);
}

// Tests that after search results are returned, the personal context notice
// is prepended at the top (before the search result suggestions).
TEST_F(AtMemoryManagerTest, PersonalContext_NoticePositioning_SearchResults) {
  autofill_client().set_should_show_personal_context_at_memory_notice(true);
  SeeFormAndShowPopup();

  // Mock search results returned by the query service.
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kUnknown, u"", u"Some Value");

  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _))
      .WillOnce(RunOnceCallback<3>(MemorySearchResults(
          MemorySearchStatus::kFinalResponseSuccess, std::move(entries))));

  // Capture the `suggestions` delivered to the `update_callback_`.
  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillRepeatedly(SaveArg<0>(&suggestions));

  // Submit the search query to trigger query execution.
  manager().OnSearchSubmitted(u"query");

  // Verify that the personal context notice card is prepended first, followed
  // by the search result entry.
  ASSERT_EQ(2u, suggestions.size());
  EXPECT_EQ(SuggestionType::kPersonalContextNotice, suggestions[0].type);
  EXPECT_EQ(SuggestionType::kAtMemorySearchResult, suggestions[1].type);
}

// Tests that during the fetching state (while search is in progress), the UI
// receives the `kAtMemoryFetching` meta-suggestion followed by a separator and
// the notice card if active.
TEST_F(AtMemoryManagerTest, FetchingState_Suggestions_NoticeActive) {
  autofill_client().set_should_show_personal_context_at_memory_notice(true);
  auto [form_id, field_id] = SeeForm();
  manager().OnPopupShown(
      form_id, field_id,
      AutofillSuggestionTriggerSource::kAtMemoryTriggerString, std::nullopt,
      /*is_context_secure=*/true, update_callback_.Get(),
      ukm::kInvalidSourceId);

  EXPECT_CALL(
      update_callback_,
      Run(ElementsAre(
              Field(&Suggestion::type, SuggestionType::kAtMemoryFetching),
              Field(&Suggestion::type, SuggestionType::kSeparator),
              Field(&Suggestion::type, SuggestionType::kPersonalContextNotice)),
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  // Trigger query without completing it immediately to observe fetching state.
  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"query"), _, _, _));

  manager().OnSearchSubmitted(u"query");
}

// Tests that during the fetching state when the notice has been accepted,
// the UI receives only `kAtMemoryFetching` meta-suggestion.
TEST_F(AtMemoryManagerTest, FetchingState_Suggestions_NoticeAccepted) {
  autofill_client().set_should_show_personal_context_at_memory_notice(false);

  auto [form_id, field_id] = SeeForm();
  manager().OnPopupShown(
      form_id, field_id,
      AutofillSuggestionTriggerSource::kAtMemoryTriggerString, std::nullopt,
      /*is_context_secure=*/true, update_callback_.Get(),
      ukm::kInvalidSourceId);

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemoryTriggerString))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

  EXPECT_TRUE(suggestions.empty());
}

// Tests that when Glic is enabled and search returns `kUnsupportedQuery`,
// the unsupported query suggestion is returned.
TEST_F(
    AtMemoryManagerTest,
    OnSearchSubmitted_UnsupportedQuery_GlicEnabled_UnsupportedQuerySuggestion) {
  base::HistogramTester histogram_tester;

  autofill_client().set_is_glic_enabled(true);
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kUnsupportedQuery,
                                    /*entries=*/{}, final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kOpenGemini);

  // Verify that we still logged that some sort of suggestion was shown to the
  // user despite it not being an AtMemory suggestion.
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Latency.Query", 1);
}

// Tests that when Glic is disabled and search returns `kUnsupportedQuery`,
// it falls back to the no data suggestion.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_UnsupportedQuery_GlicDisabled_NoDataSuggestion) {
  autofill_client().set_is_glic_enabled(false);
  SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kUnsupportedQuery,
                                    /*entries=*/{}, final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_DATA));
}

// Tests that search query completion does not log the QueryCompleted UMA
// metric on partial responses.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_DoesNotLogQueryCompletedMetricsOnPartialResponse) {
  base::HistogramTester histogram_tester;
  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kPartialResponseSuccess,
                                    std::move(entries), final_suggestions);
  SeeFormAndShowPopup();

  manager().OnSearchSubmitted(u"query");

  histogram_tester.ExpectTotalCount("Autofill.AtMemory.QueryCompleted", 0);
}

// Tests that search query completion logs the QueryCompleted UMA metric
// correctly on final responses.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_LogsQueryCompletedMetricsOnFinalResponse) {
  base::HistogramTester histogram_tester;
  std::vector<Suggestion> final_suggestions;
  std::vector<MemorySearchResult> entries;
  entries.emplace_back(MemoryDataType::kAddressFull, u"Address",
                       u"Full Address");
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);
  SeeFormAndShowPopup();

  manager().OnSearchSubmitted(u"query");

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.QueryCompleted",
      AtMemoryQueryCompletedStatus::kQueryReturnedData, 1);
}

// Tests that a remote sensitive main entry value is obfuscated in the
// suggestions list UI, while keeping the raw value in its payload.
// Also verifies that previewing the suggestion uses the obfuscated value,
// while filling uses the raw value directly.
TEST_F(AtMemoryManagerTest, RemoteSensitiveMainValue_Obfuscated) {
  auto [form_id, field_id] = SeeFormAndShowPopup();
  // Create an entry where the primary value is sensitive and metadata is
  // non-sensitive.
  MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport Number",
                           u"987654321");
  entry.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                   u"John Doe");
  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    {entry}, final_suggestions);
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  // 1. Verify Primary Suggestion obfuscation in the UI list.
  EXPECT_EQ(final_suggestions[0].main_text.value,
            GetObfuscatedValue(u"987654321", kVisibleSuffixLength));

  // The label row is formatted as: [type_name, bullet, metadata_value]
  // Check that the non-sensitive metadata value is NOT obfuscated.
  ASSERT_EQ(final_suggestions[0].labels.size(), 1u);
  ASSERT_EQ(final_suggestions[0].labels[0].size(), 3u);
  EXPECT_EQ(final_suggestions[0].labels[0][2].value, u"John Doe");

  // 2. Verify that the primary payload retains the raw value.
  const Suggestion::AtMemoryPayload& primary_payload =
      final_suggestions[0].GetPayload<Suggestion::AtMemoryPayload>();
  EXPECT_EQ(primary_payload.value, u"987654321");

  // 3. Verify Preview and Fill of the Primary Suggestion.
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         GetObfuscatedValue(u"987654321", kVisibleSuffixLength),
                         FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kPreview,
                                      form_id, field_id, final_suggestions[0]);

  EXPECT_CALL(
      mock_query_service(),
      AuthenticateAndFetchPiiEntity(
          Ref(autofill_client()),
          GetAuthenticationMessage(
              autofill_client().GetLastCommittedPrimaryMainFrameOrigin()),
          std::u16string_view(u"987654321"), MemoryDataType::kPassportNumber, _,
          _))
      .WillOnce(RunOnceCallback<5>(u"987654321"));

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(
                  mojom::ActionPersistence::kFill,
                  mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                  std::u16string(u"987654321"), FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);
}

// Tests that CVC (`kCreditCardSecurityCode`) in metadata is excluded from the
// main suggestion labels.
TEST_F(AtMemoryManagerTest, CvcMetadata_ExcludedFromLabels) {
  SeeFormAndShowPopup();

  // Create a credit card entry with CVC and Name in metadata.
  MemorySearchResult entry(MemoryDataType::kCreditCardNumber, u"Card Number",
                           u"1234567890123456");
  entry.metadata_list.emplace_back(MemoryDataType::kCreditCardSecurityCode,
                                   u"CVC",
                                   std::u16string(3, kMidlineEllipsisPlainDot));
  entry.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                   u"John Doe");

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    {entry}, final_suggestions);
  manager().OnSearchSubmitted(u"query");

  // Verify that the CVC is NOT in the labels, but Name is.
  // The label row should be: [type_name, bullet, Name]
  // (CVC and its bullet should be skipped).
  std::vector<std::vector<Suggestion::Text>> expected_labels = {
      {Suggestion::Text(u"Card number"), Suggestion::Text(u"\u2022"),
       Suggestion::Text(u"John Doe")}};

  EXPECT_THAT(final_suggestions,
              ElementsAre(EqualsSuggestion(
                  SuggestionType::kAtMemorySearchResult,
                  GetObfuscatedValue(u"1234567890123456", kVisibleSuffixLength),
                  Suggestion::Icon::kCardGenericSpark, expected_labels)));
}

// Tests that when an AtMemory search result is an AutofillAi attribute type
// (e.g. `kFlightReservationFlightNumber`), the main suggestion label uses the
// general Entity name, while child suggestions (metadata entries) still use
// attribute names.
TEST_F(AtMemoryManagerTest,
       AutofillAiAttribute_UsesEntityNameForMainSuggestionLabel) {
  SeeFormAndShowPopup();

  MemorySearchResult entry(MemoryDataType::kFlightReservationFlightNumber,
                           u"Flight number", u"UA123");
  entry.metadata_list.emplace_back(
      MemoryDataType::kFlightReservationArrivalAirport, u"Destination airport",
      u"SFO");

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    {entry}, final_suggestions);
  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  // Main suggestion label row should start with Entity name ("Flight"), not
  // attribute name ("Flight number").
  ASSERT_EQ(final_suggestions[0].labels.size(), 1u);
  ASSERT_GE(final_suggestions[0].labels[0].size(), 1u);
  EXPECT_EQ(final_suggestions[0].labels[0][0].value, u"Flight");

  // Child suggestion label should retain the attribute name ("Destination
  // airport").
  ASSERT_GE(final_suggestions[0].children.size(), 1u);
  ASSERT_EQ(final_suggestions[0].children[0].labels.size(), 1u);
  ASSERT_EQ(final_suggestions[0].children[0].labels[0].size(), 1u);
  EXPECT_EQ(final_suggestions[0].children[0].labels[0][0].value,
            u"Destination airport");
}

// Tests that sensitive metadata is obfuscated in the primary suggestion labels
// and in the child flyout menu, while keeping the raw value in its payload.
// Also verifies that previewing the child suggestion uses the obfuscated value,
// while filling uses the raw value directly.
TEST_F(AtMemoryManagerTest, RemoteSensitiveMetadata_Obfuscated) {
  auto [form_id, field_id] = SeeFormAndShowPopup();
  // Create an entry where the primary value is non-sensitive and metadata is
  // sensitive.
  MemorySearchResult entry(MemoryDataType::kNameFull, u"Name", u"John Doe");
  entry.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                   u"Passport Number", u"987654321");
  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    {entry}, final_suggestions);
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  // 1. Verify Primary Suggestion main text is NOT obfuscated.
  EXPECT_EQ(final_suggestions[0].main_text.value, u"John Doe");

  // The label row is formatted as: [type_name, bullet, metadata_value]
  // Check that the sensitive metadata value is obfuscated in the labels.
  ASSERT_EQ(final_suggestions[0].labels.size(), 1u);
  ASSERT_EQ(final_suggestions[0].labels[0].size(), 3u);
  EXPECT_EQ(final_suggestions[0].labels[0][2].value,
            GetObfuscatedValue(u"987654321", kVisibleSuffixLength));

  // 2. Verify Child Suggestion obfuscation in the flyout menu.
  ASSERT_EQ(final_suggestions[0].children.size(), 5u);
  EXPECT_EQ(final_suggestions[0].children[0].main_text.value,
            GetObfuscatedValue(u"987654321", kVisibleSuffixLength));

  // 3. Verify that the child payload retains the raw value.
  const Suggestion::AtMemoryPayload& child_payload =
      final_suggestions[0]
          .children[0]
          .GetPayload<Suggestion::AtMemoryPayload>();
  EXPECT_EQ(child_payload.value, u"987654321");

  // 4. Verify Preview and Fill of the Child Suggestion.
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         GetObfuscatedValue(u"987654321", kVisibleSuffixLength),
                         FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kPreview,
                                      form_id, field_id,
                                      final_suggestions[0].children[0]);

  EXPECT_CALL(
      mock_query_service(),
      AuthenticateAndFetchPiiEntity(
          Ref(autofill_client()),
          GetAuthenticationMessage(
              autofill_client().GetLastCommittedPrimaryMainFrameOrigin()),
          std::u16string_view(u"987654321"), MemoryDataType::kPassportNumber, _,
          _))
      .WillOnce(
          [&](const AutofillClient& client, const std::u16string& auth_message,
              std::u16string_view masked_value, MemoryDataType data_type,
              base::span<const EntryMetadata> metadata_list,
              AtMemoryQueryService::FetchUnmaskedPiiEntitiesCallback callback) {
            std::move(callback).Run(u"987654321");
          });

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(
                  mojom::ActionPersistence::kFill,
                  mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                  std::u16string(u"987654321"), FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id,
                                      final_suggestions[0].children[0]);
}

TEST_F(AtMemoryManagerTest, OnPopupShown_SubPopup_DoesNotResetRecorder) {
  base::HistogramTester histogram_tester;

  // 1. Show root popup. This should initialize the metrics recorder.
  auto [form_id, field_id] = SeeFormAndShowPopup();

  // 2. Show sub-popup. This should NOT reset the recorder.
  AutofillSuggestionDelegate::SuggestionMetadata metadata;
  metadata.multi_index = {0, 0};  // sub-popup
  manager().OnPopupShown(
      form_id, field_id,
      AutofillSuggestionTriggerSource::kAtMemoryTriggerString, metadata,
      /*is_context_secure=*/true, update_callback_.Get(),
      ukm::kInvalidSourceId);

  // If it had reset, the first recorder would have been destroyed and logged
  // "QuerySubmitted".
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.QuerySubmitted", 0);

  // 3. Hide popup. This should destroy the recorder and log the metric.
  manager().OnPopupHidden();
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.QuerySubmitted", false,
                                      1);
}

TEST_F(AtMemoryManagerTest, OnPopupShown_PassesSignaturesToMetricsRecorder) {
  auto [form_id, field_id] = SeeFormAndShowPopup();
  auto [form_structure, autofill_field] =
      autofill_manager().FindFormAndField(form_id, field_id);
  ASSERT_TRUE(form_structure);
  ASSERT_TRUE(autofill_field);


  AtMemoryMetricsRecorder* recorder =
      test_api(manager()).at_memory_metrics_recorder();
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(test_api(*recorder).form_signature(),
            form_structure->form_signature());
  EXPECT_EQ(test_api(*recorder).field_signature(),
            autofill_field->GetFieldSignature());
}

TEST_F(AtMemoryManagerTest, FillNonSensitiveCreditCard) {
  base::HistogramTester histogram_tester;
  CreditCard card = test::GetCreditCard();
  card.set_guid(test::MakeGuid(1));
  autofill_client()
      .GetPersonalDataManager()
      .payments_data_manager()
      .AddCreditCard(card);

  const CreditCard* added_card = autofill_client()
                                     .GetPersonalDataManager()
                                     .payments_data_manager()
                                     .GetCreditCardByGUID(test::MakeGuid(1));
  ASSERT_TRUE(added_card);
  size_t initial_use_count = added_card->usage_history().use_count();

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kCreditCardNameOnCard, u"Name",
                             card.GetRawInfo(CREDIT_CARD_NAME_FULL));
    entry.identifier = card.guid();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         card.GetRawInfo(CREDIT_CARD_NAME_FULL),
                         FillingProduct::kAtMemory, _));

  task_environment_.FastForwardBy(base::Seconds(60));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);

  const CreditCard* updated_card = autofill_client()
                                       .GetPersonalDataManager()
                                       .payments_data_manager()
                                       .GetCreditCardByGUID(test::MakeGuid(1));
  ASSERT_TRUE(updated_card);
  EXPECT_EQ(updated_card->usage_history().use_count(), initial_use_count + 1);
}

TEST_F(AtMemoryManagerTest, FillNonSensitiveAutofillAi) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  base::optional_ref<const EntityInstance> added_passport =
      autofill_client().GetEntityDataManager()->GetEntityInstance(
          passport.guid());
  ASSERT_TRUE(added_passport.has_value());
  int64_t initial_use_count = added_passport->use_count();

  auto [form_id, field_id] = SeeFormAndShowPopup();

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportName, u"Passport Name",
                             u"John Doe");
    entry.identifier = passport.guid().value();
    entry.sources = {MemoryEntrySource(MemoryEntrySourceType::kAutofill)};
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  std::u16string expected_value = u"John Doe";
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         expected_value, FillingProduct::kAtMemory, _));

  task_environment_.FastForwardBy(base::Seconds(60));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);

  base::optional_ref<const EntityInstance> updated_passport =
      autofill_client().GetEntityDataManager()->GetEntityInstance(
          passport.guid());
  ASSERT_TRUE(updated_passport.has_value());
  EXPECT_EQ(updated_passport->use_count(), initial_use_count + 1);
}

enum class SourceScenario { kNoSources, kAutofillOnly, kGmailOnly, kMixed };

class AtMemoryManagerIconTest : public AtMemoryManagerTest,
                                public WithParamInterface<SourceScenario> {
 public:
  SourceScenario scenario() const { return GetParam(); }

  static std::vector<MemoryEntrySource> GetSourcesForScenario(
      SourceScenario scenario) {
    std::vector<MemoryEntrySource> sources;
    switch (scenario) {
      case SourceScenario::kNoSources:
        break;
      case SourceScenario::kAutofillOnly:
        sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kAutofill));
        break;
      case SourceScenario::kGmailOnly:
        sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kGmail));
        break;
      case SourceScenario::kMixed:
        sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kPhotos));
        sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kGmail));
        break;
    }
    return sources;
  }
};

TEST_P(AtMemoryManagerIconTest,
       TransformsResultsIntoSuggestionsWithCorrectIcons) {
  SeeFormAndShowPopup();

  struct TestCase {
    MemoryDataType type;
    Suggestion::Icon regular_icon;
    Suggestion::Icon sparkly_icon;
  };
  const std::vector<TestCase> test_cases = {
      {MemoryDataType::kAddressFull, Suggestion::Icon::kLocation,
       Suggestion::Icon::kLocationSpark},
      {MemoryDataType::kVehiclePlateNumber, Suggestion::Icon::kVehicle,
       Suggestion::Icon::kVehicleSpark},
      {MemoryDataType::kPassportNumber, Suggestion::Icon::kPassport,
       Suggestion::Icon::kPassportSpark},
      {MemoryDataType::kFlightReservationFlightNumber,
       Suggestion::Icon::kFlight, Suggestion::Icon::kFlightSpark},
      {MemoryDataType::kDriversLicenseNumber, Suggestion::Icon::kIdCard,
       Suggestion::Icon::kIdCardSpark},
      {MemoryDataType::kKnownTravelerNumberNumber, Suggestion::Icon::kIdCard2,
       Suggestion::Icon::kIdCard2Spark},
      {MemoryDataType::kCreditCardNumber, Suggestion::Icon::kCardGenericVector,
       Suggestion::Icon::kCardGenericSpark},
      {MemoryDataType::kIban, Suggestion::Icon::kCardGenericVector,
       Suggestion::Icon::kCardGenericSpark},
      {MemoryDataType::kOrderId, Suggestion::Icon::kOrder,
       Suggestion::Icon::kOrderSpark},
      {MemoryDataType::kShipmentTrackingNumber, Suggestion::Icon::kShipment,
       Suggestion::Icon::kShipmentSpark},
      {MemoryDataType::kEmail, Suggestion::Icon::kLocation,
       Suggestion::Icon::kLocationSpark},
      {MemoryDataType::kUnknown, Suggestion::Icon::kNoIcon,
       Suggestion::Icon::kTextSpark},
  };

  std::vector<MemorySearchResult> entries =
      base::ToVector(test_cases, [&](const TestCase& test_case) {
        MemorySearchResult entry(test_case.type, u"label", u"value");
        entry.sources = GetSourcesForScenario(scenario());
        return entry;
      });

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kFinalResponseSuccess,
                                    std::move(entries), final_suggestions);

  manager().OnSearchSubmitted(u"query");

  EXPECT_EQ(final_suggestions.size(), test_cases.size());
  const bool expect_sparkly = (scenario() != SourceScenario::kAutofillOnly);
  for (auto [test_case, suggestion] :
       std::views::zip(test_cases, final_suggestions)) {
    Suggestion::Icon expected_icon =
        expect_sparkly ? test_case.sparkly_icon : test_case.regular_icon;
    EXPECT_EQ(suggestion.icon, expected_icon)
        << "For MemoryDataType: " << static_cast<int>(test_case.type)
        << " in scenario: " << static_cast<int>(scenario());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AtMemoryManagerIconTest,
                         Values(SourceScenario::kNoSources,
                                SourceScenario::kAutofillOnly,
                                SourceScenario::kGmailOnly,
                                SourceScenario::kMixed));

TEST_F(AtMemoryManagerTest, OnSearchSubmitted_PassesUrlAndTitleToQueryService) {
  SeeFormAndShowPopup();

  EXPECT_CALL(mock_query_service(),
              Query(std::u16string_view(u"test query"),
                    autofill_client().GetLastCommittedPrimaryMainFrameURL(),
                    autofill_client().GetPageTitle(), _));

  manager().OnSearchSubmitted(u"test query");
}

// Tests that receiving additional `OnPopupShown` events is safe even after
// starting a fill.
TEST_F(AtMemoryManagerTest, OnPopupShown_SubPopup_NoCrashWhenRecorderMovedOut) {
  // 1. Show root popup to initialize the recorder.
  auto [form_id, field_id] = SeeFormAndShowPopup();
  ASSERT_NE(test_api(manager()).at_memory_metrics_recorder(), nullptr);

  // 2. Fill a suggestion, which moves out at_memory_metrics_recorder_.
  Suggestion suggestion(u"test", SuggestionType::kAtMemorySearchResult);
  Suggestion::AtMemoryPayload payload;
  payload.memory_data_type = MemoryDataType::kIban;
  payload.identifier = Iban::Guid("guid");
  suggestion.payload = std::move(payload);

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form_id,
                                      field_id, suggestion);
  EXPECT_EQ(test_api(manager()).at_memory_metrics_recorder(), nullptr);

  // 3. Hovering/showing a sub-popup after recorder was moved out should NOT
  // crash.
  manager().OnPopupShown(
      form_id, field_id,
      AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {2}},
      /*is_context_secure=*/true, update_callback_.Get(),
      ukm::kInvalidSourceId);
  EXPECT_EQ(test_api(manager()).at_memory_metrics_recorder(), nullptr);
}

}  // namespace

}  // namespace autofill
