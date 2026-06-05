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
#include "components/accessibility_annotator/core/mock_accessibility_query_service.h"
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
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::ResultOf;
using ::testing::Property;
using ::testing::SaveArg;

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
               const FormData& form,
               const FormFieldData& field,
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

class AtMemoryManagerTest : public testing::Test,
                            public WithTestAutofillClientDriverManager<
                                NiceMock<MockAutofillClient>,
                                TestAutofillDriver,
                                NiceMock<MockBrowserAutofillManager>> {
 public:
  void SetUp() override {
    InitAutofillClient();
    auto mock_query_service = std::make_unique<testing::NiceMock<
        accessibility_annotator::MockAccessibilityQueryService>>();
    mock_query_service_ptr_ = mock_query_service.get();
    autofill_client().set_accessibility_query_service(
        std::move(mock_query_service));

    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
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

  accessibility_annotator::MockAccessibilityQueryService& mock_query_service() {
    return *mock_query_service_ptr_;
  }

  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

  void AddOrUpdateEntityInstance(const EntityInstance& entity) {
    autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(entity);
    webdata_helper().WaitUntilIdle();
  }

  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<accessibility_annotator::MockAccessibilityQueryService>
      mock_query_service_ptr_ = nullptr;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
};

Matcher<Suggestion> EqualsAtMemorySuggestion(
    accessibility_annotator::EntryType entry_type,
    Matcher<std::vector<Suggestion>> children_matcher = IsEmpty()) {
  return AllOf(
      Field(&Suggestion::type, SuggestionType::kAtMemorySearchResult),
      ResultOf(
          [](const Suggestion& s) {
            return s.GetPayload<Suggestion::AtMemoryPayload>().entry_type;
          },
          entry_type),
      Field(&Suggestion::children, children_matcher));
}

// Tests that OnFilterChanged with a non-empty filter generates the search
// affordance suggestion and does NOT trigger QueryService::Query.
TEST_F(AtMemoryManagerTest, OnFilterChanged_GeneratesSearchAffordance) {
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  EXPECT_CALL(mock_query_service(), Query).Times(0);

  // Expect that OnFilterChanged triggers the callback with the single
  // affordance suggestion.
  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback,
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
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  EXPECT_CALL(update_callback, Run(testing::IsEmpty(),
                                   AutofillSuggestionTriggerSource::kAtMemory));

  manager().OnFilterChanged(u"");
}

// Tests that OnSearchSubmitted triggers full search, clears currently shown
// suggestions, and successfully updates suggestions with the results once they
// arrive.
TEST_F(AtMemoryManagerTest,
       OnSearchSubmitted_TriggersQueryServiceAndClearsSuggestions) {
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  base::RepeatingCallback<void(accessibility_annotator::MemorySearchResults)>
      search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  // Expect that executing the query immediately clears suggestions.
  EXPECT_CALL(update_callback, Run(testing::IsEmpty(),
                                   AutofillSuggestionTriggerSource::kAtMemory));

  manager().OnSearchSubmitted(u"query");

  // Simulate search results returning from the query service.
  std::vector<accessibility_annotator::MemorySearchResult> entries;
  entries.emplace_back(accessibility_annotator::EntryType::kAddressFull,
                       u"Address", u"Full Address");
  accessibility_annotator::MemorySearchResults results(
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
      std::move(entries));

  // Expect that when search results arrive, suggestions are updated.
  std::vector<Suggestion> final_suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&final_suggestions));

  search_callback.Run(std::move(results));

  ASSERT_EQ(final_suggestions.size(), 1u);
  EXPECT_EQ(final_suggestions[0].type, SuggestionType::kAtMemorySearchResult);
  EXPECT_EQ(final_suggestions[0].main_text.value, u"Full Address");
}

// Tests that when filling an attribute (e.g. Passport Number), the manager
// fetches the unmasked entity instance from AutofillAiAccessManager and fills
// the unmasked attribute value correctly.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_AttributeSuccess) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

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
  Suggestion suggestion(u"some result", SuggestionType::kAtMemorySearchResult);

  Suggestion::AtMemoryPayload at_memory_payload(
      u"some text", accessibility_annotator::EntryType::kPassportNumber);
  at_memory_payload.identifier = passport.guid();
  suggestion.payload = std::move(at_memory_payload);

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

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form,
                                      field, suggestion);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);
}

// Tests that when filling a full entity (e.g. Passport Full), the manager
// fetches the unmasked entity instance from AutofillAiAccessManager and fills
// the primary entity value correctly.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_EntitySuccess) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

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
  Suggestion suggestion(u"some result", SuggestionType::kAtMemorySearchResult);

  Suggestion::AtMemoryPayload at_memory_payload(
      u"some text", accessibility_annotator::EntryType::kPassportFull);
  at_memory_payload.identifier = passport.guid();
  suggestion.payload = std::move(at_memory_payload);

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

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form,
                                      field, suggestion);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);
}

// Tests that when fetching the unmasked entity instance fails, the manager
// triggers the fetch failure notification and does not fill any value.
TEST_F(AtMemoryManagerTest, FillSensitiveAutofillAiData_FetchFailed) {
  base::HistogramTester histogram_tester;
  EntityInstance passport = test::GetPassportEntityInstanceWithRandomGuid();
  AddOrUpdateEntityInstance(passport);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

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
  Suggestion suggestion(u"some result", SuggestionType::kAtMemorySearchResult);

  Suggestion::AtMemoryPayload at_memory_payload(
      u"some text", accessibility_annotator::EntryType::kPassportNumber);
  at_memory_payload.identifier = passport.guid();
  suggestion.payload = std::move(at_memory_payload);

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
  EXPECT_CALL(autofill_manager(), FillOrPreviewField(_, _, _, _, _, _, _))
      .Times(0);

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form,
                                      field, suggestion);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", false, 1);
}

// Tests that SPII entries and metadata are filtered out from the search
// results when the context is insecure.
TEST_F(AtMemoryManagerTest, FiltersSpiiInInsecureContext) {
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/false, update_callback.Get());

  base::RepeatingCallback<void(accessibility_annotator::MemorySearchResults)>
      search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<accessibility_annotator::MemorySearchResult> entries;
  // Non-SPII entry.
  entries.emplace_back(accessibility_annotator::EntryType::kAddressFull,
                       u"Address", u"Full Address");
  // SPII entry.
  entries.emplace_back(accessibility_annotator::EntryType::kPassportNumber,
                       u"IBAN", u"1234");

  // Non-SPII entry with mixed metadata.
  accessibility_annotator::MemorySearchResult mixed_entry(
      accessibility_annotator::EntryType::kPhone, u"Phone", u"123");
  mixed_entry.metadata_list.emplace_back(
      accessibility_annotator::EntryType::kPhone, u"Phone meta", u"123");
  mixed_entry.metadata_list.emplace_back(
      accessibility_annotator::EntryType::kPassportNumber, u"IBAN meta",
      u"1234");
  entries.push_back(std::move(mixed_entry));

  accessibility_annotator::MemorySearchResults results(
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
      std::move(entries));

  search_callback.Run(std::move(results));

  EXPECT_THAT(
      resulting_suggestions,
      ElementsAre(EqualsAtMemorySuggestion(
                      accessibility_annotator::EntryType::kAddressFull),
                  EqualsAtMemorySuggestion(
                      accessibility_annotator::EntryType::kPhone,
                      ElementsAre(EqualsAtMemorySuggestion(
                          accessibility_annotator::EntryType::kPhone)))));
}

// Tests that SPII entries and metadata are retained in the search results
// when the context is secure.
TEST_F(AtMemoryManagerTest, KeepsSpiiInSecureContext) {
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  base::RepeatingCallback<void(accessibility_annotator::MemorySearchResults)>
      search_callback;
  EXPECT_CALL(mock_query_service(), Query(std::u16string_view(u"query"), _))
      .WillOnce(SaveArg<1>(&search_callback));

  std::vector<Suggestion> resulting_suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillRepeatedly(SaveArg<0>(&resulting_suggestions));

  manager().OnSearchSubmitted(u"query");

  std::vector<accessibility_annotator::MemorySearchResult> entries;
  // Non-SPII entry.
  entries.emplace_back(accessibility_annotator::EntryType::kAddressFull,
                       u"Address", u"Full Address");
  // SPII entry.
  entries.emplace_back(accessibility_annotator::EntryType::kPassportNumber,
                       u"IBAN", u"1234");

  // Non-SPII entry with mixed metadata.
  accessibility_annotator::MemorySearchResult mixed_entry(
      accessibility_annotator::EntryType::kPhone, u"Phone", u"123");
  mixed_entry.metadata_list.emplace_back(
      accessibility_annotator::EntryType::kPhone, u"Phone meta", u"123");
  mixed_entry.metadata_list.emplace_back(
      accessibility_annotator::EntryType::kPassportNumber, u"IBAN meta",
      u"1234");
  entries.push_back(std::move(mixed_entry));

  accessibility_annotator::MemorySearchResults results(
      accessibility_annotator::MemorySearchStatus::kFinalResponseSuccess,
      std::move(entries));

  search_callback.Run(std::move(results));

  EXPECT_THAT(
      resulting_suggestions,
      ElementsAre(
          EqualsAtMemorySuggestion(
              accessibility_annotator::EntryType::kAddressFull),
          EqualsAtMemorySuggestion(
              accessibility_annotator::EntryType::kPassportNumber),
          EqualsAtMemorySuggestion(
              accessibility_annotator::EntryType::kPhone,
              ElementsAre(
                  EqualsAtMemorySuggestion(
                      accessibility_annotator::EntryType::kPhone),
                  EqualsAtMemorySuggestion(
                      accessibility_annotator::EntryType::kPassportNumber)))));
}

// Tests that non-SPII data fills correctly and records the funnel metrics.
TEST_F(AtMemoryManagerTest, FillNonSensitiveData_Success) {
  base::HistogramTester histogram_tester;
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];
  Suggestion suggestion(u"some result", SuggestionType::kAtMemorySearchResult);

  Suggestion::AtMemoryPayload at_memory_payload(
      u"John Doe", accessibility_annotator::EntryType::kNameFull);
  suggestion.payload = std::move(at_memory_payload);

  std::u16string expected_value = u"John Doe";
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAtMemoryTrigger, _, _,
                         expected_value, FillingProduct::kAtMemory, _));

  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form,
                                      field, suggestion);

  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);
}

// Tests that funnel metrics are recorded correctly even if multiple are shown.
TEST_F(AtMemoryManagerTest, FillOverlappingPopups) {
  base::HistogramTester histogram_tester;

  // 1. Show Popup 1.
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_1;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback_1.Get());

  FormData form = test::CreateTestAddressFormData();
  std::vector<FieldType> field_types(form.fields().size(), UNKNOWN_TYPE);
  autofill_manager().AddSeenForm(form, field_types);
  FormFieldData field = form.fields()[0];
  Suggestion suggestion(u"some result", SuggestionType::kAtMemorySearchResult);

  Suggestion::AtMemoryPayload at_memory_payload(
      u"some text", accessibility_annotator::EntryType::kIban);
  at_memory_payload.identifier =
      Iban::Guid("12345678-1234-1234-1234-123456789012");
  suggestion.payload = std::move(at_memory_payload);

  MockIbanAccessManager* mock_iban_access_manager =
      autofill_client().GetPaymentsAutofillClient()->GetIbanAccessManager();

  base::OnceCallback<void(const std::u16string& value)> fetch_callback;
  EXPECT_CALL(*mock_iban_access_manager, FetchValue(_, _))
      .WillOnce(
          [&](const Suggestion::Payload& payload,
              base::OnceCallback<void(const std::u16string& value)> callback) {
            fetch_callback = std::move(callback);
          });

  // 2. Accept async suggestion on Popup 1.
  manager().FillOrPreviewSearchResult(mojom::ActionPersistence::kFill, form,
                                      field, suggestion);

  // 3. Hide Popup 1.
  manager().OnPopupHidden();

  // At this stage:
  // - Popup 1's displayed is logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.Funnel.PopupDisplayed"),
      BucketsAre(
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1)));
  // - QuerySubmitted, SuggestionAccepted, SuggestionFilled are not logged yet
  // because Popup 1's async fill is still pending.
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Funnel.QuerySubmitted",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", 0);
  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Funnel.SuggestionFilled",
                                    0);

  // 4. Show Popup 2 (overlapping with the pending async fill of Popup 1).
  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback_2;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                         /*is_context_secure=*/true, update_callback_2.Get());

  // 5. Hide Popup 2 (without accepting suggestions).
  manager().OnPopupHidden();

  // Verify Popup 2 logged its displayed, query submitted, suggestion accepted:
  // - PopupDisplayed should have context menu trigger as well now.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.Funnel.PopupDisplayed"),
      BucketsAre(
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1),
          Bucket(AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1)));
  // - QuerySubmitted should have one sample (false, from Popup 2).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.Funnel.QuerySubmitted"),
      BucketsAre(Bucket(false, 1)));
  // - SuggestionAccepted should have one sample (false, from Popup 2).
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AtMemory.Funnel.SuggestionAccepted"),
              BucketsAre(Bucket(false, 1)));

  // 6. Complete the async fill for Popup 1.
  std::u16string unmasked_iban = u"ES12345678901234567890";
  std::move(fetch_callback).Run(unmasked_iban);

  // Now, Popup 1's metrics should also be logged:
  // - QuerySubmitted should have two samples (both false).
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.AtMemory.Funnel.QuerySubmitted"),
      BucketsAre(Bucket(false, 2)));
  // - SuggestionAccepted should have one true (from Popup 1) and one false
  // (from Popup 2).
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AtMemory.Funnel.SuggestionAccepted"),
              BucketsAre(Bucket(false, 1), Bucket(true, 1)));
  // - SuggestionFilled should be logged as true.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.AtMemory.Funnel.SuggestionFilled"),
              BucketsAre(Bucket(true, 1)));
}

// Tests that the personal context notice is appended when the feature is
// enabled and the user needs to see the notice.
TEST_F(AtMemoryManagerTest, PersonalContextEnabled_AppendsNoticeSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContextFirstRunNoticePhase2);

  autofill_client().set_should_show_personal_context_autofill_notice(true);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

  ASSERT_EQ(1u, suggestions.size());
  EXPECT_EQ(SuggestionType::kPersonalContextNotice, suggestions[0].type);
  EXPECT_EQ(Suggestion::FiltrationPolicy::kStatic,
            suggestions[0].filtration_policy);
}

// Tests that the personal context notice is not appended when the feature is
// enabled but the user does not need to see the notice.
TEST_F(AtMemoryManagerTest,
       PersonalContextEnabled_DoesNotAppendNoticeSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      personal_context::features::kPersonalContextFirstRunNoticePhase2);

  autofill_client().set_should_show_personal_context_autofill_notice(false);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

  EXPECT_TRUE(suggestions.empty());
}

// Tests that the personal context notice is not appended when the feature is
// disabled.
TEST_F(AtMemoryManagerTest,
       PersonalContextDisabled_DoesNotAppendNoticeSuggestion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      personal_context::features::kPersonalContextFirstRunNoticePhase2);

  base::MockCallback<AtMemoryManager::UpdateSuggestionsCallback>
      update_callback;
  manager().OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory,
                         /*is_context_secure=*/true, update_callback.Get());

  std::vector<Suggestion> suggestions;
  EXPECT_CALL(update_callback,
              Run(_, AutofillSuggestionTriggerSource::kAtMemory))
      .WillOnce(SaveArg<0>(&suggestions));

  manager().OnFilterChanged(u"");

  EXPECT_TRUE(suggestions.empty());
}

}  // namespace

}  // namespace autofill
