// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/types/zip.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/at_memory_utils.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/strings/grit/components_strings.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::accessibility_annotator::MemoryDataType;
using ::accessibility_annotator::MemoryEntrySource;
using ::accessibility_annotator::MemoryEntrySourceType;
using ::accessibility_annotator::MemorySearchResult;
using ::accessibility_annotator::MemorySearchResults;
using ::accessibility_annotator::MemorySearchStatus;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;

constexpr size_t kVisibleSuffixLength = 4;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void,
              ShowAutofillAiFetchFromWalletFailureNotification,
              (),
              (override));
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
  void SetUp() override {
    InitAutofillClient();
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

 protected:
  AtMemoryManager& manager() { return autofill_manager().GetAtMemoryManager(); }

  MockAtMemoryQueryService& mock_query_service() {
    return *mock_query_service_ptr_;
  }

  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

  void AddOrUpdateEntityInstance(const EntityInstance& entity) {
    autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
    webdata_helper().WaitUntilIdle();
  }

  void MockQueryResultsAndExpectCallback(
      std::u16string_view query,
      MemorySearchStatus status,
      std::vector<MemorySearchResult> entries,
      std::vector<Suggestion>& final_suggestions) {
    EXPECT_CALL(mock_query_service(), Query(query, _))
        .WillOnce([status, entries = std::move(entries)](
                      std::u16string_view query,
                      base::RepeatingCallback<void(MemorySearchResults)>
                          callback) mutable {
          callback.Run(MemorySearchResults(status, std::move(entries)));
        });
    InSequence s;
    EXPECT_CALL(update_callback_,
                Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemory));
    EXPECT_CALL(update_callback_,
                Run(_, AutofillSuggestionTriggerSource::kAtMemory))
        .WillOnce(SaveArg<0>(&final_suggestions));
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockAtMemoryQueryService> mock_query_service_ptr_ = nullptr;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_;
};

Matcher<Suggestion> EqualsAtMemorySuggestion(
    MemoryDataType memory_data_type,
    Matcher<std::vector<Suggestion>> children_matcher = IsEmpty()) {
  return AllOf(
      Field(&Suggestion::type, SuggestionType::kAtMemorySearchResult),
      ResultOf(
          [](const Suggestion& s) {
            return s.GetPayload<Suggestion::AtMemoryPayload>().memory_data_type;
          },
          memory_data_type),
      Field(&Suggestion::children, children_matcher));
}

// Tests that OnFilterChanged with a non-empty filter generates the search
// affordance suggestion and does NOT trigger QueryService::Query.
TEST_F(AtMemoryManagerTest, OnFilterChanged_GeneratesSearchAffordance) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  EXPECT_CALL(mock_query_service(), Query).Times(0);

  // Expect that OnFilterChanged triggers the callback with the single
  // affordance suggestion.
  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"query");

  ASSERT_EQ(suggestions.size(), 1u);
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

// Tests that OnFilterChanged with an empty filter clears all suggestions.
TEST_F(AtMemoryManagerTest, OnFilterChanged_EmptyFilterClearsSuggestions) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  EXPECT_CALL(update_callback_,
              Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemory));

  manager().OnFilterChanged(u"");
}

// Tests that OnSearchSubmitted triggers full search, clears currently shown
// suggestions, and successfully updates suggestions with the results once they
// arrive.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_TriggersQueryServiceAndClearsSuggestions) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  // Expect that executing the query immediately clears suggestions.
  EXPECT_CALL(update_callback_,
              Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemory));

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
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&final_suggestions));

  search_callback.Run(std::move(results));

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"Full Address");
}

// Tests that when a search result has an empty type name and no metadata, the
// generated suggestion has no labels.
TEST_F(AtMemoryManagerTest, OnSearchSubmitted_SchemalessResultHasEmptyLabels) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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

// Tests that when the user is offline, the manager displays the no connection
// suggestion.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_QueryServiceReturnsNoConnectionFailure) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(u"query",
                                    MemorySearchStatus::kNoConnectionFailure,
                                    /*entries=*/{}, final_suggestions);

  manager().OnSearchSubmitted(u"query");

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemoryNoConnection);
  EXPECT_EQ(final_suggestions[0].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_NO_CONNECTION));
}

// Tests that when filling an attribute (e.g. Passport Number), the manager
// fetches the unmasked entity instance from AutofillAiAccessManager and fills
// the unmasked attribute value correctly.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_AttributeSuccess) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];

  base::optional_ref<const AttributeInstance> passport_attribute =
      passport.attribute(AttributeType(AttributeTypeName::kPassportNumber));
  ASSERT_TRUE(passport_attribute.has_value());

  EXPECT_CALL(*mock_ai_access_manager_ptr,
              FetchEntityInstance(Eq(passport), true, _))
      .WillOnce([&](EntityInstance entity, bool will_fill,
                    AutofillAiAccessManager::OnEntityInstanceFetchedCallback
                        callback) {
        std::move(callback).Run(entity, /*reauth_attempted=*/false);
        return true;
      });

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         passport_attribute->GetCompleteRawInfo(),
                         FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      form.global_id(), field.global_id(),
                                      final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
}

// Tests that when filling a full entity (e.g. Passport Full), the manager
// fetches the unmasked entity instance from AutofillAiAccessManager and fills
// the primary entity value correctly.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_EntitySuccess) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kPassportFull, u"Passport",
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

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];

  EXPECT_CALL(*mock_ai_access_manager_ptr,
              FetchEntityInstance(Eq(passport), true, _))
      .WillOnce([&](EntityInstance entity, bool will_fill,
                    AutofillAiAccessManager::OnEntityInstanceFetchedCallback
                        callback) {
        std::move(callback).Run(entity, /*reauth_attempted=*/false);
        return true;
      });

  std::optional<AttributeType> expected_primary_attribute_type =
      GetPrimaryAttributeType(passport);
  ASSERT_TRUE(expected_primary_attribute_type.has_value());
  base::optional_ref<const AttributeInstance> expected_primary_attribute =
      passport.attribute(*expected_primary_attribute_type);
  ASSERT_TRUE(expected_primary_attribute.has_value());
  std::u16string expected_primary_value =
      expected_primary_attribute->GetCompleteInfo(
          autofill_client().GetAppLocale());
  ASSERT_FALSE(expected_primary_value.empty());

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         expected_primary_value, FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      form.global_id(), field.global_id(),
                                      final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", 1);
}

// Tests that when fetching the unmasked entity instance fails, the manager
// triggers the fetch failure notification and does not fill any value.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_FetchFailed) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];

  EXPECT_CALL(*mock_ai_access_manager_ptr,
              FetchEntityInstance(Eq(passport), true, _))
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
              ShowAutofillAiFetchFromWalletFailureNotification());
  EXPECT_CALL(autofill_manager(), FillOrPreviewField).Times(0);

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      form.global_id(), field.global_id(),
                                      final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      false, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", 0);
}

// Tests that SPII entries and metadata are filtered out from the search
// results when the context is insecure.
TEST_F(AtMemoryManagerTest, FiltersSpiiInInsecureContext) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/false, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
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
      ElementsAre(EqualsAtMemorySuggestion(MemoryDataType::kAddressFull),
                  EqualsAtMemorySuggestion(MemoryDataType::kPhone,
                                           ElementsAre(EqualsAtMemorySuggestion(
                                               MemoryDataType::kPhone)))));
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

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(RunOnceCallback<1>(std::move(results)));

  InSequence s;
  // Executing the query immediately clears existing suggestions before
  // returning search results.
  EXPECT_CALL(update_callback_,
              Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemory));
  EXPECT_CALL(
      update_callback_,
      Run(ElementsAre(EqualsAtMemorySuggestion(MemoryDataType::kAddressFull),
                      EqualsAtMemorySuggestion(
                          MemoryDataType::kDriversLicenseName,
                          ElementsAre(EqualsAtMemorySuggestion(
                              MemoryDataType::kDriversLicenseState)))),
          AutofillSuggestionTriggerSource::kAtMemory));

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

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(RunOnceCallback<1>(std::move(results)));

  InSequence s;
  // Executing the query immediately clears existing suggestions before
  // returning search results.
  EXPECT_CALL(update_callback_,
              Run(IsEmpty(), AutofillSuggestionTriggerSource::kAtMemory));
  EXPECT_CALL(update_callback_,
              Run(ElementsAre(EqualsAtMemorySuggestion(MemoryDataType::kIban)),
                  AutofillSuggestionTriggerSource::kAtMemory));

  manager().OnSearchSubmitted(u"query");
}

// Tests that SPII entries and metadata are retained in the search results
// when the context is secure.
TEST_F(AtMemoryManagerTest, KeepsSpiiInSecureContext) {
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  base::RepeatingCallback<void(MemorySearchResults)> search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
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
          EqualsAtMemorySuggestion(MemoryDataType::kAddressFull),
          EqualsAtMemorySuggestion(MemoryDataType::kPassportNumber),
          EqualsAtMemorySuggestion(
              MemoryDataType::kPhone,
              ElementsAre(
                  EqualsAtMemorySuggestion(MemoryDataType::kPhone),
                  EqualsAtMemorySuggestion(MemoryDataType::kPassportNumber)))));
}

// Tests that non-SPII data fills correctly and records the funnel metrics.
TEST_F(AtMemoryManagerTest, FillNonSensitiveData_Success) {
  base::HistogramTester histogram_tester;
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  std::vector<Suggestion> final_suggestions;
  {
    MemorySearchResult entry(MemoryDataType::kNameFull, u"Name", u"John Doe");
    MockQueryResultsAndExpectCallback(u"query",
                                      MemorySearchStatus::kFinalResponseSuccess,
                                      {entry}, final_suggestions);
  }
  manager().OnSearchSubmitted(u"query");
  ASSERT_EQ(final_suggestions.size(), 1u);

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];

  std::u16string expected_value = u"John Doe";
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         expected_value, FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      form.global_id(), field.global_id(),
                                      final_suggestions[0]);

  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                      true, 1);
}

// Tests that funnel metrics are recorded correctly even if multiple are shown.
TEST_F(AtMemoryManagerTest, FillOverlappingPopups) {
  base::HistogramTester histogram_tester;

  // 1. Show Popup 1.
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];

  MockIbanAccessManager* mock_iban_access_manager =
      autofill_client().GetPaymentsAutofillClient()->GetIbanAccessManager();

  base::OnceCallback<void(const std::u16string& value)> fetch_callback;
  EXPECT_CALL(*mock_iban_access_manager, FetchValue)
      .WillOnce(
          [&](const Suggestion::Payload& payload,
              base::OnceCallback<void(const std::u16string& value)> callback) {
            fetch_callback = std::move(callback);
          });

  // 2. Accept async suggestion on Popup 1.
  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      form.global_id(), field.global_id(),
                                      final_suggestions[0]);

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
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", 0);

  // 4. Show Popup 2 (overlapping with the pending async fill of Popup 1).
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_2;
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
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
  std::u16string unmasked_iban = u"ES12345678901234567890";
  std::move(fetch_callback).Run(unmasked_iban);

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
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", 1);
}

// Tests that the personal context notice is appended when the user needs to see
// the notice.
TEST_F(AtMemoryManagerTest, PersonalContext_AppendsNoticeSuggestion) {
  autofill_client().set_should_show_personal_context_at_memory_notice(true);

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(suggestions.empty());
#else
  ASSERT_EQ(1u, suggestions.size());
  EXPECT_EQ(SuggestionType::kPersonalContextNotice, suggestions[0].type);
  EXPECT_EQ(Suggestion::FiltrationPolicy::kStatic,
            suggestions[0].filtration_policy);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

// Tests that the personal context notice is not appended when the user does not
// need to see the notice.
TEST_F(AtMemoryManagerTest, PersonalContext_DoesNotAppendNoticeSuggestion) {
  autofill_client().set_should_show_personal_context_at_memory_notice(false);

  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback_,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
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
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

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
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         FormSignature(0), FieldSignature(0));
  // Create an entry where the primary value is sensitive and metadata is
  // non-sensitive.
  MemorySearchResult entry(MemoryDataType::kPassportNumber, u"Passport Number",
                           u"987654321");
  entry.metadata_list.emplace_back(MemoryDataType::kNameFull, u"Name",
                                   u"John Doe");
  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(
      u"query",
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
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
                                      FormGlobalId(), FieldGlobalId(),
                                      final_suggestions[0]);

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(
                  mojom::ActionPersistence::kFill,
                  mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                  std::u16string(u"987654321"), FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      FormGlobalId(), FieldGlobalId(),
                                      final_suggestions[0]);
}

// Tests that sensitive metadata is obfuscated in the primary suggestion labels
// and in the child flyout menu, while keeping the raw value in its payload.
// Also verifies that previewing the child suggestion uses the obfuscated value,
// while filling uses the raw value directly.
TEST_F(AtMemoryManagerTest, RemoteSensitiveMetadata_Obfuscated) {
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         FormSignature(0), FieldSignature(0));
  // Create an entry where the primary value is non-sensitive and metadata is
  // sensitive.
  MemorySearchResult entry(MemoryDataType::kNameFull, u"Name", u"John Doe");
  entry.metadata_list.emplace_back(MemoryDataType::kPassportNumber,
                                   u"Passport Number", u"987654321");
  std::vector<Suggestion> final_suggestions;
  MockQueryResultsAndExpectCallback(
      u"query",
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
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
  ASSERT_EQ(final_suggestions[0].children.size(), 1u);
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
                                      FormGlobalId(), FieldGlobalId(),
                                      final_suggestions[0].children[0]);

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(
                  mojom::ActionPersistence::kFill,
                  mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                  std::u16string(u"987654321"), FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill,
                                      FormGlobalId(), FieldGlobalId(),
                                      final_suggestions[0].children[0]);
}

TEST_F(AtMemoryManagerTest, OnPopupShown_SubPopup_DoesNotResetRecorder) {
  base::HistogramTester histogram_tester;

  // 1. Show root popup. This should initialize the metrics recorder.
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  // 2. Show sub-popup. This should NOT reset the recorder.
  AutofillSuggestionDelegate::SuggestionMetadata metadata;
  metadata.multi_index = {0, 0};  // sub-popup
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory, metadata,
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
  manager().OnPopupShown(FormGlobalId(), FieldGlobalId(),
                         AutofillSuggestionTriggerSource::kAtMemory,
                         std::nullopt,
                         /*is_context_secure=*/true, update_callback_.Get(),
                         ukm::kInvalidSourceId);

  struct TestCase {
    MemoryDataType type;
    Suggestion::Icon regular_icon;
    Suggestion::Icon sparkly_icon;
  };
  const std::vector<TestCase> test_cases = {
      {MemoryDataType::kAddressFull, Suggestion::Icon::kLocation,
       Suggestion::Icon::kLocationSpark},
      {MemoryDataType::kVehicle, Suggestion::Icon::kVehicle,
       Suggestion::Icon::kVehicleSpark},
      {MemoryDataType::kPassportFull, Suggestion::Icon::kPassport,
       Suggestion::Icon::kPassportSpark},
      {MemoryDataType::kFlightReservationFull, Suggestion::Icon::kFlight,
       Suggestion::Icon::kFlightSpark},
      {MemoryDataType::kDriversLicenseFull, Suggestion::Icon::kIdCard,
       Suggestion::Icon::kIdCardSpark},
      {MemoryDataType::kKnownTravelerNumberFull, Suggestion::Icon::kIdCard2,
       Suggestion::Icon::kIdCard2Spark},
      {MemoryDataType::kCreditCardNumber, Suggestion::Icon::kCardGenericVector,
       Suggestion::Icon::kCardGenericSpark},
      {MemoryDataType::kIban, Suggestion::Icon::kCardGenericVector,
       Suggestion::Icon::kCardGenericSpark},
      {MemoryDataType::kOrderFull, Suggestion::Icon::kOrder,
       Suggestion::Icon::kOrderSpark},
      {MemoryDataType::kShipmentFull, Suggestion::Icon::kShipment,
       Suggestion::Icon::kShipmentSpark},
      {MemoryDataType::kEmail, Suggestion::Icon::kNoIcon,
       Suggestion::Icon::kTextSpark},
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
       base::zip(test_cases, final_suggestions)) {
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

}  // namespace

}  // namespace autofill
