// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/iban_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::Field;
using testing::Truly;
using testing::UnorderedElementsAre;

namespace autofill {

constexpr char kNickname_0[] = "Nickname 0";
constexpr char kNickname_1[] = "Nickname 1";

namespace {

class MockSuggestionsHandler : public IBANManager::SuggestionsHandler {
 public:
  MockSuggestionsHandler() = default;
  MockSuggestionsHandler(const MockSuggestionsHandler&) = delete;
  MockSuggestionsHandler& operator=(const MockSuggestionsHandler&) = delete;
  ~MockSuggestionsHandler() override = default;

  MOCK_METHOD(void,
              OnSuggestionsReturned,
              (FieldGlobalId field_id,
               AutoselectFirstSuggestion autoselect_first_suggestion,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};

}  // namespace

class IBANManagerTest : public testing::Test {
 protected:
  IBANManagerTest() : iban_manager_(&personal_data_manager_) {}

  void SetUp() override {
    personal_data_manager_.SetAutofillCreditCardEnabled(true);
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
        .WillByDefault(testing::Return(gfx::test::CreateImage()));

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client_.GetAutofillOptimizationGuide()),
            ShouldBlockSingleFieldSuggestions)
        .WillByDefault(testing::Return(false));
  }

  void TearDown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
  }

  // Sets up the TestPersonalDataManager with an IBAN.
  IBAN SetUpIBAN(base::StringPiece value, base::StringPiece nickname) {
    IBAN iban;
    std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    iban.set_guid(guid);
    iban.set_value(base::UTF8ToUTF16(std::string(value)));
    iban.set_nickname(base::UTF8ToUTF16(std::string(nickname)));
    personal_data_manager_.AddIBANForTest(std::make_unique<IBAN>(iban));
    return iban;
  }

  // Adds an IBAN focused field to the suggestions context.
  SuggestionsContext GetIbanFocusedSuggestionsContext(
      AutofillField& autofill_field,
      autofill::ServerFieldType type = IBAN_VALUE) {
    SuggestionsContext context;
    autofill_field.SetTypeTo(AutofillType(type));
    context.focused_field = &autofill_field;
    FormData form_data;
    test::CreateTestIbanFormData(&form_data);
    form_structure_ = std::make_unique<FormStructure>(form_data);
    context.form_structure = form_structure_.get();
    return context;
  }

  // Sets up the TestPersonalDataManager with an IBAN and corresponding
  // suggestion.
  Suggestion SetUpIBANAndSuggestion(base::StringPiece value,
                                    base::StringPiece nickname) {
    IBAN iban = SetUpIBAN(value, nickname);
    Suggestion iban_suggestion(iban.GetIdentifierStringForAutofillDisplay());
    iban_suggestion.frontend_id = POPUP_ITEM_ID_IBAN_ENTRY;
    return iban_suggestion;
  }

  Suggestion SetUpSeparator() {
    Suggestion separator;
    separator.frontend_id = POPUP_ITEM_ID_SEPARATOR;
    return separator;
  }

  Suggestion SetUpFooterManagePaymentMethods() {
    Suggestion footer_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
    footer_suggestion.frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
    footer_suggestion.icon = "settingsIcon";
    return footer_suggestion;
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  testing::NiceMock<MockSuggestionsHandler> suggestions_handler_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<FormStructure> form_structure_;
  IBANManager iban_manager_;
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

MATCHER_P(MatchesTextAndFrontendId, suggestion, "") {
  return arg.main_text == suggestion.main_text &&
         arg.frontend_id == suggestion.frontend_id;
}

TEST_F(IBANManagerTest, ShowsIBANSuggestions) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(test::kIbanValue, kNickname_0);
  Suggestion iban_suggestion_1 =
      SetUpIBANAndSuggestion(test::kIbanValue_1, kNickname_1);

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  EXPECT_CALL(
      suggestions_handler_,
      OnSuggestionsReturned(
          test_field.global_id(), AutoselectFirstSuggestion(false),
          testing::IsSupersetOf({MatchesTextAndFrontendId(iban_suggestion_0),
                                 MatchesTextAndFrontendId(iban_suggestion_1)})))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, PaymentsAutofillEnabledPrefOff_NoIbanSuggestionsShown) {
  personal_data_manager_.SetAutofillCreditCardEnabled(false);
  SetUpIBANAndSuggestion(test::kIbanValue, kNickname_0);
  SetUpIBANAndSuggestion(test::kIbanValue_1, kNickname_1);

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // Because the "Save and autofill payment methods" toggle is off, the
  // suggestion handler should not be triggered.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, IBANSuggestions_SeparatorAndFooter) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(test::kIbanValue, kNickname_0);
  Suggestion iban_suggestion_1 = SetUpSeparator();
  Suggestion iban_suggestion_2 = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned IBAN-based
  // suggestions. A separator and the "Manage payment methods..." row should
  // also be returned.
  EXPECT_CALL(suggestions_handler_,
              OnSuggestionsReturned(
                  test_field.global_id(), AutoselectFirstSuggestion(false),
                  testing::UnorderedElementsAre(
                      MatchesTextAndFrontendId(iban_suggestion_0),
                      MatchesTextAndFrontendId(iban_suggestion_1),
                      MatchesTextAndFrontendId(iban_suggestion_2))))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, ShowsIBANSuggestions_NoSuggestion) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(test::kIbanValue, kNickname_0);
  Suggestion iban_suggestion_1 =
      SetUpIBANAndSuggestion(test::kIbanValue_1, kNickname_1);

  AutofillField test_field;
  test_field.value = base::UTF8ToUTF16(std::string(test::kIbanValue));
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Verify that `OnSuggestionsReturned` handler is not triggered any IBAN-based
  // suggestions as the field already contains an IBAN.
  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, ShowsIBANSuggestions_OnlyPrefixMatch) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(test::kIbanValue_1, kNickname_0);
  Suggestion iban_suggestion_1 =
      SetUpIBANAndSuggestion(test::kIbanValue_2, kNickname_1);
  Suggestion iban_suggestion_2 = SetUpSeparator();
  Suggestion iban_suggestion_3 = SetUpFooterManagePaymentMethods();

  AutofillField test_field;
  test_field.value = u"CH";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions whose prefixes match `prefix_`. Both values should
  // be returned because they both start with CH56. Other than that, there are
  // one separator and one footer suggestion displayed.
  EXPECT_CALL(suggestions_handler_,
              OnSuggestionsReturned(
                  test_field.global_id(), AutoselectFirstSuggestion(false),
                  testing::UnorderedElementsAre(
                      MatchesTextAndFrontendId(iban_suggestion_0),
                      MatchesTextAndFrontendId(iban_suggestion_1),
                      MatchesTextAndFrontendId(iban_suggestion_2),
                      MatchesTextAndFrontendId(iban_suggestion_3))))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));

  test_field.value = u"CH5604";

  // Setting up mock to verify that the handler is returned only one
  // IBAN-based suggestion whose prefix matches `prefix_`. Only one of the two
  // IBANs should stay because the other will be filtered out. Other than that,
  // there are one separator and one footer suggestion displayed.
  EXPECT_CALL(suggestions_handler_,
              OnSuggestionsReturned(
                  test_field.global_id(), AutoselectFirstSuggestion(false),
                  testing::UnorderedElementsAre(
                      MatchesTextAndFrontendId(iban_suggestion_0),
                      MatchesTextAndFrontendId(iban_suggestion_2),
                      MatchesTextAndFrontendId(iban_suggestion_3))))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));

  test_field.value = u"AB56";

  // Verify that the handler is not triggered because no IBAN suggestions match
  // the given prefix.
  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, DoesNotShowIBANsForBlockedWebsite) {
  SetUpIBAN(test::kIbanValue, kNickname_0);
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Setting up mock to verify that suggestions returning is not triggered if
  // the website is blocked.
  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client_.GetAutofillOptimizationGuide()),
          ShouldBlockSingleFieldSuggestions)
      .WillByDefault(testing::Return(true));

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

// Test that suggestions are returned on platforms that don't have an
// AutofillOptimizationGuide. Having no AutofillOptimizationGuide means that
// suggestions cannot and will not be blocked.
TEST_F(IBANManagerTest, ShowsIBANSuggestions_OptimizationGuideNotPresent) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(test::kIbanValue, kNickname_0);
  AutofillField test_field;
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Delete the AutofillOptimizationGuide.
  autofill_client_.ResetAutofillOptimizationGuide();

  // Setting up mock to verify that the handler is returned a list of
  // IBAN-based suggestions.
  EXPECT_CALL(
      suggestions_handler_,
      OnSuggestionsReturned(
          test_field.global_id(), AutoselectFirstSuggestion(false),
          testing::IsSupersetOf({MatchesTextAndFrontendId(iban_suggestion_0)})))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, NotIbanFieldFocused_NoSuggestionsShown) {
  SetUpIBAN(test::kIbanValue, kNickname_0);

  AutofillField test_field;
  test_field.value = base::UTF8ToUTF16(std::string(test::kIbanValue));
  // Set the field type to any type than "IBAN_VALUE".
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(
      test_field, CREDIT_CARD_VERIFICATION_CODE);

  // Setting up mock to verify that suggestions returning is not triggered if
  // we are not focused on an IBAN field.
  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

// Test that the metrics for IBAN-related suggestions shown and shown once are
// logged correctly.
TEST_F(IBANManagerTest, Metrics_SuggestionsShown) {
  base::HistogramTester histogram_tester;
  SetUpIBAN(test::kIbanValue, kNickname_0);

  AutofillField test_field;
  test_field.unique_renderer_id = test::MakeFieldRendererId();
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Simulate request for suggestions.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(), context));

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(), context));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 2),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              1)));
}

// Test that the metrics for IBAN-related suggestion selected and selected once
// are logged correctly.
TEST_F(IBANManagerTest, Metrics_SuggestionSelected) {
  base::HistogramTester histogram_tester;
  SetUpIBAN(test::kIbanValue, kNickname_0);
  SetUpIBAN(test::kIbanValue_1, kNickname_1);
  SetUpIBAN(test::kIbanValue_2, "");

  AutofillField test_field;
  test_field.unique_renderer_id = test::MakeFieldRendererId();
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  // Simulate request for suggestions and select one suggested IBAN.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(), context));
  iban_manager_.OnSingleFieldSuggestionSelected(u"", POPUP_ITEM_ID_IBAN_ENTRY);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelected, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelectedOnce, 1);

  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(), context));
  iban_manager_.OnSingleFieldSuggestionSelected(u"", POPUP_ITEM_ID_IBAN_ENTRY);

  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelected, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Iban.Suggestions",
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelectedOnce, 1);
}

TEST_F(IBANManagerTest, Metrics_NoSuggestionShown) {
  base::HistogramTester histogram_tester;
  SetUpIBAN(test::kIbanValue, kNickname_0);
  SetUpIBAN(test::kIbanValue_1, kNickname_1);
  AutofillField test_field;
  // Input a prefix that does not have any matching IBAN value so that no IBAN
  // suggestions will be shown.
  test_field.value = u"XY";
  SuggestionsContext context = GetIbanFocusedSuggestionsContext(test_field);

  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // The suggestion handler should be triggered as some IBANs are available.
  // However, no suggestions are returned due to the prefix match requirement.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Iban.Suggestions"),
      BucketsAre(
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown, 0),
          base::Bucket(
              autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce,
              0)));
}

}  // namespace autofill
