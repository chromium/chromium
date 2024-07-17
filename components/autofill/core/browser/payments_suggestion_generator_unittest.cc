// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_suggestion_generator.h"

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

using gfx::test::AreImagesEqual;

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::Matcher;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

Matcher<Suggestion> EqualLabels(
    const std::vector<std::vector<Suggestion::Text>>& suggestion_objects) {
  return Field(&Suggestion::labels, suggestion_objects);
}

Matcher<Suggestion> EqualLabels(
    const std::vector<std::vector<std::u16string>>& labels) {
  std::vector<std::vector<Suggestion::Text>> suggestion_objects;
  for (const auto& row : labels) {
    suggestion_objects.emplace_back();
    for (const auto& col : row) {
      suggestion_objects.back().emplace_back(col);
    }
  }
  return EqualLabels(suggestion_objects);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
Matcher<Suggestion> EqualsFieldByFieldFillingSuggestion(
    SuggestionType id,
    const std::u16string& main_text,
    FieldType field_by_field_filling_type_used,
    const Suggestion::Payload& payload,
    const std::vector<std::vector<Suggestion::Text>>& labels = {}) {
  return AllOf(
      Field(&Suggestion::type, id),
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true))),
      Field(&Suggestion::payload, payload),
      Field(&Suggestion::icon, Suggestion::Icon::kNoIcon),
      Field(&Suggestion::field_by_field_filling_type_used,
            std::optional(field_by_field_filling_type_used)),
      EqualLabels(labels));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

Matcher<Suggestion> EqualsIbanSuggestion(
    const std::u16string& text,
    const Suggestion::Payload& payload,
    const std::u16string& first_label_value) {
  return AllOf(Field(&Suggestion::type, SuggestionType::kIbanEntry),
               Field(&Suggestion::main_text,
                     Suggestion::Text(text, Suggestion::Text::IsPrimary(true))),
               Field(&Suggestion::payload, payload),
               EqualLabels(first_label_value.empty()
                               ? std::vector<std::vector<Suggestion::Text>>{}
                               : std::vector<std::vector<Suggestion::Text>>{
                                     {Suggestion::Text(first_label_value)}}));
}

#if !BUILDFLAG(IS_IOS)
Matcher<Suggestion> EqualsUndoAutofillSuggestion() {
  return EqualsSuggestion(SuggestionType::kUndoOrClear,
#if BUILDFLAG(IS_ANDROID)
                          base::i18n::ToUpper(l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_UNDO_MENU_ITEM)),
#else
                          l10n_util::GetStringUTF16(
                              IDS_AUTOFILL_UNDO_MENU_ITEM),
#endif
                          Suggestion::Icon::kUndo);
}
#endif

Matcher<Suggestion> EqualsManagePaymentsMethodsSuggestion(bool with_gpay_logo) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return EqualsSuggestion(
      SuggestionType::kManageCreditCard,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
      with_gpay_logo ? Suggestion::Icon::kGooglePay
                     : Suggestion::Icon::kSettings);
#else
  return AllOf(EqualsSuggestion(SuggestionType::kManageCreditCard,
                                l10n_util::GetStringUTF16(
                                    IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
                                Suggestion::Icon::kSettings),
               Field(&Suggestion::trailing_icon,
                     with_gpay_logo ? (ui::NativeTheme::GetInstanceForNativeUi()
                                               ->ShouldUseDarkColors()
                                           ? Suggestion::Icon::kGooglePayDark
                                           : Suggestion::Icon::kGooglePay)
                                    : Suggestion::Icon::kNoIcon));
#endif
}

// Checks that `arg` contains necessary credit card footer suggestions. `arg`
// has to be of type std::vector<Suggestion>.
MATCHER_P(ContainsCreditCardFooterSuggestions, with_gpay_logo, "") {
  EXPECT_GT(arg.size(), 2ul);
  EXPECT_THAT(arg[arg.size() - 2],
              EqualsSuggestion(SuggestionType::kSeparator));
  EXPECT_THAT(arg.back(),
              EqualsManagePaymentsMethodsSuggestion(with_gpay_logo));
  return true;
}

}  // namespace

// TODO(crbug.com/40176273): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class PaymentsSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    payments_data().SetPrefService(autofill_client_.GetPrefs());
    payments_data().SetSyncServiceForTest(&sync_service_);
    autofill_client_.set_autofill_offer_manager(
        std::make_unique<AutofillOfferManager>(
            autofill_client_.GetPersonalDataManager(),
            /*coupon_service_delegate=*/nullptr, /*shopping_service=*/nullptr));
  }

  void TearDown() override {
    if (did_set_up_image_resource_for_test_) {
      CleanUpIbanImageResources();
      did_set_up_image_resource_for_test_ = false;
    }
  }

  CreditCard CreateServerCard(
      const std::string& guid = "00000000-0000-0000-0000-000000000001",
      const std::string& server_id = "server_id1",
      int instrument_id = 1) {
    CreditCard server_card(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&server_card, "Elvis Presley", "1111" /* Visa */,
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", /*cvc=*/u"123");
    server_card.SetNetworkForMaskedCard(kVisaCard);
    server_card.set_server_id(server_id);
    server_card.set_guid(guid);
    server_card.set_instrument_id(instrument_id);
    return server_card;
  }

  CreditCard CreateLocalCard(
      const std::string& guid = "00000000-0000-0000-0000-000000000001") {
    CreditCard local_card(guid, test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Elvis Presley", "4111111111111111",
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", /*cvc=*/u"123");
    return local_card;
  }

  gfx::Image CustomIconForTest() { return gfx::test::CreateImage(32, 32); }

  void SetUpIbanImageResources() {
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
        .WillByDefault(testing::Return(CustomIconForTest()));
    did_set_up_image_resource_for_test_ = true;
  }

  void CleanUpIbanImageResources() {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(
        original_resource_bundle_.ExtractAsDangling());
  }

  bool VerifyCardArtImageExpectation(Suggestion& suggestion,
                                     const GURL& expected_url,
                                     const gfx::Image& expected_image) {
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      auto* custom_icon_url =
          absl::get_if<Suggestion::CustomIconUrl>(&suggestion.custom_icon);
      GURL url = custom_icon_url ? **custom_icon_url : GURL();
      return url == expected_url;
    } else {
      CHECK(absl::holds_alternative<gfx::Image>(suggestion.custom_icon));
      return AreImagesEqual(absl::get<gfx::Image>(suggestion.custom_icon),
                            expected_image);
    }
  }

  TestPaymentsDataManager& payments_data() {
    return autofill_client_.GetPersonalDataManager()
        ->test_payments_data_manager();
  }

  const std::string& app_locale() { return payments_data().app_locale(); }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
  // Tracks whether SetUpIbanImageResources() has been called, so that the
  // created images can be cleaned up when the test has finished.
  bool did_set_up_image_resource_for_test_ = false;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// TODO(crbug.com/325646493): Clean up
// PaymentsSuggestionGeneratorTest.AutofillCreditCardBenefitsLabelTest setup and
// parameters.
// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Issuer ID which is set for the credit card with benefits.
class AutofillCreditCardBenefitsLabelTest
    : public PaymentsSuggestionGeneratorTest,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>, std::string>> {
 public:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableCardBenefitsForAmericanExpress,
         features::kAutofillEnableCardBenefitsForCapitalOne,
         features::kAutofillEnableVirtualCardMetadata,
         features::kAutofillEnableCardProductName},
        /*disabled_features=*/{});

    std::u16string benefit_description;
    int64_t instrument_id;

    if (absl::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
      CreditCardFlatRateBenefit benefit =
          absl::get<CreditCardFlatRateBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else if (absl::holds_alternative<CreditCardMerchantBenefit>(
                   GetBenefit())) {
      CreditCardMerchantBenefit benefit =
          absl::get<CreditCardMerchantBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
      // Set the page URL in order to ensure that the merchant benefit is
      // displayed.
      autofill_client()->set_last_committed_primary_main_frame_url(
          benefit.merchant_domains().begin()->GetURL());
    } else if (absl::holds_alternative<CreditCardCategoryBenefit>(
                   GetBenefit())) {
      ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                  autofill_client()->GetAutofillOptimizationGuide()),
              AttemptToGetEligibleCreditCardBenefitCategory)
          .WillByDefault(testing::Return(
              CreditCardCategoryBenefit::BenefitCategory::kSubscription));
      CreditCardCategoryBenefit benefit =
          absl::get<CreditCardCategoryBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else {
      NOTREACHED_NORETURN();
    }

    expected_benefit_text_ = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS,
        benefit_description);
    card_ = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1",
        /*instrument_id=*/instrument_id);
    card_.set_issuer_id(std::get<1>(GetParam()));
    payments_data().AddServerCreditCard(card_);
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  const CreditCard& card() { return card_; }

  const std::u16string& expected_benefit_text() {
    return expected_benefit_text_;
  }

  // Checks that CreateCreditCardSuggestion appropriately labels cards with
  // benefits in MetadataLoggingContext.
  void DoBenefitSuggestionLabel_MetadataLoggingContextTest() {
    autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
    CreateCreditCardSuggestionForTest(
        card(), *autofill_client(), CREDIT_CARD_NUMBER,
        /*virtual_card_option=*/false,
        /*card_linked_offer_available=*/false, &metadata_logging_context);

    base::flat_map<int64_t, std::string>
        expected_instrument_ids_to_issuer_ids_with_benefits_available = {
            {card().instrument_id(), card().issuer_id()}};
    EXPECT_EQ(metadata_logging_context
                  .instrument_ids_to_issuer_ids_with_benefits_available,
              expected_instrument_ids_to_issuer_ids_with_benefits_available);
  }

 private:
  std::u16string expected_benefit_text_;
  CreditCard card_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    PaymentsSuggestionGeneratorTest,
    AutofillCreditCardBenefitsLabelTest,
    testing::Combine(testing::Values(&test::GetActiveCreditCardFlatRateBenefit,
                                     &test::GetActiveCreditCardCategoryBenefit,
                                     &test::GetActiveCreditCardMerchantBenefit),
                     ::testing::Values("amex", "capitalone")));

// Checks that for FPAN suggestions that the benefit description is displayed.
TEST_P(AutofillCreditCardBenefitsLabelTest, BenefitSuggestionLabel_Fpan) {
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(card(), *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{
              Suggestion::Text(expected_benefit_text())},
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that feature_for_iph is set to display the credit card benefit IPH for
// FPAN suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_Fpan) {
  EXPECT_EQ(CreateCreditCardSuggestionForTest(
                card(), *autofill_client(), CREDIT_CARD_NUMBER,
                /*virtual_card_option=*/false,
                /*card_linked_offer_available=*/false)
                .feature_for_iph,
            &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
}

// Checks that feature_for_iph is set to display the virtual card IPH for
// virtual card suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_VirtualCard) {
  EXPECT_EQ(CreateCreditCardSuggestionForTest(
                card(), *autofill_client(), CREDIT_CARD_NUMBER,
                /*virtual_card_option=*/true,
                /*card_linked_offer_available=*/false)
                .feature_for_iph,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
}

// Checks that for virtual cards suggestion the benefit description is shown
// with a virtual card label appended.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard) {
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(card(), *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{
              Suggestion::Text(expected_benefit_text())},
          std::vector<Suggestion::Text>{
              Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))}));
}

// Checks that for credit card suggestions with eligible benefits, the
// instrument id of the credit card is marked in the MetadataLoggingContext.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_MetadataLoggingContext) {
  DoBenefitSuggestionLabel_MetadataLoggingContextTest();
}

// Checks that for credit card suggestions with eligible benefits, the
// instrument id of the credit card is marked in the MetadataLoggingContext. The
// instrument ids should also be available when the benefit flags are disabled.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_MetadataLoggingContext_FlagsDisabled) {
  base::test::ScopedFeatureList disable_benefits;
  disable_benefits.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          features::kAutofillEnableCardBenefitsForAmericanExpress,
          features::kAutofillEnableCardBenefitsForCapitalOne});
  DoBenefitSuggestionLabel_MetadataLoggingContextTest();
}

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_MerchantUrlIsDifferent) {
  if (!absl::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-merchant benefits.";
  }
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  // Merchant benefit description is not returned.
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(card(), *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that the category benefit description is not displayed for suggestions
// where the webpage's category in the optimization guide is different from the
// benefit's applicable category.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_CategoryIsDifferent) {
  if (!absl::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-category benefits.";
  }

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));

  // Category benefit description is not returned.
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(card(), *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that the benefit description is not displayed when benefit suggestions
// are disabled for the given card and url.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_BlockedUrl) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockBenefitSuggestionLabelsForCardAndUrl)
      .WillByDefault(testing::Return(true));

  // Benefit description is not returned.
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(card(), *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false)
          .labels,
      testing::ElementsAre(
          std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests the scenario when:
// - autofill is triggered from the context menu on a field which is classified
// as a credit card field;
// - there is no card which has values to fill the respective field (or the
// field is a CVC which cannot be filled this way).
// In this scenario, suggestions should look the same as the ones for an
// unclassified field.
TEST_F(PaymentsSuggestionGeneratorTest,
       NoCreditCardsHaveValuesForClassifiedField_PaymentsManualFallback) {
  base::test::ScopedFeatureList features(
      features::kAutofillForUnclassifiedFieldsAvailable);
  CreditCard card = test::GetIncompleteCreditCard();
  ASSERT_FALSE(card.HasRawInfo(PHONE_HOME_WHOLE_NUMBER));
  payments_data().AddCreditCard(card);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NAME_FULL,
      AutofillSuggestionTriggerSource::kManualFallbackPayments,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  // This is the check which actually verifies that the suggestion looks the
  // same as the ones for an unclassified field (such a suggestion has
  // `is_acceptable` as false).
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));

  suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      AutofillSuggestionTriggerSource::kManualFallbackPayments,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  ASSERT_EQ(3u, suggestions.size());
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[0].is_acceptable, false);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

TEST_F(PaymentsSuggestionGeneratorTest,
       RemoveExpiredCreditCardsNotUsedSinceTimestamp) {
  const base::Time kNow = AutofillClock::Now();
  const base::Time kDisuseTime =
      kNow - kDisusedDataModelTimeDelta - base::Days(1);
  size_t card_number = 4111111111111111ul;

  std::vector<CreditCard> credit_cards;
  for (bool is_local : {false, true}) {
    for (bool is_expired : {false, true}) {
      for (bool is_disused : {false, true}) {
        // Create a credit card based on the current iteration.
        CreditCard credit_card =
            is_expired ? test::GetExpiredCreditCard() : test::GetCreditCard();
        credit_card.SetNumber(base::NumberToString16(card_number++));
        credit_card.set_use_date(is_disused ? kDisuseTime : kNow);
        if (is_local) {
          credit_card.set_record_type(CreditCard::RecordType::kLocalCard);
          payments_data().AddCreditCard(credit_card);
        } else {
          credit_card.set_record_type(
              CreditCard::RecordType::kMaskedServerCard);
          payments_data().AddServerCreditCard(credit_card);
        }
        credit_cards.push_back(credit_card);
      }
    }
  }
  base::HistogramTester histogram_tester;
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggestForTest(
      *autofill_client(), FormFieldData(), UNKNOWN_TYPE,
      /*suppress_disused_cards=*/true,
      /*prefix_match=*/false, /*include_virtual_cards=*/false);

  // Expect that only the last card (disused, expired and local) is removed.
  credit_cards.pop_back();
  EXPECT_THAT(cards_to_suggest, UnorderedElementsAreArray(credit_cards));

  constexpr char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

// Tests that credit card suggestions are not subject to prefix matching for the
// credit card number if `kAutofillDontPrefixMatchCreditCardNumbersOrCvcs` is
// enabled.
TEST_F(PaymentsSuggestionGeneratorTest,
       NoPrefixMatchingForCreditCardsIfFeatureIsTurnedOn) {
  base::test::ScopedFeatureList features(
      features::kAutofillDontPrefixMatchCreditCardNumbersOrCvcs);
  CreditCard card1 = test::GetCreditCard();
  card1.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(card1);
  CreditCard card2 = test::GetCreditCard2();
  card2.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  payments_data().AddServerCreditCard(card2);

  auto get_cards = [&](std::u16string field_value) {
    FormFieldData field;
    field.set_value(std::move(field_value));
    return GetOrderedCardsToSuggestForTest(*autofill_client(), field,
                                           CREDIT_CARD_NUMBER,
                                           /*suppress_disused_cards=*/false,
                                           /*prefix_match=*/true,
                                           /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u""), UnorderedElementsAre(card1, card2));

  ASSERT_NE(card1.number(), card2.number());
  EXPECT_THAT(get_cards(card1.number()), UnorderedElementsAre(card1, card2));

  EXPECT_THAT(get_cards(card2.number()), UnorderedElementsAre(card1, card2));
}

// Tests that credit card suggestions are not subject to prefix matching for the
// CVC if `kAutofillDontPrefixMatchCreditCardNumbersOrCvcs` is enabled.
TEST_F(PaymentsSuggestionGeneratorTest,
       NoPrefixMatchingForCvcsIfFeatureIsTurnedOn) {
  base::test::ScopedFeatureList features(
      features::kAutofillDontPrefixMatchCreditCardNumbersOrCvcs);
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"Cardholder name",
                          /*card_number=*/"1111222233334444",
                          /*expiration_month=*/nullptr,
                          /*expiration_year*/ nullptr,
                          /*billing_address_id=*/"", /*cvc=*/u"123");
  credit_card.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(credit_card);

  auto get_cards = [&](std::u16string field_value) {
    FormFieldData field;
    field.set_value(std::move(field_value));
    return GetOrderedCardsToSuggestForTest(*autofill_client(), field,
                                           CREDIT_CARD_VERIFICATION_CODE,
                                           /*suppress_disused_cards=*/false,
                                           /*prefix_match=*/true,
                                           /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u""), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"1"), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"2"), ElementsAre(credit_card));
}

TEST_F(PaymentsSuggestionGeneratorTest,
       ManualFallback_UnusedExpiredCardsAreNotSuppressed) {
  CreditCard local_card = test::GetCreditCard();
  local_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  local_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2000");
  local_card.set_use_date(AutofillClock::Now() - kDisusedDataModelTimeDelta -
                          base::Days(1));
  payments_data().AddCreditCard(local_card);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), UNKNOWN_TYPE,
      AutofillSuggestionTriggerSource::kManualFallbackPayments,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  EXPECT_FALSE(suggestions.empty());
}

TEST_F(PaymentsSuggestionGeneratorTest, GetServerCardForLocalCard) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNumber(u"4111111111111111");
  payments_data().AddServerCreditCard(server_card);
  CreditCard local_card =
      CreateLocalCard("00000000-0000-0000-0000-000000000002");

  // The server card should be returned if the local card is passed in.
  const CreditCard* result =
      payments_data().GetServerCardForLocalCard(&local_card);
  ASSERT_TRUE(result);
  EXPECT_EQ(server_card.guid(), result->guid());

  // Should return nullptr if a server card is passed in.
  EXPECT_FALSE(payments_data().GetServerCardForLocalCard(&server_card));

  // Should return nullptr if no server card has the same information as the
  // local card.
  server_card.SetNumber(u"5454545454545454");
  payments_data().ClearCreditCards();
  payments_data().AddServerCreditCard(server_card);
  EXPECT_FALSE(payments_data().GetServerCardForLocalCard(&local_card));
}

// The suggestions of credit cards with card linked offers are moved to the
// front. This test checks that the order of the other cards remains stable.
TEST_F(PaymentsSuggestionGeneratorTest,
       GetSuggestionsForCreditCards_StableSortBasedOnOffer) {
  // Create three server cards.
  payments_data().ClearCreditCards();
  payments_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000001",
      /*server_id=*/"server_id1", /*instrument_id=*/1));
  payments_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000002",
      /*server_id=*/"server_id2", /*instrument_id=*/2));
  payments_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000003",
      /*server_id=*/"server_id3", /*instrument_id=*/3));

  // Create a card linked offer and attach it to server_card2.
  AutofillOfferData offer_data = test::GetCardLinkedOfferData1();
  offer_data.SetMerchantOriginForTesting({GURL("http://www.example1.com")});
  offer_data.SetEligibleInstrumentIdForTesting({2});
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("http://www.example1.com"));
  payments_data().AddAutofillOfferData(offer_data);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  EXPECT_TRUE(summary.with_offer);
  ASSERT_EQ(suggestions.size(), 5U);
  // The suggestion with card linked offer available should be ranked to the
  // top.
  EXPECT_EQ(suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")));
  // The other suggestions should have their relative ranking unchanged.
  EXPECT_EQ(suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000003")));
  EXPECT_EQ(suggestions[2].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Ensures we appropriately generate suggestions for virtual cards on a
// standalone CVC field.
TEST_F(PaymentsSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc) {
  CreditCard server_card = CreateServerCard();
  payments_data().AddServerCreditCard(server_card);

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"1234")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      GetSuggestionsForVirtualCardStandaloneCvc(
          *autofill_client(), FormFieldData(), metadata_logging_context,
          virtual_card_guid_to_last_four_map);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsSuggestionGeneratorTest,
       GetSuggestionsForVirtualCardStandaloneCvc_UndoAutofill) {
  CreditCard server_card = CreateServerCard();
  payments_data().AddServerCreditCard(CreateServerCard());

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"4444")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  FormFieldData field;
  field.set_is_autofilled(true);
  std::vector<Suggestion> suggestions =
      GetSuggestionsForVirtualCardStandaloneCvc(
          *autofill_client(), field, metadata_logging_context,
          virtual_card_guid_to_last_four_map);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kVirtualCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsUndoAutofillSuggestion(),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}
#endif

// Ensures we appropriately generate suggestions for credit saved with CVC.
TEST_F(PaymentsSuggestionGeneratorTest, GetCardSuggestionsWithCvc) {
  CreditCard card = test::WithCvc(test::GetMaskedServerCard2());
  payments_data().AddServerCreditCard(card);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(summary.with_cvc);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verifies that the GPay logo is set correctly.
TEST_F(PaymentsSuggestionGeneratorTest, ShouldDisplayGpayLogo) {
  // GPay logo should be displayed if suggestions were all for server cards;
  {
    // Create two server cards.
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1", /*instrument_id=*/1));
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    CreditCardSuggestionSummary summary;
    std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
        kDefaultTriggerSource,
        /*should_show_scan_credit_card=*/false,
        /*should_show_cards_from_account=*/false, summary);

    EXPECT_EQ(suggestions.size(), 4U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }

  payments_data().ClearCreditCards();

  // GPay logo should not be displayed if at least one local card was in the
  // suggestions.
  {
    // Create one server card and one local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    payments_data().AddCreditCard(local_card);
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    CreditCardSuggestionSummary summary;
    std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
        kDefaultTriggerSource,
        /*should_show_scan_credit_card=*/false,
        /*should_show_cards_from_account=*/false, summary);

    EXPECT_EQ(suggestions.size(), 4U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
  }

  payments_data().ClearCreditCards();

  // GPay logo should be displayed if there was an unused expired local card in
  // the suggestions.
  {
    // Create one server card and one unused expired local card.
    auto local_card = CreateLocalCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card.SetNumber(u"5454545454545454");
    local_card.SetExpirationYear(2020);
    local_card.set_use_date(AutofillClock::Now() - base::Days(365));
    payments_data().AddCreditCard(local_card);
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    CreditCardSuggestionSummary summary;
    std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
        kDefaultTriggerSource,
        /*should_show_scan_credit_card=*/false,
        /*should_show_cards_from_account=*/false, summary);

    EXPECT_EQ(suggestions.size(), 3U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }
}

TEST_F(PaymentsSuggestionGeneratorTest, NoSuggestionsWhenNoUserData) {
  FormFieldData field;
  field.set_is_autofilled(true);
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field, CREDIT_CARD_NUMBER, kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/true,
      /*should_show_cards_from_account=*/true, summary);

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(PaymentsSuggestionGeneratorTest, ShouldShowScanCreditCard) {
  payments_data().AddCreditCard(test::GetCreditCard());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/true,
      /*should_show_cards_from_account=*/false, summary);

  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0],
              EqualsSuggestion(SuggestionType::kCreditCardEntry));
  EXPECT_THAT(
      suggestions[1],
      EqualsSuggestion(SuggestionType::kScanCreditCard,
                       l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
                       Suggestion::Icon::kScanCreditCard));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

TEST_F(PaymentsSuggestionGeneratorTest, ShouldShowCardsFromAccount) {
  payments_data().AddCreditCard(test::GetCreditCard());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/true, summary);

  EXPECT_EQ(suggestions.size(), 4ul);
  EXPECT_THAT(suggestions[0],
              EqualsSuggestion(SuggestionType::kCreditCardEntry));
  EXPECT_THAT(suggestions[1],
              EqualsSuggestion(
                  SuggestionType::kShowAccountCards,
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
                  Suggestion::Icon::kGoogle));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsSuggestionGeneratorTest,
       FieldWasAutofilled_UndoAutofillOnCreditCardForm) {
  payments_data().AddCreditCard(test::GetCreditCard());
  FormFieldData field;
  field.set_is_autofilled(true);
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field, CREDIT_CARD_NUMBER, kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsUndoAutofillSuggestion(),
                          EqualsManagePaymentsMethodsSuggestion(
                              /*with_gpay_logo=*/false)));
}
#endif

// Test that the virtual card option is shown when all of the prerequisites are
// met.
TEST_F(PaymentsSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(&server_card, *autofill_client()));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(&local_card, *autofill_client()));
}

// Test that the virtual card option is shown when the autofill optimization
// guide is not present.
TEST_F(PaymentsSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_AutofillOptimizationGuideNotPresent) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(server_card);
  autofill_client()->ResetAutofillOptimizationGuide();

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(&server_card, *autofill_client()));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(&local_card, *autofill_client()));
}

// Test that the virtual card option is shown even if the merchant is opted-out
// of virtual cards.
TEST_F(PaymentsSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_InDisabledStateForOptedOutMerchants) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableVcnGrayOutForMerchantOptOut);

  // Create an enrolled server card.
  CreditCard server_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  payments_data().AddServerCreditCard(server_card);

  // Even if the URL is opted-out of virtual cards for `server_card`, display
  // the virtual card suggestion.
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(&server_card, *autofill_client()));
}

// Test that the virtual card option is not shown if the merchant is opted-out
// of virtual cards.
TEST_F(PaymentsSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_MerchantOptedOutOfVirtualCards) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kAutofillEnableVcnGrayOutForMerchantOptOut);
  // Create an enrolled server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If the URL is opted-out of virtual cards for `server_card`, do not display
  // the virtual card suggestion.
  auto* optimization_guide = autofill_client()->GetAutofillOptimizationGuide();
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(optimization_guide),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(&server_card, *autofill_client()));
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(&local_card, *autofill_client()));
}

// Test that the virtual card option is not shown if the server card we might be
// showing a virtual card option for is not enrolled into virtual card.
TEST_F(PaymentsSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_ServerCardNotEnrolledInVirtualCard) {
  // Create an unenrolled server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnspecified);
  payments_data().AddServerCreditCard(server_card);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // For server card not enrolled, both local and server card should return
  // false.
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(&server_card, *autofill_client()));
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(&local_card, *autofill_client()));
}

// Test that the virtual card option is not shown for a local card with no
// server card duplicate.
TEST_F(PaymentsSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_LocalCardWithoutServerCardDuplicate) {
  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // The local card does not have a server duplicate, should return false.
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(&local_card, *autofill_client()));
}

TEST_F(PaymentsSuggestionGeneratorTest, GetLocalIbanSuggestions) {
  SetUpIbanImageResources();

  auto MakeLocalIban = [](const std::u16string& value,
                          const std::u16string& nickname) {
    Iban iban(Iban::Guid(base::Uuid::GenerateRandomV4().AsLowercaseString()));
    iban.set_value(value);
    if (!nickname.empty()) {
      iban.set_nickname(nickname);
    }
    return iban;
  };
  Iban iban0 =
      MakeLocalIban(u"CH56 0483 5012 3456 7800 9", u"My doctor's IBAN");
  Iban iban1 =
      MakeLocalIban(u"DE91 1000 0000 0123 4567 89", u"My brother's IBAN");
  Iban iban2 =
      MakeLocalIban(u"GR96 0810 0010 0000 0123 4567 890", u"My teacher's IBAN");
  Iban iban3 = MakeLocalIban(u"PK70 BANK 0000 1234 5678 9000", u"");

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({iban0, iban1, iban2, iban3});

  // There are 6 suggestions, 4 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 6u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(iban0.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban0.guid()), iban0.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban1.guid()), iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban2.guid()), iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[3],
      EqualsIbanSuggestion(iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(iban3.guid()), iban3.nickname()));

  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[5].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[5].type, SuggestionType::kManageIban);
}

TEST_F(PaymentsSuggestionGeneratorTest, GetServerIbanSuggestions) {
  SetUpIbanImageResources();

  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban server_iban3 = test::GetServerIban3();

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({server_iban1, server_iban2, server_iban3});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(server_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban1.instrument_id())),
                           server_iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(server_iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban2.instrument_id())),
                           server_iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(server_iban3.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban3.instrument_id())),
                           server_iban3.nickname()));

  EXPECT_EQ(iban_suggestions[3].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kManageIban);
}

TEST_F(PaymentsSuggestionGeneratorTest, GetLocalAndServerIbanSuggestions) {
  SetUpIbanImageResources();

  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();
  Iban local_iban1 = test::GetLocalIban();

  std::vector<Suggestion> iban_suggestions =
      GetSuggestionsForIbans({server_iban1, server_iban2, local_iban1});

  // There are 5 suggestions, 3 for IBAN suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  ASSERT_EQ(iban_suggestions.size(), 5u);

  EXPECT_THAT(
      iban_suggestions[0],
      EqualsIbanSuggestion(server_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban1.instrument_id())),
                           server_iban1.nickname()));

  EXPECT_THAT(
      iban_suggestions[1],
      EqualsIbanSuggestion(server_iban2.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::BackendId(Suggestion::InstrumentId(
                               server_iban2.instrument_id())),
                           server_iban2.nickname()));

  EXPECT_THAT(
      iban_suggestions[2],
      EqualsIbanSuggestion(local_iban1.GetIdentifierStringForAutofillDisplay(),
                           Suggestion::Guid(local_iban1.guid()),
                           local_iban1.nickname()));

  EXPECT_EQ(iban_suggestions[3].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kManageIban);
}

TEST_F(PaymentsSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  std::vector<const AutofillOfferData*> promo_code_offers;

  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/1, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  promo_code_offers.push_back(&offer1);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::FreeListingCouponOffer(
      /*offer_id=*/2, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  promo_code_offers.push_back(&offer2);

  std::vector<Suggestion> promo_code_suggestions =
      GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 4);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("1")));
  EXPECT_THAT(promo_code_suggestions[0],
              EqualLabels({{u"test_value_prop_text_1"}}));
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("1")));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("2")));
  EXPECT_THAT(promo_code_suggestions[1],
              EqualLabels({{u"test_value_prop_text_2"}}));
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(Suggestion::Guid("2")));
  EXPECT_EQ(promo_code_suggestions[1].type,
            SuggestionType::kMerchantPromoCodeEntry);

  EXPECT_EQ(promo_code_suggestions[2].type, SuggestionType::kSeparator);

  EXPECT_EQ(promo_code_suggestions[3].main_text.value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
  EXPECT_EQ(promo_code_suggestions[3].GetPayload<GURL>(),
            offer1.GetOfferDetailsUrl().spec());
  EXPECT_EQ(promo_code_suggestions[3].type,
            SuggestionType::kSeePromoCodeDetails);
}

TEST_F(PaymentsSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_InvalidPromoCodeURL) {
  std::vector<const AutofillOfferData*> promo_code_offers;
  AutofillOfferData offer;
  offer.SetPromoCode("test_promo_code_1");
  offer.SetValuePropTextInDisplayStrings("test_value_prop_text_1");
  offer.SetOfferIdForTesting(1);
  offer.SetOfferDetailsUrl(GURL("invalid-url"));
  promo_code_offers.push_back(&offer);

  std::vector<Suggestion> promo_code_suggestions =
      GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 1);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_THAT(promo_code_suggestions[0],
              EqualLabels({{u"test_value_prop_text_1"}}));
  EXPECT_FALSE(
      absl::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);
}

// This class helps test the credit card contents that are displayed in
// Autofill suggestions. It covers suggestions on Desktop/Android dropdown,
// and on Android keyboard accessory.
class AutofillCreditCardSuggestionContentTest
    : public PaymentsSuggestionGeneratorTest {
 public:
  AutofillCreditCardSuggestionContentTest() {
    feature_list_metadata_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnableVirtualCardMetadata,
                              features::kAutofillEnableCardProductName,
                              features::
                                  kAutofillEnableVcnGrayOutForMerchantOptOut},
        /*disabled_features=*/{});
  }

  ~AutofillCreditCardSuggestionContentTest() override = default;

  bool keyboard_accessory_enabled() const {
#if BUILDFLAG(IS_ANDROID)
    return true;
#else
    return false;
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_metadata_;
};

// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the cardholder name field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_VirtualCardMetadata_NameField) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the "Virtual card" label is added as a prefix
    // to the cardholder name.
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Virtual card  Elvis Presley");
    EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"");
  } else {
    // On other platforms, the cardholder name is shown on the first line.
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Elvis Presley");
    EXPECT_EQ(virtual_card_name_field_suggestion.minor_text.value, u"");
  }

#if BUILDFLAG(IS_IOS)
  // There should be 2 lines of labels:
  // 1. Obfuscated last 4 digits "..1111".
  // 2. Virtual card label.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/2, u"1111"));
#else
  if (keyboard_accessory_enabled()) {
    // There should be only 1 line of label: obfuscated last 4 digits "..1111".
    EXPECT_THAT(virtual_card_name_field_suggestion,
                EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                    /*obfuscation_length=*/2, u"1111")}}));
  } else {
    // There should be 2 lines of labels:
    // 1. Card name + obfuscated last 4 digits "CardName  ....1111". Card name
    // and last four are populated separately.
    // 2. Virtual card label.
    ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 2U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value, u"Visa");
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][1].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif
  EXPECT_EQ(virtual_card_name_field_suggestion.is_acceptable, true);
  if (!keyboard_accessory_enabled()) {
    // The virtual card text should be populated in the labels to be shown in a
    // new line.
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
              u"Virtual card");
  }
}

// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the card number field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_VirtualCardMetadata_NumberField) {
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_IOS)
  // Only card number is displayed on the first line.
  EXPECT_EQ(
      virtual_card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   /*obfuscation_length=*/2, u"1111")}));
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value, u"");
#else
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the "Virtual card" label is added as a prefix
    // to the card number. The obfuscated last four digits are shown in a
    // separate view.
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
              u"Virtual card  Visa");
    EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111"));
  } else {
    // Card name and the obfuscated last four digits are shown separately.
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value, u"Visa");
    EXPECT_EQ(virtual_card_number_field_suggestion.minor_text.value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif
  EXPECT_EQ(virtual_card_number_field_suggestion.is_acceptable, true);
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, there is no label.
    ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
  } else {
    // For Desktop/Android dropdown, and on iOS, "Virtual card" is the label.
    EXPECT_THAT(virtual_card_number_field_suggestion,
                EqualLabels({{u"Virtual card"}}));
  }
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the cardholder name field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_MaskedServerCardMetadata_NameField) {
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for non-virtual cards.
  Suggestion real_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Elvis Presley");
  EXPECT_EQ(real_card_name_field_suggestion.minor_text.value, u"");

#if BUILDFLAG(IS_IOS)
  // For IOS, the label is "..1111".
  EXPECT_THAT(real_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111")}}));
#else
  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, the label is "..1111".
    EXPECT_THAT(real_card_name_field_suggestion,
                EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                    /*obfuscation_length=*/2, u"1111")}}));
  } else {
    // For Desktop/Android, the label is "CardName  ....1111". Card name and
    // last four are shown separately.
    ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
    ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 2U);
    EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value, u"Visa");
    EXPECT_EQ(real_card_name_field_suggestion.labels[0][1].value,
              CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/4, u"1111"));
  }
#endif
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the card number field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_MaskedServerCardMetadata_NumberField) {
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for non-virtual cards.
  Suggestion real_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false);

#if BUILDFLAG(IS_IOS)
  // Only the card number is displayed on the first line.
  EXPECT_EQ(
      real_card_number_field_suggestion.main_text.value,
      base::StrCat({u"Visa  ", CreditCard::GetObfuscatedStringForCardDigits(
                                   /*obfuscation_length=*/2, u"1111")}));
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value, u"");
#else
  // For Desktop/Android, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(real_card_number_field_suggestion.minor_text.value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/keyboard_accessory_enabled() ? 2 : 4,
                u"1111"));
#endif

  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(
      real_card_number_field_suggestion,
      EqualLabels(
          {{base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))})}}));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback) {
  CreditCard server_card = CreateServerCard();

  Suggestion server_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(server_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(server_card_suggestion.is_acceptable, false);
  // For Desktop, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(server_card_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(server_card_suggestion.minor_text.value,
            server_card.ObfuscatedNumberWithVisibleLastFourDigits(4));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(server_card_suggestion,
              EqualLabels({{server_card.GetInfo(
                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale())}}));

  EXPECT_EQ(server_card_suggestion.acceptance_a11y_announcement,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_A11Y_ANNOUNCE_EXPANDABLE_ONLY_ENTRY));
}

// Verify that the virtual credit card suggestion has the correct
// `Suggestion::type, AX label and is selectable.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard) {
  CreditCard enrolled_card = test::GetVirtualCard();

  Suggestion enrolled_card_suggestion = CreateCreditCardSuggestionForTest(
      enrolled_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(enrolled_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(enrolled_card_suggestion.is_acceptable, true);
  EXPECT_EQ(enrolled_card_suggestion.acceptance_a11y_announcement,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_A11Y_ANNOUNCE_VIRTUAL_CARD_MANUAL_FALLBACK_ENTRY));
}

// Verify that the virtual credit card suggestion has the correct labels.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard_Labels) {
  CreditCard enrolled_card = test::GetVirtualCard();

  Suggestion enrolled_card_suggestion = CreateCreditCardSuggestionForTest(
      enrolled_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  // For Desktop, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(enrolled_card_suggestion.main_text.value, u"Mastercard");
  EXPECT_EQ(enrolled_card_suggestion.minor_text.value,
            enrolled_card.ObfuscatedNumberWithVisibleLastFourDigits(4));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_EQ(enrolled_card_suggestion.labels.size(), 2U);
  EXPECT_EQ(enrolled_card_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(
      enrolled_card_suggestion.labels[0][0].value,
      enrolled_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale()));
  EXPECT_EQ(enrolled_card_suggestion.labels[1].size(), 1U);
  EXPECT_EQ(enrolled_card_suggestion.labels[1][0].value,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE));
}

// Verify that the virtual credit card suggestion has no nested suggestions.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_VirtualCreditCard_NestedSuggestions) {
  CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();

  Suggestion enrolled_card_suggestion = CreateCreditCardSuggestionForTest(
      enrolled_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  EXPECT_TRUE(enrolled_card_suggestion.children.empty());
}

// Verify that the nested suggestion's texts are populated correctly for a
// masked server card suggestion when payments manual fallback is triggered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_NestedSuggestions) {
  CreditCard server_card = test::GetMaskedServerCard();

  Suggestion server_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card full name
  // 2. Credit card number
  // 3. Separator
  // 4. Credit card expiry date
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(server_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              server_card.ObfuscatedNumberWithVisibleLastFourDigits(12),
              CREDIT_CARD_NUMBER, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))}}),
          AllOf(Field(&Suggestion::type, SuggestionType::kSeparator)),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                  app_locale()),
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
              Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))}})));
}

// Verify that the nested suggestion's texts are populated correctly for a
// credit card with no expiry date set.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_NoExpiryDate_NestedSuggestions) {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, /*name_on_card=*/"Cardholder name",
                          /*card_number=*/"1111222233334444",
                          /*expiration_month=*/nullptr,
                          /*expiration_year*/ nullptr,
                          /*billing_address_id=*/"", /*cvc=*/u"123");

  Suggestion server_card_suggestion = CreateCreditCardSuggestionForTest(
      credit_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card full name
  // 2. Credit card number
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale()),
              CREDIT_CARD_NAME_FULL, Suggestion::Guid(credit_card.guid())),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              credit_card.ObfuscatedNumberWithVisibleLastFourDigits(12),
              CREDIT_CARD_NUMBER, Suggestion::Guid(credit_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))}})));
}

// Verify that the nested suggestion's texts are populated correctly for a
// credit card with no cardholder name and credit card number.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_ManualFallback_NoNameAndNumber_NestedSuggestions) {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, /*name_on_card=*/nullptr,
                          /*card_number=*/nullptr, test::NextMonth().c_str(),
                          test::NextYear().c_str(),
                          /*billing_address_id=*/"", /*cvc=*/u"123");

  Suggestion server_card_suggestion = CreateCreditCardSuggestionForTest(
      credit_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // The child suggestions should be:
  //
  // 1. Credit card expiry date
  EXPECT_THAT(
      server_card_suggestion.children,
      ElementsAre(EqualsFieldByFieldFillingSuggestion(
          SuggestionType::kCreditCardFieldByFieldFilling,
          credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale()),
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
          Suggestion::Guid(credit_card.guid()),
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))}})));
}

// Verify nested suggestions of the expiry date suggestion.
TEST_F(AutofillCreditCardSuggestionContentTest,
       CreateCreditCardSuggestion_ManualFallback_NestedExpiryDateSuggestions) {
  CreditCard server_card = CreateServerCard();

  Suggestion server_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), UNKNOWN_TYPE,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // The expiry date child suggestions should be:
  //
  // 1. Expiry year.
  // 2. Expiry month.
  EXPECT_THAT(
      server_card_suggestion.children[3].children,
      ElementsAre(
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale()),
              CREDIT_CARD_EXP_MONTH, Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_MONTH_SUGGESTION_LABEL))}}),
          EqualsFieldByFieldFillingSuggestion(
              SuggestionType::kCreditCardFieldByFieldFilling,
              server_card.GetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale()),
              CREDIT_CARD_EXP_2_DIGIT_YEAR,
              Suggestion::Guid(server_card.guid()),
              {{Suggestion::Text(l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_YEAR_SUGGESTION_LABEL))}})));
}

// Verify that manual fallback credit card suggestions are not filtered.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_ManualFallbackSuggestionsNotFiltered) {
  payments_data().AddServerCreditCard(CreateServerCard());

  FormFieldData field_data;
  field_data.set_value(u"$$$");
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field_data, UNKNOWN_TYPE,
      AutofillSuggestionTriggerSource::kManualFallbackPayments,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Credit card suggestions should not depend on the field's value.
  EXPECT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Verify that the suggestion's texts are populated correctly for a local and
// server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_CvcField) {
  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  // We used last 4 to deduplicate local card and server card so we should set
  // local card with different last 4.
  local_card.SetNumber(u"5454545454545454");
  payments_data().AddCreditCard(std::move(local_card));
  payments_data().AddServerCreditCard(CreateServerCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Both local card and server card suggestion should be shown when CVC field
  // is focused.
  ASSERT_EQ(suggestions.size(), 4U);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#else
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Mastercard");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Verify that the suggestion's texts are populated correctly for a duplicate
// local and server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_Duplicate_CvcField) {
  // Create 2 duplicate local and server card with same last 4.
  payments_data().AddCreditCard(CreateLocalCard());
  payments_data().AddServerCreditCard(CreateServerCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Only 1 suggestion + footer should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_VirtualCard_CvcField) {
  // Create a server card with CVC that enrolled to virtual card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(std::move(server_card));

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#else
  EXPECT_EQ(suggestions[0].main_text.value, u"Virtual card  CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[0].minor_text.value, u"");
  EXPECT_EQ(suggestions[1].minor_text.value, u"");
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_VirtualCard_Duplicate_CvcField) {
  // Create duplicate local and server card with CVC that enrolled to virtual
  // card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(std::move(server_card));
  payments_data().AddCreditCard(CreateLocalCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_VERIFICATION_CODE,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if BUILDFLAG(IS_IOS)
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetSuggestionsForCreditCards_LargeKeyboardAccessoryFormat) {
  // Enable formatting for large keyboard accessories.
  autofill_client()->set_format_for_large_keyboard_accessory(true);

  CreditCard server_card = CreateServerCard();

  const std::u16string obfuscated_number =
      CreditCard::GetObfuscatedStringForCardDigits(/*obfuscation_length=*/2, u"1111");
  const std::u16string name_full =
      server_card.GetRawInfo(CREDIT_CARD_NAME_FULL);
  const std::u16string exp_date =
      server_card.GetRawInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  const std::u16string card_type = server_card.GetRawInfo(CREDIT_CARD_TYPE);
  const std::u16string type_and_number =
      base::StrCat({card_type, u"  ", obfuscated_number});

  Suggestion card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // From the credit card number field, the suggestion should show the card type
  // and number and the label should show the expiration date.
  EXPECT_EQ(card_number_field_suggestion.main_text.value, type_and_number);
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{exp_date}}));

  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NAME_FULL,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // From the credit card name field, the suggestion should show the full name
  // and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({name_full}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_EXP_MONTH,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // From a credit card expiry field, the suggestion should show the expiration
  // date and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({exp_date}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  server_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  // From a virtual credit card, the suggestion should show the card name and
  // the label should show the card's virtual status, type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({server_card.CardNameForAutofillDisplay(
                server_card.nickname())}));
  EXPECT_THAT(
      card_number_field_suggestion,
      EqualLabels({{l10n_util::GetStringUTF16(
                        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
                    u" • " + card_type + u" " + obfuscated_number}}));
}

#endif  // BUILDFLAG(IS_IOS)

// The boolean param denotes if merchant has opted out of VCN.
class AutofillCreditCardSuggestionContentVcnMerchantOptOutTest
    : public AutofillCreditCardSuggestionContentTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_merchant_opted_out() { return GetParam(); }

  int expected_message_id() {
    return is_merchant_opted_out()
               ? IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE
               : IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE;
  }

 private:
  void SetUp() override {
    AutofillCreditCardSuggestionContentTest::SetUp();
    // Content test is only needed when the gray-out feature is enabled.
    // Otherwise user will not see a VCN for opted out merchants.
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableVcnGrayOutForMerchantOptOut, true);

    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client()->GetAutofillOptimizationGuide()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(is_merchant_opted_out()));
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    testing::Bool());

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the cardholder name field is focused based on if
// merchant accepts virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NameField) {
  CreditCard server_card = test::GetVirtualCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // `is_acceptable` is false only when merchant has opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.is_acceptable,
            !is_merchant_opted_out());

  // `apply_deactivated_style` is true only when merchant has opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.apply_deactivated_style,
            is_merchant_opted_out());

  if (keyboard_accessory_enabled()) {
    // There should be only 1 line of label: obfuscated last 4 digits "..4444".
    EXPECT_THAT(virtual_card_name_field_suggestion,
                EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                    /*obfuscation_length=*/2, u"4444")}}));
  } else {
    // The virtual card text should be populated in the labels to be shown in a
    // new line.
    ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(expected_message_id()));
  }
}

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the card number field is focused based on if
// merchant accepts virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NumberField) {
  CreditCard server_card = test::GetVirtualCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // `is_acceptable` is false only when flag is enabled and merchant has opted
  // out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.is_acceptable,
            !is_merchant_opted_out());
  // `apply_deactivated_style` is true only when merchant has opted out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.apply_deactivated_style,
            is_merchant_opted_out());

  if (keyboard_accessory_enabled()) {
    // For the keyboard accessory, there is no label.
    ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
  } else {
    EXPECT_THAT(
        virtual_card_number_field_suggestion,
        EqualLabels({{l10n_util::GetStringUTF16(expected_message_id())}}));
  }
}

class PaymentsSuggestionGeneratorTestForMetadata
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  PaymentsSuggestionGeneratorTestForMetadata() {
    feature_list_card_product_description_.InitWithFeatureState(
        features::kAutofillEnableCardProductName, std::get<0>(GetParam()));
    feature_list_card_art_image_.InitWithFeatureState(
        features::kAutofillEnableCardArtImage, std::get<1>(GetParam()));
  }

  ~PaymentsSuggestionGeneratorTestForMetadata() override = default;

  bool card_product_description_enabled() const {
    return std::get<0>(GetParam());
  }
  bool card_art_image_enabled() const { return std::get<1>(GetParam()); }
  bool card_has_capital_one_icon() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_card_product_description_;
  base::test::ScopedFeatureList feature_list_card_art_image_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PaymentsSuggestionGeneratorTestForMetadata,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_ServerCard) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  payments_data().AddCardArtImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());
}

TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_LocalCard_NoServerDuplicate) {
  // Create a local card.
  CreditCard local_card = CreateLocalCard();

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, GURL(),
                                            gfx::Image()));
}

TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_LocalCard_ServerDuplicate) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  payments_data().AddServerCreditCard(server_card);
  payments_data().AddCardArtImage(card_art_url, fake_image);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")));
  EXPECT_EQ(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                          fake_image),
            card_art_image_enabled());
}

// Verifies that the `metadata_logging_context` is correctly set.
TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       GetSuggestionsForCreditCards_MetadataLoggingContext) {
  {
    // Create one server card with no metadata.
    CreditCard server_card = CreateServerCard();
    server_card.set_issuer_id(kCapitalOneCardIssuerId);
    if (card_has_capital_one_icon()) {
      server_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
    }
    payments_data().AddServerCreditCard(server_card);

    CreditCardSuggestionSummary summary;
    GetSuggestionsForCreditCards(*autofill_client(), FormFieldData(),
                                 CREDIT_CARD_NUMBER, kDefaultTriggerSource,
                                 /*should_show_scan_credit_card=*/false,
                                 /*should_show_cards_from_account=*/false,
                                 summary);

    EXPECT_TRUE(summary.metadata_logging_context
                    .instruments_with_metadata_available.empty());
    EXPECT_FALSE(
        summary.metadata_logging_context.card_product_description_shown);
    EXPECT_FALSE(summary.metadata_logging_context.card_art_image_shown);

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it did not have metadata.
    base::flat_map<std::string, bool>
        expected_issuer_or_network_to_metadata_availability = {
            {server_card.issuer_id(), false}, {server_card.network(), false}};
    EXPECT_EQ(summary.metadata_logging_context
                  .issuer_or_network_to_metadata_availability,
              expected_issuer_or_network_to_metadata_availability);
  }

  payments_data().ClearCreditCards();

  {
    // Create a server card with card product description & card art image.
    CreditCard server_card_with_metadata = CreateServerCard();
    server_card_with_metadata.set_issuer_id(kCapitalOneCardIssuerId);
    server_card_with_metadata.set_product_description(u"product_description");
    server_card_with_metadata.set_card_art_url(
        GURL("https://www.example.com/card-art.png"));
    payments_data().AddServerCreditCard(server_card_with_metadata);

    CreditCardSuggestionSummary summary;
    GetSuggestionsForCreditCards(*autofill_client(), FormFieldData(),
                                 CREDIT_CARD_NUMBER, kDefaultTriggerSource,
                                 /*should_show_scan_credit_card=*/false,
                                 /*should_show_cards_from_account=*/false,
                                 summary);

    EXPECT_TRUE(
        summary.metadata_logging_context.instruments_with_metadata_available
            .contains(server_card_with_metadata.instrument_id()));
    EXPECT_EQ(summary.metadata_logging_context.card_product_description_shown,
              card_product_description_enabled());
    EXPECT_EQ(summary.metadata_logging_context.card_art_image_shown,
              card_art_image_enabled());

    // Verify that a record is added that a Capital One card suggestion
    // was generated, and it had metadata.
    base::flat_map<std::string, bool> expected_issuer_to_metadata_availability =
        {{server_card_with_metadata.issuer_id(), true},
         {server_card_with_metadata.network(), true}};
    EXPECT_EQ(summary.metadata_logging_context
                  .issuer_or_network_to_metadata_availability,
              expected_issuer_to_metadata_availability);
  }
}

// TODO(crbug.com/332595462): Improve card art url unittest coverage to include
// potential edge cases.
//  Verifies that the custom icon is set correctly. The card art should be shown
//  when the metadata card art flag is enabled. Capital One virtual card icon is
//  an exception which should only and always be shown for virtual cards.
TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       GetSuggestionsForCreditCards_CustomCardIcon) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url =
      GURL(card_has_capital_one_icon() ? kCapitalOneCardArtUrl
                                       : "https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  gfx::Image fake_image = CustomIconForTest();
  payments_data().AddServerCreditCard(server_card);
  payments_data().AddCardArtImage(card_art_url, fake_image);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
      kDefaultTriggerSource,
      /*should_show_scan_credit_card=*/false,
      /*should_show_cards_from_account=*/false, summary);

  // Suggestions in `suggestions` are persisted in order of their presentation
  // to the user in the Autofill dropdown and currently virtual cards are shown
  // before their associated FPAN suggestion.
  Suggestion virtual_card_suggestion = suggestions[0];
  Suggestion fpan_card_suggestion = suggestions[1];

  // Verify that for virtual cards, the custom icon is shown if the card art is
  // the Capital One virtual card art or if the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(virtual_card_suggestion, card_art_url,
                                          fake_image),
            card_has_capital_one_icon() || card_art_image_enabled());

  // Verify that for FPAN, the custom icon is shown if the card art is not the
  // Capital One virtual card art and the metadata card art is enabled.
  EXPECT_EQ(VerifyCardArtImageExpectation(fpan_card_suggestion, card_art_url,
                                          fake_image),
            !card_has_capital_one_icon() && card_art_image_enabled());
}

class PaymentsSuggestionGeneratorTestForOffer
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  PaymentsSuggestionGeneratorTestForOffer() {
#if BUILDFLAG(IS_ANDROID)
    keyboard_accessory_offer_enabled_ = GetParam();
    if (keyboard_accessory_offer_enabled_) {
      scoped_feature_keyboard_accessory_offer_.InitWithFeatures(
          {features::kAutofillEnableOffersInClankKeyboardAccessory}, {});
    } else {
      scoped_feature_keyboard_accessory_offer_.InitWithFeatures(
          {}, {features::kAutofillEnableOffersInClankKeyboardAccessory});
    }
#endif
  }
  ~PaymentsSuggestionGeneratorTestForOffer() override = default;

  bool keyboard_accessory_offer_enabled() {
#if BUILDFLAG(IS_ANDROID)
    return keyboard_accessory_offer_enabled_;
#else
    return false;
#endif
  }

#if BUILDFLAG(IS_ANDROID)
 private:
  bool keyboard_accessory_offer_enabled_;
  base::test::ScopedFeatureList scoped_feature_keyboard_accessory_offer_;
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         PaymentsSuggestionGeneratorTestForOffer,
                         testing::Bool());

// Test to make sure the suggestion gets populated with the right content if the
// card has card linked offer available.
TEST_P(PaymentsSuggestionGeneratorTestForOffer,
       CreateCreditCardSuggestion_ServerCardWithOffer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillEnableVirtualCardMetadata,
                             features::kAutofillEnableCardProductName,
                             features::kAutofillEnableCardArtImage});
  // Create a server card.
  CreditCard server_card1 =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/true);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));
  EXPECT_EQ(virtual_card_suggestion.labels.size(), 1u);

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/true);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));

  if (keyboard_accessory_offer_enabled()) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
    EXPECT_EQ(real_card_suggestion.feature_for_iph,
              &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
#endif
  } else {
    ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
    ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(real_card_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
  }
}

// Test to make sure the suggestion gets populated with the right content if the
// card has card linked offer available.
TEST_P(PaymentsSuggestionGeneratorTestForOffer,
       CreateCreditCardSuggestion_ServerCardWithOffer_MetadataEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableVirtualCardMetadata,
                            features::kAutofillEnableCardProductName,
                            features::kAutofillEnableCardArtImage},
      /*disabled_features=*/{});
  // Create a server card.
  CreditCard server_card1 =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/true);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));

  size_t expected_labels_size;
#if BUILDFLAG(IS_ANDROID)
  // For credit card number field, the expiration date is not shown as a
  // suggestion label when the virtual card metadata flag is enabled on Android
  // OS.
  expected_labels_size = 0;
#else
  expected_labels_size = 1;
#endif
  EXPECT_EQ(virtual_card_suggestion.labels.size(), expected_labels_size);

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/true);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::BackendId>(),
            Suggestion::BackendId(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")));

  if (keyboard_accessory_offer_enabled()) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
    EXPECT_EQ(real_card_suggestion.feature_for_iph,
              &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
#endif
  } else {
    ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
    ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(real_card_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
  }
}

}  // namespace autofill
