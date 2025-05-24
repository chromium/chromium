// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace autofill {
namespace {

using ::gfx::test::AreImagesEqual;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// A dot ("•") separator.
inline constexpr char16_t kEllipsisDotSeparator[] = u"\u2022";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

// This function is currently only used for BNPL unittests, and BNPL is
// currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
Matcher<Suggestion> EqualsSuggestion(const Suggestion& suggestion) {
  return AllOf(Field(&Suggestion::type, suggestion.type),
               Field(&Suggestion::main_text, suggestion.main_text),
               Field(&Suggestion::minor_texts, suggestion.minor_texts),
               Field(&Suggestion::icon, suggestion.icon),
               Field(&Suggestion::labels, suggestion.labels));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

Matcher<Suggestion> EqualsIbanSuggestion(
    const std::u16string& identifier_string,
    const Suggestion::Payload& payload,
    const std::u16string& nickname) {
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    if (nickname.empty()) {
      return AllOf(
          Field(&Suggestion::type, SuggestionType::kIbanEntry),
          Field(&Suggestion::main_text, Suggestion::Text(identifier_string)),
          Field(&Suggestion::payload, payload));
    }
    return AllOf(Field(&Suggestion::type, SuggestionType::kIbanEntry),
                 Field(&Suggestion::main_text, Suggestion::Text(nickname)),
                 Field(&Suggestion::minor_texts,
                       std::vector<Suggestion::Text>{
                           Suggestion::Text(identifier_string)}),
                 Field(&Suggestion::payload, payload));
  }
  if (nickname.empty()) {
    return AllOf(Field(&Suggestion::type, SuggestionType::kIbanEntry),
                 Field(&Suggestion::main_text,
                       Suggestion::Text(identifier_string,
                                        Suggestion::Text::IsPrimary(true))),
                 Field(&Suggestion::payload, payload));
  }
  return AllOf(
      Field(&Suggestion::type, SuggestionType::kIbanEntry),
      Field(&Suggestion::main_text,
            Suggestion::Text(nickname, Suggestion::Text::IsPrimary(true))),
      Field(&Suggestion::payload, payload),
      EqualLabels(std::vector<std::vector<Suggestion::Text>>{
          {Suggestion::Text(identifier_string)}}));
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
                     with_gpay_logo ? Suggestion::Icon::kGooglePay
                                    : Suggestion::Icon::kNoIcon));
#endif
}

auto ContainsCreditCardFooterSuggestions(bool with_gpay_logo) {
  return AllOf(
      SizeIs(Ge(2)),
      ResultOf(
          [](const auto& container) {
            return base::span(container).template last<2>();
          },
          ElementsAre(EqualsSuggestion(SuggestionType::kSeparator),
                      EqualsManagePaymentsMethodsSuggestion(with_gpay_logo))));
}

auto SuggestionWithGuidPayload(const Suggestion::Guid& guid) {
  return Property(&Suggestion::GetPayload<Suggestion::Guid>, guid);
}

class MockCreditCardFormEventLogger
    : public autofill_metrics::CreditCardFormEventLogger {
 public:
  using autofill_metrics::CreditCardFormEventLogger::CreditCardFormEventLogger;
  MOCK_METHOD(
      void,
      OnMetadataLoggingContextReceived,
      (autofill_metrics::CardMetadataLoggingContext metadata_logging_context),
      (override));
};

// TODO(crbug.com/40176273): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class PaymentsSuggestionGeneratorTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    payments_data().SetPrefService(autofill_client_.GetPrefs());
    payments_data().SetSyncServiceForTest(&sync_service_);
    autofill_client_.GetPaymentsAutofillClient()->set_autofill_offer_manager(
        std::make_unique<AutofillOfferManager>(
            &autofill_client_.GetPersonalDataManager()
                 .payments_data_manager()));
    credit_card_form_event_logger_ =
        std::make_unique<NiceMock<MockCreditCardFormEventLogger>>(
            &autofill_manager_);
  }

  void TearDown() override {
    if (did_set_up_image_resource_for_test_) {
      CleanUpIbanImageResources();
      did_set_up_image_resource_for_test_ = false;
    }
    credit_card_form_event_logger_->OnDestroyed();
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
          std::get_if<Suggestion::CustomIconUrl>(&suggestion.custom_icon);
      GURL url = custom_icon_url ? **custom_icon_url : GURL();
      return url == expected_url;
    } else {
      CHECK(std::holds_alternative<gfx::Image>(suggestion.custom_icon));
      return AreImagesEqual(std::get<gfx::Image>(suggestion.custom_icon),
                            expected_image);
    }
  }

  TestPaymentsDataManager& payments_data() {
    return autofill_client_.GetPersonalDataManager()
        .test_payments_data_manager();
  }

  const std::string& app_locale() { return payments_data().app_locale(); }

  TestAutofillClient* autofill_client() { return &autofill_client_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  TestAutofillDriver autofill_driver_{&autofill_client_};
  TestBrowserAutofillManager autofill_manager_{&autofill_driver_};

 protected:
  testing::NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
  // Tracks whether SetUpIbanImageResources() has been called, so that the
  // created images can be cleaned up when the test has finished.
  bool did_set_up_image_resource_for_test_ = false;
  std::unique_ptr<MockCreditCardFormEventLogger> credit_card_form_event_logger_;
};

// The card benefits label generation currently varies across operating systems.
// On iOS, the label is not displayed at present. For Desktop, the benefit is
// shown along with a "terms apply" message (e.g., "5% cash back on all
// purchases (terms apply)"). On Android(Clank), the benefits label is displayed
// without the "(terms apply)" message (e.g., "5% cash back on all purchases").
// Therefore, it is necessary to separate the tests for these methods based on
// the specific operating system.
#if !BUILDFLAG(IS_IOS)
// TODO(crbug.com/325646493): Clean up
// PaymentsSuggestionGeneratorTest.AutofillCreditCardBenefitsLabelTest setup and
// parameters.
// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Whether the flag to sync benefits source is enabled.
// 3. Issuer ID which is set for the credit card with benefits.
// 4. Benefit source which is set for the credit card with benefits.
class AutofillCreditCardBenefitsLabelTest
    : public PaymentsSuggestionGeneratorTest,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>,
                     bool,
                     std::string,
                     std::string>> {
 public:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kAutofillEnableCardBenefitsForAmericanExpress, true},
         {features::kAutofillEnableCardBenefitsForBmo, true},
         {features::kAutofillEnableFlatRateCardBenefitsFromCurinos, true},
         {features::kAutofillEnableCardBenefitsIph, true},
         {features::kAutofillEnableNewFopDisplayDesktop, true},
         {features::kAutofillEnableCardBenefitsSourceSync,
          IsCreditCardBenefitsSourceSyncEnabled()}});

    std::u16string benefit_description;
    int64_t instrument_id;

    if (std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
      CreditCardFlatRateBenefit benefit =
          std::get<CreditCardFlatRateBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else if (std::holds_alternative<CreditCardMerchantBenefit>(
                   GetBenefit())) {
      CreditCardMerchantBenefit benefit =
          std::get<CreditCardMerchantBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
      // Set the page URL in order to ensure that the merchant benefit is
      // displayed.
      autofill_client()->set_last_committed_primary_main_frame_url(
          benefit.merchant_domains().begin()->GetURL());
    } else if (std::holds_alternative<CreditCardCategoryBenefit>(
                   GetBenefit())) {
      ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                  autofill_client()->GetAutofillOptimizationGuide()),
              AttemptToGetEligibleCreditCardBenefitCategory)
          .WillByDefault(testing::Return(
              CreditCardCategoryBenefit::BenefitCategory::kSubscription));
      CreditCardCategoryBenefit benefit =
          std::get<CreditCardCategoryBenefit>(GetBenefit());
      payments_data().AddCreditCardBenefitForTest(benefit);
      benefit_description = benefit.benefit_description();
      instrument_id = *benefit.linked_card_instrument_id();
    } else {
      NOTREACHED();
    }

#if !BUILDFLAG(IS_ANDROID)
    expected_benefit_text_ = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS,
        benefit_description);
#else
    expected_benefit_text_ = benefit_description;
#endif  // !BUILDFLAG(IS_ANDROID)
    card_ = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1",
        /*instrument_id=*/instrument_id);
    if (IsCreditCardBenefitsSourceSyncEnabled()) {
      card_.set_benefit_source(GetBenefitSource());
    } else {
      card_.set_issuer_id(GetIssuerId());
    }
    payments_data().AddServerCreditCard(card_);
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  bool IsCreditCardBenefitsSourceSyncEnabled() const {
    return std::get<1>(GetParam());
  }

  const std::string& GetIssuerId() const { return std::get<2>(GetParam()); }

  const std::string& GetBenefitSource() const {
    return std::get<3>(GetParam());
  }

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
        expected_instrument_ids_to_available_benefit_sources = {
            {card().instrument_id(), card().benefit_source()}};
    EXPECT_EQ(
        metadata_logging_context.instrument_ids_to_available_benefit_sources,
        expected_instrument_ids_to_available_benefit_sources);
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
                     ::testing::Bool(),
                     ::testing::Values("amex", "bmo"),
                     ::testing::Values("amex", "bmo", "curinos")));

#if !BUILDFLAG(IS_ANDROID)
// Checks that for FPAN suggestions that the benefit description is displayed.
TEST_P(AutofillCreditCardBenefitsLabelTest, BenefitSuggestionLabel_Fpan) {
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_TRUE(suggestion.labels.empty());
  } else {
    EXPECT_THAT(suggestion.labels,
                ElementsAre(std::vector<Suggestion::Text>{
                    Suggestion::Text(expected_benefit_text())}));
  }
}

// Checks that feature is set to display the credit card benefit IPH for
// FPAN suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_Fpan) {
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_EQ(suggestion.iph_metadata.feature, nullptr);
  } else {
    EXPECT_EQ(suggestion.iph_metadata.feature,
              &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
  }
}

// Checks that feature is set to display the virtual card IPH for
// virtual card suggestions with benefits labels.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_VirtualCard) {
  EXPECT_EQ(CreateCreditCardSuggestionForTest(
                card(), *autofill_client(), CREDIT_CARD_NUMBER,
                /*virtual_card_option=*/true,
                /*card_linked_offer_available=*/false)
                .iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
}

// Checks that `feature` is set to null when the flag is off.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_IsNullWhenFlagIsDisabled) {
  base::test::ScopedFeatureList disable_benefits_iph;
  disable_benefits_iph.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillEnableCardBenefitsIph});
  EXPECT_EQ(CreateCreditCardSuggestionForTest(
                card(), *autofill_client(), CREDIT_CARD_NUMBER,
                /*virtual_card_option=*/false,
                /*card_linked_offer_available=*/false)
                .iph_metadata.feature,
            nullptr);
}

// Checks that `feature` is set to null when the card is not eligible
// for the benefits.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_IsNullWhenCardNotEligibleForBenefits) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockBenefitSuggestionLabelsForCardAndUrl)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(CreateCreditCardSuggestionForTest(
                card(), *autofill_client(), CREDIT_CARD_NUMBER,
                /*virtual_card_option=*/false,
                /*card_linked_offer_available=*/false)
                .iph_metadata.feature,
            nullptr);
}

// Checks that for virtual cards suggestion the benefit description is shown
// as a label.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard) {
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_TRUE(suggestion.labels.empty());
  } else {
    EXPECT_THAT(suggestion.labels,
                ElementsAre(std::vector<Suggestion::Text>{
                    Suggestion::Text(expected_benefit_text())}));
  }
}

// Checks that for merchant opt-out virtual cards suggestion the benefit
// description is not shown.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard_MerchantOptOut) {
  CreditCard virtual_card = CreditCard::CreateVirtualCard(card());
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(virtual_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false)
          .labels,
      ElementsAre(std::vector<Suggestion::Text>{
          Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE))}));
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
          features::kAutofillEnableCardBenefitsForBmo,
          features::kAutofillEnableFlatRateCardBenefitsFromCurinos});
  DoBenefitSuggestionLabel_MetadataLoggingContextTest();
}

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_MerchantUrlIsDifferent) {
  if (!std::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-merchant benefits.";
  }
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // Merchant benefit description is not returned.
  EXPECT_TRUE(suggestion.labels.empty());
}

// Checks that the category benefit description is not displayed for suggestions
// where the webpage's category in the optimization guide is different from the
// benefit's applicable category.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_CategoryIsDifferent) {
  if (!std::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-category benefits.";
  }

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // Category benefit description is not returned.
  EXPECT_TRUE(suggestion.labels.empty());
}

// Checks that the benefit description is not displayed when benefit suggestions
// are disabled for the given card and url.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_BlockedUrl) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockBenefitSuggestionLabelsForCardAndUrl)
      .WillByDefault(testing::Return(true));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  // Benefit description is not returned.
  EXPECT_TRUE(suggestion.labels.empty());
}

// Checks that for FPAN suggestions that the benefit description is displayed.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_Fpan_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_THAT(suggestion.labels,
                ElementsAre(std::vector<Suggestion::Text>{Suggestion::Text(
                    card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                   /*app_locale=*/"en-US"))}));
  } else {
    EXPECT_THAT(
        suggestion.labels,
        ElementsAre(
            std::vector<Suggestion::Text>{
                Suggestion::Text(expected_benefit_text())},
            std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
                CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
  }
}

// Checks that for virtual cards suggestion the benefit description is shown
// with a virtual card label appended.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_THAT(suggestion.labels,
                ElementsAre(std::vector<Suggestion::Text>{
                    Suggestion::Text(l10n_util::GetStringUTF16(
                        IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))}));
  } else {
    EXPECT_THAT(
        suggestion.labels,
        ElementsAre(
            std::vector<Suggestion::Text>{
                Suggestion::Text(expected_benefit_text())},
            std::vector<Suggestion::Text>{
                Suggestion::Text(l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))}));
  }
}

// Checks that for merchant opt-out virtual cards suggestion the benefit
// description is not shown.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard_MerchantOptOut_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard virtual_card = CreditCard::CreateVirtualCard(card());
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      virtual_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_THAT(
        suggestion.labels,
        ElementsAre(std::vector<Suggestion::Text>{
            Suggestion::Text(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE))}));
  } else {
    EXPECT_THAT(
        suggestion.labels,
        ElementsAre(
            std::vector<Suggestion::Text>{
                Suggestion::Text(expected_benefit_text())},
            std::vector<
                Suggestion::Text>{Suggestion::Text(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE))}));
  }
}

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(
    AutofillCreditCardBenefitsLabelTest,
    BenefitSuggestionLabelNotDisplayed_MerchantUrlIsDifferent_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  if (!std::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
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
      ElementsAre(std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that the category benefit description is not displayed for suggestions
// where the webpage's category in the optimization guide is different from the
// benefit's applicable category.
TEST_P(
    AutofillCreditCardBenefitsLabelTest,
    BenefitSuggestionLabelNotDisplayed_CategoryIsDifferent_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  if (!std::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
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
      ElementsAre(std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

// Checks that the benefit description is not displayed when benefit suggestions
// are disabled for the given card and url.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_BlockedUrl_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
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
      ElementsAre(std::vector<Suggestion::Text>{Suggestion::Text(card().GetInfo(
          CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, /*app_locale=*/"en-US"))}));
}

#else

TEST_P(AutofillCreditCardBenefitsLabelTest,
       GetCreditCardSuggestionsForTouchToFill_BenefitsAdded_RealCard) {
  std::vector<CreditCard> cards = {card()};
  base::flat_map<int64_t, std::string>
      expected_instrument_ids_to_available_benefit_sources = {
          {cards[0].instrument_id(), cards[0].benefit_source()}};
  EXPECT_CALL(*credit_card_form_event_logger_,
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_THAT(suggestions[0],
                EqualLabels({{card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                             app_locale())}}));
    EXPECT_FALSE(suggestions[0]
                     .GetPayload<Suggestion::PaymentsPayload>()
                     .should_display_terms_available);
  } else {
    EXPECT_THAT(suggestions[0],
                EqualLabels({{expected_benefit_text()},
                             {card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                             app_locale())}}));
    EXPECT_TRUE(suggestions[0]
                    .GetPayload<Suggestion::PaymentsPayload>()
                    .should_display_terms_available);
  }
}

TEST_P(AutofillCreditCardBenefitsLabelTest,
       GetCreditCardSuggestionsForTouchToFill_BenefitsAdded_VirtualCard) {
  CreditCard virtual_card = CreditCard::CreateVirtualCard(card());
  std::vector<CreditCard> cards = {virtual_card};
  base::flat_map<int64_t, std::string>
      expected_instrument_ids_to_available_benefit_sources = {
          {cards[0].instrument_id(), cards[0].benefit_source()}};
  EXPECT_CALL(*credit_card_form_event_logger_,
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  EXPECT_EQ(suggestions[0].type, SuggestionType::kVirtualCreditCardEntry);
  if (IsCreditCardBenefitsSourceSyncEnabled() &&
      GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_THAT(suggestions[0],
                EqualLabels({{l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)}}));
    EXPECT_FALSE(suggestions[0]
                     .GetPayload<Suggestion::PaymentsPayload>()
                     .should_display_terms_available);
  } else {
    EXPECT_THAT(
        suggestions[0],
        EqualLabels({{expected_benefit_text()},
                     {l10n_util::GetStringUTF16(
                         IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)}}));
    EXPECT_TRUE(suggestions[0]
                    .GetPayload<Suggestion::PaymentsPayload>()
                    .should_display_terms_available);
  }
}

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(
    AutofillCreditCardBenefitsLabelTest,
    GetCreditCardSuggestionsForTouchToFill_BenefitsNotAdded_NonApplicableUrl) {
  if (!std::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-merchant benefits.";
  }
  autofill_client()->set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  // Merchant benefit description is not returned.
  EXPECT_THAT(suggestions[0],
              EqualLabels({{card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                           app_locale())}}));
  EXPECT_FALSE(suggestions[0]
                   .GetPayload<Suggestion::PaymentsPayload>()
                   .should_display_terms_available);
}

// Checks that the category benefit description is not displayed for suggestions
// where the webpage's category in the optimization guide is different from the
// benefit's applicable category.
TEST_P(
    AutofillCreditCardBenefitsLabelTest,
    GetCreditCardSuggestionsForTouchToFill_BenefitsNotAdded_DifferentCategory) {
  if (!std::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-category benefits.";
  }

  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  // Category benefit description is not returned.
  EXPECT_THAT(suggestions[0],
              EqualLabels({{card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                           app_locale())}}));
  EXPECT_FALSE(suggestions[0]
                   .GetPayload<Suggestion::PaymentsPayload>()
                   .should_display_terms_available);
}

// Checks that the benefit description is not displayed when benefit suggestions
// are disabled for the given card and url.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       GetCreditCardSuggestionsForTouchToFill_BenefitsNotAdded_BlockedUrl) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
              autofill_client()->GetAutofillOptimizationGuide()),
          ShouldBlockBenefitSuggestionLabelsForCardAndUrl)
      .WillByDefault(testing::Return(true));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  // Benefit description is not returned.
  EXPECT_THAT(suggestions[0],
              EqualLabels({{card().GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                           app_locale())}}));
  EXPECT_FALSE(suggestions[0]
                   .GetPayload<Suggestion::PaymentsPayload>()
                   .should_display_terms_available);
}

TEST_P(
    AutofillCreditCardBenefitsLabelTest,
    GetCreditCardSuggestionsForTouchToFill_OnMetadataLoggingContextReceivedCalled) {
  std::vector<CreditCard> cards = {card(),
                                   CreditCard::CreateVirtualCard(card())};
  base::flat_map<int64_t, std::string>
      expected_instrument_ids_to_available_benefit_sources = {
          {cards[0].instrument_id(), cards[0].benefit_source()},
          {cards[1].instrument_id(), cards[1].benefit_source()}};
  EXPECT_CALL(*credit_card_form_event_logger_,
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  GetCreditCardSuggestionsForTouchToFill(cards, *autofill_client(),
                                         *credit_card_form_event_logger_);
}

#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_IOS)

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
        credit_card.usage_history().set_use_date(is_disused ? kDisuseTime
                                                            : kNow);
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
      /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);

  // Expect that only the last card (disused, expired and local) is removed.
  credit_cards.pop_back();
  EXPECT_THAT(cards_to_suggest, UnorderedElementsAreArray(credit_cards));

  constexpr char kHistogramName[] = "Autofill.CreditCardsSuppressedForDisuse";
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

// Tests that credit card suggestions are not subject to prefix matching for the
// credit card number.
TEST_F(PaymentsSuggestionGeneratorTest, NoPrefixMatchingForCreditCards) {
  CreditCard card1 = test::GetCreditCard();
  card1.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(card1);
  CreditCard card2 = test::GetCreditCard2();
  card2.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  payments_data().AddServerCreditCard(card2);

  auto get_cards = [&](std::u16string field_value) {
    FormFieldData field;
    field.set_value(std::move(field_value));
    return GetOrderedCardsToSuggestForTest(
        *autofill_client(), field, CREDIT_CARD_NUMBER,
        /*suppress_disused_cards=*/false,
        /*prefix_match=*/true,
        /*require_non_empty_value_on_trigger_field=*/true,
        /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u""), UnorderedElementsAre(card1, card2));

  ASSERT_NE(card1.number(), card2.number());
  EXPECT_THAT(get_cards(card1.number()), UnorderedElementsAre(card1, card2));

  EXPECT_THAT(get_cards(card2.number()), UnorderedElementsAre(card1, card2));
}

// Tests that credit card suggestions are not subject to prefix matching for the
// CVC.
TEST_F(PaymentsSuggestionGeneratorTest,
       NoPrefixMatchingForCvcsIfFeatureIsTurnedOn) {
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
    return GetOrderedCardsToSuggestForTest(
        *autofill_client(), field, CREDIT_CARD_VERIFICATION_CODE,
        /*suppress_disused_cards=*/false,
        /*prefix_match=*/true,
        /*require_non_empty_value_on_trigger_field=*/true,
        /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u""), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"1"), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"2"), ElementsAre(credit_card));
}

#if !BUILDFLAG(IS_IOS)
// Tests that all the credit card suggestions are shown when a credit card field
// was autofilled and focused if `kAutofillPaymentsFieldSwapping` is enabled.
TEST_F(PaymentsSuggestionGeneratorTest, PaymentsFieldSwapping) {
  base::test::ScopedFeatureList features{
      features::kAutofillPaymentsFieldSwapping};
  CreditCard credit_card1;
  test::SetCreditCardInfo(&credit_card1, /*name_on_card=*/"Cardholder name",
                          /*card_number=*/"1111222233334444",
                          /*expiration_month=*/nullptr,
                          /*expiration_year*/ nullptr,
                          /*billing_address_id=*/"", /*cvc=*/u"123");
  credit_card1.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(credit_card1);

  CreditCard credit_card2;
  test::SetCreditCardInfo(&credit_card2, /*name_on_card=*/"Cardholder name",
                          /*card_number=*/"4444333322221111",
                          /*expiration_month=*/nullptr,
                          /*expiration_year*/ nullptr,
                          /*billing_address_id=*/"", /*cvc=*/u"123");
  credit_card2.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(credit_card2);

  auto get_cards = [&](std::u16string field_value, FieldType type,
                       bool is_autofilled) {
    FormFieldData field;
    field.set_is_autofilled(is_autofilled);
    field.set_value(std::move(field_value));
    return GetOrderedCardsToSuggestForTest(
        *autofill_client(), field, type,
        /*suppress_disused_cards=*/false,
        /*prefix_match=*/true,
        /*require_non_empty_value_on_trigger_field=*/true,
        /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u"", CREDIT_CARD_NUMBER, false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1111222233334444", CREDIT_CARD_NUMBER, true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"4444333322221111", CREDIT_CARD_NUMBER, true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1111222233334444", CREDIT_CARD_NUMBER, false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1", CREDIT_CARD_NUMBER, true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"", CREDIT_CARD_NAME_FULL, false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"Cardholder name", CREDIT_CARD_NAME_FULL, false),
              ElementsAre(credit_card2, credit_card1));
}
#endif  // !BUILDFLAG(IS_IOS)

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
       GetCreditCardOrCvcFieldSuggestions_StableSortBasedOnOffer) {
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
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  EXPECT_TRUE(summary.with_offer);
  ASSERT_EQ(suggestions.size(), 5U);
  // The suggestion with card linked offer available should be ranked to the
  // top.
  EXPECT_EQ(suggestions[0].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000002"));
  // The other suggestions should have their relative ranking unchanged.
  EXPECT_EQ(suggestions[1].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000003"));
  EXPECT_EQ(suggestions[2].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Ensures we appropriately generate suggestions for virtual cards on a
// standalone CVC field.
TEST_F(PaymentsSuggestionGeneratorTest,
       GetVirtualCardStandaloneCvcFieldSuggestions) {
  CreditCard server_card = CreateServerCard();
  payments_data().AddServerCreditCard(server_card);

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"1234")});
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  std::vector<Suggestion> suggestions =
      GetVirtualCardStandaloneCvcFieldSuggestions(
          *autofill_client(), FormFieldData(), metadata_logging_context,
          virtual_card_guid_to_last_four_map);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsSuggestionGeneratorTest,
       GetVirtualCardStandaloneCvcFieldSuggestions_UndoAutofill) {
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
      GetVirtualCardStandaloneCvcFieldSuggestions(
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
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(summary.with_cvc);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Ensures we appropriately generate suggestions for card info retrieval
// enrolled card.
TEST_F(PaymentsSuggestionGeneratorTest,
       GetCardSuggestionsWithCardInfoRetrievalEnrolled) {
  CreditCard card = test::GetMaskedServerCardEnrolledIntoRuntimeRetrieval();
  payments_data().AddServerCreditCard(card);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  // There are 3 suggestions, 1 for card info retrieval enrolled card
  // suggestions, followed by a separator, and followed by "Manage payment
  // methods..." which redirects to the Chrome payment methods settings page.
  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(summary.with_card_info_retrieval_enrolled);
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
    std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
        *autofill_client(), FormFieldData(),
        /*four_digit_combinations_in_dom=*/{},
        /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
        CREDIT_CARD_NUMBER,
        /*should_show_scan_credit_card=*/false, summary);

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
    std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
        *autofill_client(), FormFieldData(),
        /*four_digit_combinations_in_dom=*/{},
        /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
        CREDIT_CARD_NUMBER,
        /*should_show_scan_credit_card=*/false, summary);

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
    local_card.usage_history().set_use_date(AutofillClock::Now() -
                                            base::Days(365));
    payments_data().AddCreditCard(local_card);
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    CreditCardSuggestionSummary summary;
    std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
        *autofill_client(), FormFieldData(),
        /*four_digit_combinations_in_dom=*/{},
        /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
        CREDIT_CARD_NUMBER,
        /*should_show_scan_credit_card=*/false, summary);

    EXPECT_EQ(suggestions.size(), 3U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }
}

TEST_F(PaymentsSuggestionGeneratorTest, NoSuggestionsWhenNoUserData) {
  FormFieldData field;
  field.set_is_autofilled(true);
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/true, summary);

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(PaymentsSuggestionGeneratorTest, ShouldShowScanCreditCard) {
  payments_data().AddCreditCard(test::GetCreditCard());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/true, summary);

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

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsSuggestionGeneratorTest,
       FieldWasAutofilled_UndoAutofillOnCreditCardForm) {
  payments_data().AddCreditCard(test::GetCreditCard());
  FormFieldData field;
  field.set_is_autofilled(true);
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

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

// Ensures that all suggestions generated by `GetCreditCardFooterSuggestions`
// can be recognized by `IsCreditCardFooterSuggestion.
// False should be returned if the index is out of range.
TEST_F(PaymentsSuggestionGeneratorTest, IsCreditCardFooterSuggestion) {
  std::vector<Suggestion> footer_suggestions =
      GetCreditCardFooterSuggestionsForTest(
          /*should_show_scan_credit_card=*/true, /*is_autofilled=*/true,
          /*with_gpay_logo=*/true);

  for (size_t index = 0; index < footer_suggestions.size(); index++) {
    EXPECT_TRUE(IsCreditCardFooterSuggestion(footer_suggestions, index));
  }

  EXPECT_FALSE(IsCreditCardFooterSuggestion(footer_suggestions,
                                            footer_suggestions.size()));
}

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
class PaymentsSuggestionGeneratorBnplTest
    : public PaymentsSuggestionGeneratorTest {
 public:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableBuyNowPayLaterSyncing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that the pay over time option is generated with expected content
// and inserted as the last entry before the footer suggestions.
TEST_F(PaymentsSuggestionGeneratorBnplTest, MaybeUpdateSuggestionsWithBnpl) {
  // Add a server card with vcn enrolled.
  payments_data().AddServerCreditCard(
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber());
  // Add a server card with vcn not enrolled.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add a local card.
  payments_data().AddCreditCard(test::GetCreditCard());

  // Add BNPL issuers.
  payments_data().AddBnplIssuer(BnplIssuer(
      /*instrument_id=*/1234, BnplIssuer::IssuerId::kBnplZip,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/50'000'000,
                                      /*price_upper_bound=*/200'000'000)}));
  payments_data().AddBnplIssuer(BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'000'000,
                                      /*price_upper_bound=*/200'000'000)}));

  std::vector<CreditCard> ordered_cards_for_suggestions =
      GetOrderedCardsToSuggestForTest(
          *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
          /*suppress_disused_cards=*/false,
          /*prefix_match=*/false,
          /*require_non_empty_value_on_trigger_field=*/false,
          /*include_virtual_cards=*/true);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      {}, CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  uint64_t extracted_amount_in_micros = 50'000'000;

  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateSuggestionsWithBnpl(suggestions,
                                     payments_data().GetBnplIssuers(),
                                     extracted_amount_in_micros);

  // `updated_suggesions` should contains 7 suggestions which are 4 credit
  // card suggestions, 1 BNPL suggestion, 1 separator, and 1 manage card
  // footer.
  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  std::vector<Suggestion>& updated_suggestions =
      update_suggestions_result.suggestions;
  ASSERT_EQ(updated_suggestions.size(), 7U);
  size_t current_suggestion_index = 0;
  // Checks card suggestions stayed in the same order after the insertion.
  for (const CreditCard& card : ordered_cards_for_suggestions) {
    EXPECT_THAT(updated_suggestions[current_suggestion_index++],
                EqualsSuggestion(CreateCreditCardSuggestionForTest(
                    card, *autofill_client(), CREDIT_CARD_NUMBER,
                    /*virtual_card_option=*/card.record_type() ==
                        CreditCard::RecordType::kVirtualCard,
                    /*card_linked_offer_available=*/false)));
  }

  // Checks BNPL suggestion is inserted.
  EXPECT_EQ(updated_suggestions[current_suggestion_index]
                .GetPayload<Suggestion::PaymentsPayload>()
                .extracted_amount_in_micros,
            extracted_amount_in_micros);
  EXPECT_THAT(
      updated_suggestions[current_suggestion_index++],
      EqualsSuggestion(
          SuggestionType::kBnplEntry,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_MAIN_TEXT),
          Suggestion::Icon::kBnpl,
          {{Suggestion::Text(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL, u"$34"))}}));

  // Checks the footer suggestions stayed in the same order after the insertion.
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              EqualsSuggestion(SuggestionType::kSeparator));
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              EqualsSuggestion(SuggestionType::kManageCreditCard));
  EXPECT_EQ(current_suggestion_index, updated_suggestions.size());
}

// Ensures that `GetBnplPriceLowerBound()` returns the minimum lower price
// bound among all given issuers.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_ReturnLowerAmount) {
  std::vector<BnplIssuer> bnpl_issuers = {
      BnplIssuer(
          /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
          {BnplIssuer::EligiblePriceRange("USD",
                                          /*price_lower_bound=*/34'666'666,
                                          /*price_upper_bound=*/200'000'000)}),
      BnplIssuer(
          /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplZip,
          {BnplIssuer::EligiblePriceRange("USD",
                                          /*price_lower_bound=*/34'000'000,
                                          /*price_upper_bound=*/200'000'000)}),
      BnplIssuer(
          /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAfterpay,
          {BnplIssuer::EligiblePriceRange("USD",
                                          /*price_lower_bound=*/22'000'000,
                                          /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$22");
}

// Ensures that `GetBnplPriceLowerBound()` returns the minimum lower price
// bound in USD among all given issuers.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_ReturnLowerAmountInUsd) {
  std::vector<BnplIssuer> bnpl_issuers = {
      BnplIssuer(
          /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
          {BnplIssuer::EligiblePriceRange("USD",
                                          /*price_lower_bound=*/34'000'000,
                                          /*price_upper_bound=*/200'000'000),
           BnplIssuer::EligiblePriceRange("GBP",
                                          /*price_lower_bound=*/20'000'000,
                                          /*price_upper_bound=*/200'000'000)}),
      BnplIssuer(
          /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAfterpay,
          {BnplIssuer::EligiblePriceRange("USD",
                                          /*price_lower_bound=*/22'000'000,
                                          /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$22");
}

// Ensures that `GetBnplPriceLowerBound()` returns the minimum lower price bound
// that is in whole currency unit in integer format.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_AmountInInteger) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'000'000,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$34");
}

#if defined(GTEST_HAS_DEATH_TEST)
// Ensures that the CHECK in `GetBnplPriceLowerBound()` catches the case when
// no BNPL issuer has eligible price range in USD.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_NoMatchingPriceRange) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("GBP",
                                      /*price_lower_bound=*/34'000'000,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_DEATH(GetBnplPriceLowerBoundForTest(bnpl_issuers), "");
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

// Ensures that `GetBnplPriceLowerBound` returns the minimum lower price bound
// that has more than 2 decimal points with proper rounding.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_AmountWithMoreThanTwoDecimal) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'666'666,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$34.67");
}

// Ensures that `GetBnplPriceLowerBound` returns the minimum lower price bound
// that has single digit cents value with 0 after the decimal point.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_AmountWithSingleDigitCents) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'070'000,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$34.07");
}

// Ensures that `GetBnplPriceLowerBound` returns the rounded up minimum lower
// price bound if the amount has cents value higher than 99.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_AmountWithMoreThanNintyNineCents) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'996'666,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$35");
}

// Ensures that the pay over time option is not added if the suggestion list
// is empty.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       MaybeUpdateSuggestionsWithBnpl_EmptySuggestionList) {
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  EXPECT_FALSE(
      MaybeUpdateSuggestionsWithBnpl({}, payments_data().GetBnplIssuers(),
                                     /*extracted_amount_in_micros=*/50'000'000)
          .is_bnpl_suggestion_added);
}

// Ensures that the pay over time option is not added if the suggestion list
// already contains a BNPL suggestion.
TEST_F(PaymentsSuggestionGeneratorBnplTest,
       MaybeUpdateSuggestionsWithBnpl_SuggestionListWithBnplInserted) {
  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Add suggestions with BNPL suggestion inserted to client.
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      {}, CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);
  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateSuggestionsWithBnpl(suggestions,
                                     payments_data().GetBnplIssuers(),
                                     /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_THAT(update_suggestions_result.suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kBnplEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
  EXPECT_FALSE(
      MaybeUpdateSuggestionsWithBnpl(update_suggestions_result.suggestions,
                                     payments_data().GetBnplIssuers(),
                                     /*extracted_amount_in_micros=*/50'000'000)
          .is_bnpl_suggestion_added);
}

TEST_F(PaymentsSuggestionGeneratorBnplTest,
       MaybeUpdateSuggestionsWithBnpl_IphBubble) {
  // Add a server card.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add BNPL issuers.
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      {}, CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);
  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateSuggestionsWithBnpl(suggestions,
                                     payments_data().GetBnplIssuers(),
                                     /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  Suggestion bnpl_suggestion = *std::ranges::find_if(
      update_suggestions_result.suggestions,
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  EXPECT_EQ(bnpl_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillBnplAffirmOrZipSuggestionFeature);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

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

TEST_F(PaymentsSuggestionGeneratorTest,
       GetPromoCodeSuggestionsFromPromoCodeOffers_ValidPromoCodes) {
  std::vector<const AutofillOfferData*> promo_code_offers;

  base::Time expiry = AutofillClock::Now() + base::Days(2);
  std::vector<GURL> merchant_origins;
  DisplayStrings display_strings;
  display_strings.value_prop_text = "test_value_prop_text_1";
  std::string promo_code = "test_promo_code_1";
  AutofillOfferData offer1 = AutofillOfferData::GPayPromoCodeOffer(
      /*offer_id=*/1, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings, promo_code);

  promo_code_offers.push_back(&offer1);

  DisplayStrings display_strings2;
  display_strings2.value_prop_text = "test_value_prop_text_2";
  std::string promo_code2 = "test_promo_code_2";
  AutofillOfferData offer2 = AutofillOfferData::GPayPromoCodeOffer(
      /*offer_id=*/2, expiry, merchant_origins,
      /*offer_details_url=*/GURL("https://offer-details-url.com/"),
      display_strings2, promo_code2);

  promo_code_offers.push_back(&offer2);

  std::vector<Suggestion> promo_code_suggestions =
      GetPromoCodeSuggestionsFromPromoCodeOffers(promo_code_offers);
  EXPECT_TRUE(promo_code_suggestions.size() == 4);

  EXPECT_EQ(promo_code_suggestions[0].main_text.value, u"test_promo_code_1");
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("1"));
  EXPECT_THAT(promo_code_suggestions[0],
              EqualLabels({{u"test_value_prop_text_1"}}));
  EXPECT_EQ(promo_code_suggestions[0].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("1"));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);
  EXPECT_EQ(promo_code_suggestions[1].main_text.value, u"test_promo_code_2");
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("2"));
  EXPECT_THAT(promo_code_suggestions[1],
              EqualLabels({{u"test_value_prop_text_2"}}));
  EXPECT_EQ(promo_code_suggestions[1].GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("2"));
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
  EXPECT_FALSE(std::holds_alternative<GURL>(promo_code_suggestions[0].payload));
  EXPECT_EQ(promo_code_suggestions[0].type,
            SuggestionType::kMerchantPromoCodeEntry);
}

TEST_F(PaymentsSuggestionGeneratorTest,
       GenerateLocalSaveAndFillSuggestion_CreditCardUploadDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/false);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  // `suggestions` should contain 3 suggestions which are save and fill
  // suggestion, separator, and manage cards footer.
  ASSERT_GE(suggestions.size(), 3ul);
  EXPECT_THAT(
      suggestions[0],
      EqualsSuggestion(
          SuggestionType::kSaveAndFillCreditCardEntry,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
          Suggestion::Icon::kSaveAndFill,
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_LOCAL_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}}));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

TEST_F(PaymentsSuggestionGeneratorTest,
       GenerateServerSaveAndFillSuggestion_CreditCardUploadEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/true);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  // `suggestions` should contain 3 suggestions which are save and fill
  // suggestion, separator, and manage cards footer.
  ASSERT_GE(suggestions.size(), 3ul);
  EXPECT_THAT(
      suggestions[0],
      EqualsSuggestion(
          SuggestionType::kSaveAndFillCreditCardEntry,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
          Suggestion::Icon::kSaveAndFill,
          {{Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_SERVER_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}}));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

TEST_F(PaymentsSuggestionGeneratorTest,
       GenerateLocalSaveAndFillSuggestion_FlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSaveAndFill);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  ASSERT_GE(suggestions.size(), 0ul);
}

TEST_F(PaymentsSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenCreditCardIsSavedInProfile) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);

  payments_data().AddCreditCard(test::GetCreditCard());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_EQ(suggestions.size(), 3ul);
  EXPECT_THAT(suggestions[0],
              EqualsSuggestion(SuggestionType::kCreditCardEntry));
}

TEST_F(PaymentsSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenCreditCardFormIsIncomplete) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(PaymentsSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenIncognito) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
  autofill_client()->set_is_off_the_record(true);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_TRUE(suggestions.empty());
}

TEST_F(PaymentsSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenFieldHasMoreThanThreeChars) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);

  CreditCardSuggestionSummary summary;
  FormFieldData field;
  field.set_value(u"1234");
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field, CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_TRUE(suggestions.empty());
}

// Verify "Save and Fill" suggestion is offered when the user is not in
// incognito mode, has no saved credit cards, the credit card form is complete
// (has CVC, cardholder name, card number, and expiration date fields), and a
// field within the form group is focused with no more than 3 characters
// entered.
TEST_F(PaymentsSuggestionGeneratorTest,
       SaveAndFillSuggestion_OfferedWhenCriteriaMet) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/true);

  // Verify user is not in incognito mode.
  ASSERT_FALSE(autofill_client()->IsOffTheRecord());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER, summary,
      /*is_complete_form=*/true,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kSaveAndFillCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

// This class helps test the IBAN suggestion contents that are displayed in
// Autofill suggestions.
class AutofillIbanSuggestionContentTest
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillIbanSuggestionContentTest() {
    feature_list_metadata_.InitWithFeatureStates(
        {{features::kAutofillEnableNewFopDisplayDesktop,
          IsNewFopDisplayEnabled()}});
  }

  ~AutofillIbanSuggestionContentTest() override = default;

  void SetUpIbanImageResources() {
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &mock_resource_delegate_,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
    if (IsNewFopDisplayEnabled()) {
      ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN))
          .WillByDefault(testing::Return(CustomIconForTest()));
    } else {
      ON_CALL(mock_resource_delegate_, GetImageNamed(IDR_AUTOFILL_IBAN_OLD))
          .WillByDefault(testing::Return(CustomIconForTest()));
    }
    did_set_up_image_resource_for_test_ = true;
  }

  bool IsNewFopDisplayEnabled() const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return false;
#else
    return GetParam();
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_metadata_;
};

INSTANTIATE_TEST_SUITE_P(PaymentsSuggestionGeneratorTest,
                         AutofillIbanSuggestionContentTest,
                         ::testing::Bool());

TEST_P(AutofillIbanSuggestionContentTest, GetLocalIbanSuggestions) {
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

TEST_P(AutofillIbanSuggestionContentTest, GetServerIbanSuggestions) {
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

  EXPECT_THAT(iban_suggestions[0],
              EqualsIbanSuggestion(
                  server_iban1.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban1.instrument_id()),
                  server_iban1.nickname()));

  EXPECT_THAT(iban_suggestions[1],
              EqualsIbanSuggestion(
                  server_iban2.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban2.instrument_id()),
                  server_iban2.nickname()));

  EXPECT_THAT(iban_suggestions[2],
              EqualsIbanSuggestion(
                  server_iban3.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban3.instrument_id()),
                  server_iban3.nickname()));

  EXPECT_EQ(iban_suggestions[3].type, SuggestionType::kSeparator);

  EXPECT_EQ(iban_suggestions[4].main_text.value,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  EXPECT_EQ(iban_suggestions[4].type, SuggestionType::kManageIban);
}

TEST_P(AutofillIbanSuggestionContentTest, GetLocalAndServerIbanSuggestions) {
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

  EXPECT_THAT(iban_suggestions[0],
              EqualsIbanSuggestion(
                  server_iban1.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban1.instrument_id()),
                  server_iban1.nickname()));

  EXPECT_THAT(iban_suggestions[1],
              EqualsIbanSuggestion(
                  server_iban2.GetIdentifierStringForAutofillDisplay(),
                  Suggestion::InstrumentId(server_iban2.instrument_id()),
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

// This class helps test the credit card contents that are displayed in
// Autofill suggestions. It covers suggestions on Desktop/Android dropdown,
// and on Android keyboard accessory.
class AutofillCreditCardSuggestionContentTest
    : public PaymentsSuggestionGeneratorTest {
 public:
  AutofillCreditCardSuggestionContentTest() {
    feature_list_metadata_.InitAndEnableFeature(
        features::kAutofillEnableNewFopDisplayDesktop);
  }

  ~AutofillCreditCardSuggestionContentTest() override = default;

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

#if BUILDFLAG(IS_ANDROID)
  // For the keyboard accessory, the "Virtual card" label is added as a prefix
  // to the cardholder name.
  EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
            u"Virtual card  Elvis Presley");
  EXPECT_TRUE(virtual_card_name_field_suggestion.minor_texts.empty());
#elif BUILDFLAG(IS_IOS)
  if (virtual_card_name_field_suggestion.IsAcceptable()) {
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Virtual card");
  } else {
    EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
              u"Virtual disabled card");
  }
#else
  // On other platforms, the cardholder name is shown on the first line.
  EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
            u"Elvis Presley");
  EXPECT_TRUE(virtual_card_name_field_suggestion.minor_texts.empty());
#endif

#if BUILDFLAG(IS_IOS)
  // There should be 1 lines of label:
  // 1. Obfuscated last 4 digits "..1111".
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/2, u"1111"));
#elif BUILDFLAG(IS_ANDROID)
  // There should be only 1 line of label: obfuscated last 4 digits "..1111".
  EXPECT_THAT(virtual_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111")}}));
#else
  // The label for virtual card suggestion should be:
  // Card Network + last 4 digits.
  EXPECT_THAT(
      virtual_card_name_field_suggestion,
      EqualLabels(
          {{server_card.NetworkAndLastFourDigits(/*obfuscation_length=*/2)}}));
#endif
  EXPECT_EQ(virtual_card_name_field_suggestion.IsAcceptable(), true);
  EXPECT_EQ(virtual_card_name_field_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
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
  if (virtual_card_number_field_suggestion.IsAcceptable()) {
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
              u"Virtual card");
  } else {
    EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
              u"Virtual card disabled");
  }
#elif BUILDFLAG(IS_ANDROID)
  // For the keyboard accessory, the "Virtual card" label is added as a prefix
  // to the card number. The obfuscated last four digits are shown in a
  // separate view.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
            u"Virtual card  Visa");
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_texts[0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/2, u"1111"));
#else
  // For Desktop, display the card name and the last 4 digits, followed
  // by a dot separator and expiration date.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value,
            server_card.NetworkAndLastFourDigits(/*obfuscation_length=*/2));
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_texts[0].value,
            kEllipsisDotSeparator);
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_texts[1].value,
            server_card.AbbreviatedExpirationDateForDisplay(false));
#endif
  EXPECT_EQ(virtual_card_number_field_suggestion.IsAcceptable(), true);
  EXPECT_EQ(virtual_card_number_field_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
#if BUILDFLAG(IS_ANDROID)
  // For the keyboard accessory, there is no label.
  ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
#endif
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
  EXPECT_TRUE(real_card_name_field_suggestion.minor_texts.empty());

#if BUILDFLAG(IS_IOS)
  // For IOS, the label is "..1111".
  EXPECT_THAT(real_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111")}}));
#elif BUILDFLAG(IS_ANDROID)
  // For the keyboard accessory, the label is "..1111".
  EXPECT_THAT(real_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"1111")}}));
#else
  // For Desktop, the label is "Visa ..1111 • 02/29". Network name and
  // last four followed by expiration date.
  EXPECT_THAT(
      real_card_name_field_suggestion,
      EqualLabels(
          {{server_card.NetworkAndLastFourDigits(/*obfuscation_length=*/2),
            kEllipsisDotSeparator,
            server_card.AbbreviatedExpirationDateForDisplay(false)}}));
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
  EXPECT_TRUE(real_card_number_field_suggestion.minor_texts.empty());
  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(
      real_card_number_field_suggestion,
      EqualLabels(
          {{base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))})}}));
#elif BUILDFLAG(IS_ANDROID)
  // For Android, split the first line and populate the card name and
  // the last 4 digits separately.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(real_card_number_field_suggestion.minor_texts[0].value,
            CreditCard::GetObfuscatedStringForCardDigits(2, u"1111"));
  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(
      real_card_number_field_suggestion,
      EqualLabels(
          {{base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))})}}));
#else
  // For Desktop, display the card name and the last 4 digits, followed
  // by a dot separator and expiration date.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value,
            server_card.NetworkAndLastFourDigits(/*obfuscation_length=*/2));
  EXPECT_EQ(real_card_number_field_suggestion.minor_texts[0].value,
            kEllipsisDotSeparator);
  EXPECT_EQ(real_card_number_field_suggestion.minor_texts[1].value,
            server_card.AbbreviatedExpirationDateForDisplay(false));
  // The label is empty.
  EXPECT_TRUE(real_card_number_field_suggestion.labels.empty());
#endif
}

// Verify that the suggestion's texts are populated correctly for a local and
// server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_CvcField) {
  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  // We used last 4 to deduplicate local card and server card so we should set
  // local card with different last 4.
  local_card.SetNumber(u"5454545454545454");
  payments_data().AddCreditCard(std::move(local_card));
  payments_data().AddServerCreditCard(CreateServerCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions =
      GetCreditCardOrCvcFieldSuggestions(
          *autofill_client(), FormFieldData(),
          /*four_digit_combinations_in_dom=*/{},
          /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
          CREDIT_CARD_VERIFICATION_CODE,
          /*should_show_scan_credit_card=*/false, summary);

  // Both local card and server card suggestion should be shown when CVC field
  // is focused.
  ASSERT_EQ(suggestions.size(), 4U);
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#else
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Mastercard");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Verify that the suggestion's texts are populated correctly for a duplicate
// local and server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_Duplicate_CvcField) {
  // Create 2 duplicate local and server card with same last 4.
  payments_data().AddCreditCard(CreateLocalCard());
  payments_data().AddServerCreditCard(CreateServerCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions =
      GetCreditCardOrCvcFieldSuggestions(
          *autofill_client(), FormFieldData(),
          /*four_digit_combinations_in_dom=*/{},
          /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
          CREDIT_CARD_VERIFICATION_CODE,
          /*should_show_scan_credit_card=*/false, summary);

  // Only 1 suggestion + footer should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_VirtualCard_CvcField) {
  // Create a server card with CVC that enrolled to virtual card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(std::move(server_card));

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions =
      GetCreditCardOrCvcFieldSuggestions(
          *autofill_client(), FormFieldData(),
          /*four_digit_combinations_in_dom=*/{},
          /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
          CREDIT_CARD_VERIFICATION_CODE,
          /*should_show_scan_credit_card=*/false, summary);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"Virtual card  CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Visa");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#elif !BUILDFLAG(IS_IOS)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verify that the FPAN and VCN suggestion's texts are populated correctly for a
// enrolled card when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_VirtualCard_Duplicate_CvcField) {
  // Create duplicate local and server card with CVC that enrolled to virtual
  // card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(std::move(server_card));
  payments_data().AddCreditCard(CreateLocalCard());

  CreditCardSuggestionSummary summary;
  const std::vector<Suggestion> suggestions =
      GetCreditCardOrCvcFieldSuggestions(
          *autofill_client(), FormFieldData(),
          /*four_digit_combinations_in_dom=*/{},
          /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
          CREDIT_CARD_VERIFICATION_CODE,
          /*should_show_scan_credit_card=*/false, summary);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if BUILDFLAG(IS_IOS)
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_LargeKeyboardAccessoryFormat) {
  // Enable formatting for large keyboard accessories.
  autofill_client()->set_format_for_large_keyboard_accessory(true);

  CreditCard server_card = CreateServerCard();

  const std::u16string obfuscated_number =
      CreditCard::GetObfuscatedStringForCardDigits(/*obfuscation_length=*/2,
                                                   u"1111");
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the cardholder name field is focused.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_NameField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // On other platforms, the cardholder name is shown on the first line.
  EXPECT_EQ(virtual_card_name_field_suggestion.main_text.value,
            u"Elvis Presley");
  EXPECT_TRUE(virtual_card_name_field_suggestion.minor_texts.empty());

  // There should be 1 lines of labels with 2 columns:
  // 1. Card name "Visa".
  // 2. obfuscated last 4 digits "....1111".
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 2U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value, u"Visa");
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][1].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"1111"));
  EXPECT_EQ(virtual_card_name_field_suggestion.IsAcceptable(), true);
  EXPECT_EQ(virtual_card_name_field_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
            u"Virtual card");
}

// Verify that the suggestion's texts are populated correctly for a virtual card
// suggestion when the card number field is focused.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_NumberField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // Card name and the obfuscated last four digits are shown separately.
  EXPECT_EQ(virtual_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(virtual_card_number_field_suggestion.minor_texts[0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"1111"));
  EXPECT_EQ(virtual_card_number_field_suggestion.IsAcceptable(), true);
  EXPECT_EQ(virtual_card_number_field_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  EXPECT_THAT(virtual_card_number_field_suggestion,
              EqualLabels({{u"Virtual card"}}));
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the cardholder name field is focused.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_MaskedServerCardMetadata_NameField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = CreateServerCard();

  // Name field suggestion for non-virtual cards.
  Suggestion real_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false);

  // Only the name is displayed on the first line.
  EXPECT_EQ(real_card_name_field_suggestion.main_text.value, u"Elvis Presley");
  EXPECT_TRUE(real_card_name_field_suggestion.minor_texts.empty());
  // Card network and last four are shown separately.
  ASSERT_EQ(real_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(real_card_name_field_suggestion.labels[0].size(), 2U);
  EXPECT_EQ(real_card_name_field_suggestion.labels[0][0].value, u"Visa");
  EXPECT_EQ(real_card_name_field_suggestion.labels[0][1].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/4, u"1111"));
}

// Verify that the suggestion's texts are populated correctly for a masked
// server card suggestion when the card number field is focused.
TEST_F(
    AutofillCreditCardSuggestionContentTest,
    CreateCreditCardSuggestion_MaskedServerCardMetadata_NumberField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = CreateServerCard();

  // Card number field suggestion for non-virtual cards.
  Suggestion real_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false,
                                        /*card_linked_offer_available=*/false);

  // Split the first line and populate the card name and the last 4 digits
  // separately.
  EXPECT_EQ(real_card_number_field_suggestion.main_text.value, u"Visa");
  EXPECT_EQ(real_card_number_field_suggestion.minor_texts[0].value,
            CreditCard::GetObfuscatedStringForCardDigits(4, u"1111"));

  // The label is the expiration date formatted as mm/yy.
  EXPECT_THAT(
      real_card_number_field_suggestion,
      EqualLabels(
          {{base::StrCat({base::UTF8ToUTF16(test::NextMonth()), u"/",
                          base::UTF8ToUTF16(test::NextYear().substr(2))})}}));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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
    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client()->GetAutofillOptimizationGuide()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(is_merchant_opted_out()));
  }
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

  // `IsAcceptable()` returns false only when merchant has opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.IsAcceptable(),
            !is_merchant_opted_out());

  // `HasDeactivatedStyle()` returns true only when merchant has opted out of
  // VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.HasDeactivatedStyle(),
            is_merchant_opted_out());
  EXPECT_EQ(
      virtual_card_name_field_suggestion.iph_metadata.feature,
      virtual_card_name_field_suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
#if BUILDFLAG(IS_ANDROID)
  // Android: There should be only 1 line of label: obfuscated last 4 digits
  // "..4444".
  EXPECT_THAT(virtual_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"4444")}}));
#elif BUILDFLAG(IS_IOS)
  // iOS: In dropdown, there should be one line, with the value equal to
  // obfuscated last four digits. And in AdjustVirtualCardSuggestionContent
  // we would make minor text the value, and set main text as the virtual card
  // label.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/2, u"4444"));
#else
  // Desktop: There should be one line for network and last four, and one line
  // if merchant opt-out virtual card text.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(),
            is_merchant_opted_out() ? 2U : 1U);
  if (is_merchant_opted_out()) {
    EXPECT_THAT(
        virtual_card_name_field_suggestion,
        EqualLabels(
            {{server_card.NetworkAndLastFourDigits(/*obfuscation_length=*/2)},
             {l10n_util::GetStringUTF16(expected_message_id())}}));
  } else {
    EXPECT_THAT(virtual_card_name_field_suggestion,
                EqualLabels({{server_card.NetworkAndLastFourDigits(
                    /*obfuscation_length=*/2)}}));
  }
#endif
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

  // `IsAcceptable()` returns false only when flag is enabled and merchant has
  // opted out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.IsAcceptable(),
            !is_merchant_opted_out());
  // `HasDeactivatedStyle()` returns true only when merchant has opted out of
  // VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.HasDeactivatedStyle(),
            is_merchant_opted_out());
  EXPECT_EQ(
      virtual_card_number_field_suggestion.iph_metadata.feature,
      virtual_card_number_field_suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);

#if BUILDFLAG(IS_ANDROID)
  // In Android, when filling card number, the labels are removed.
  ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
#elif BUILDFLAG(IS_IOS)
  // In iOS, when filling card number, only the expiration date will be shown.
  ASSERT_EQ(virtual_card_number_field_suggestion.labels.size(), 1U);
  EXPECT_EQ(virtual_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_NE(virtual_card_number_field_suggestion.labels[0][0].value,
            l10n_util::GetStringUTF16(expected_message_id()));
#else
  // Desktop, the label should be one-line message if it's merchant opt out.
  EXPECT_EQ(virtual_card_number_field_suggestion.labels.size(),
            is_merchant_opted_out() ? 1U : 0U);
#endif
}

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the cardholder name field is focused based on if
// merchant accepts virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NameField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = test::GetVirtualCard();

  // Name field suggestion for virtual cards.
  Suggestion virtual_card_name_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // `IsAcceptable()` returns false only when merchant has opted out of VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.IsAcceptable(),
            !is_merchant_opted_out());

  // `HasDeactivatedStyle()` returns true only when merchant has opted out of
  // VCN.
  EXPECT_EQ(virtual_card_name_field_suggestion.HasDeactivatedStyle(),
            is_merchant_opted_out());
  EXPECT_EQ(
      virtual_card_name_field_suggestion.iph_metadata.feature,
      virtual_card_name_field_suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
#if BUILDFLAG(IS_ANDROID)
  // Android: There should be only 1 line of label: obfuscated last 4 digits
  // "..4444".
  EXPECT_THAT(virtual_card_name_field_suggestion,
              EqualLabels({{CreditCard::GetObfuscatedStringForCardDigits(
                  /*obfuscation_length=*/2, u"4444")}}));
#elif BUILDFLAG(IS_IOS)
  // iOS: In dropdown, there should be one line, with the value equal to
  // obfuscated last four digits. And in AdjustVirtualCardSuggestionContent
  // we would make minor text the value, and set main text as the virtual card
  // label.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 1U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[0].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[0][0].value,
            CreditCard::GetObfuscatedStringForCardDigits(
                /*obfuscation_length=*/2, u"4444"));
#else
  // Desktop: There should be two lines, where first line is the card number
  // and second line is the virtual card text.
  ASSERT_EQ(virtual_card_name_field_suggestion.labels.size(), 2U);
  ASSERT_EQ(virtual_card_name_field_suggestion.labels[1].size(), 1U);
  EXPECT_EQ(virtual_card_name_field_suggestion.labels[1][0].value,
            l10n_util::GetStringUTF16(expected_message_id()));
#endif
}

// Verify that the suggestion's texts are populated correctly for a virtual
// card suggestion when the card number field is focused based on if
// merchant accepts virtual cards.
TEST_P(
    AutofillCreditCardSuggestionContentVcnMerchantOptOutTest,
    CreateCreditCardSuggestion_VirtualCardMetadata_MerchantOptOut_NumberField_NewFopDisplayOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewFopDisplayDesktop);
  CreditCard server_card = test::GetVirtualCard();

  // Card number field suggestion for virtual cards.
  Suggestion virtual_card_number_field_suggestion =
      CreateCreditCardSuggestionForTest(server_card, *autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true,
                                        /*card_linked_offer_available=*/false);

  // `IsAcceptable()` returns false only when flag is enabled and merchant has
  // opted out of VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.IsAcceptable(),
            !is_merchant_opted_out());
  // `HasDeactivatedStyle()` returns true only when merchant has opted out of
  // VCN.
  EXPECT_EQ(virtual_card_number_field_suggestion.HasDeactivatedStyle(),
            is_merchant_opted_out());
  EXPECT_EQ(
      virtual_card_number_field_suggestion.iph_metadata.feature,
      virtual_card_number_field_suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);

#if BUILDFLAG(IS_ANDROID)
  // In Android, when filling card number, the labels are removed.
  ASSERT_TRUE(virtual_card_number_field_suggestion.labels.empty());
#elif BUILDFLAG(IS_IOS)
  // In iOS, when filling card number, only the expiration date will be shown.
  ASSERT_EQ(virtual_card_number_field_suggestion.labels.size(), 1U);
  EXPECT_EQ(virtual_card_number_field_suggestion.labels[0].size(), 1U);
  EXPECT_NE(virtual_card_number_field_suggestion.labels[0][0].value,
            l10n_util::GetStringUTF16(expected_message_id()));
#else
  EXPECT_THAT(
      virtual_card_number_field_suggestion,
      EqualLabels({{l10n_util::GetStringUTF16(expected_message_id())}}));
#endif
}

class PaymentsSuggestionGeneratorTestForMetadata
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  PaymentsSuggestionGeneratorTestForMetadata() = default;

  ~PaymentsSuggestionGeneratorTestForMetadata() override = default;

  bool card_has_capital_one_icon() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         PaymentsSuggestionGeneratorTestForMetadata,
                         testing::Bool());

TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_ServerCard) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  payments_data().CacheImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(virtual_card_suggestion,
                                            card_art_url, fake_image));

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                            fake_image));
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
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
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
  payments_data().CacheImage(card_art_url, fake_image);

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(virtual_card_suggestion,
                                            card_art_url, fake_image));

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000002"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                            fake_image));
}

// Verifies that the `metadata_logging_context` is correctly set.
TEST_P(PaymentsSuggestionGeneratorTestForMetadata,
       GetCreditCardOrCvcFieldSuggestions_MetadataLoggingContext) {
  {
    // Create one server card with no metadata.
    CreditCard server_card = CreateServerCard();
    server_card.set_issuer_id(kCapitalOneCardIssuerId);
    if (card_has_capital_one_icon()) {
      server_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
    }
    payments_data().AddServerCreditCard(server_card);

    CreditCardSuggestionSummary summary;
    GetCreditCardOrCvcFieldSuggestions(
        *autofill_client(), FormFieldData(),
        /*four_digit_combinations_in_dom=*/{},
        /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
        CREDIT_CARD_NUMBER,
        /*should_show_scan_credit_card=*/false, summary);

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
    GetCreditCardOrCvcFieldSuggestions(
        *autofill_client(), FormFieldData(),
        /*four_digit_combinations_in_dom=*/{},
        /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
        CREDIT_CARD_NUMBER,
        /*should_show_scan_credit_card=*/false, summary);

    EXPECT_TRUE(
        summary.metadata_logging_context.instruments_with_metadata_available
            .contains(server_card_with_metadata.instrument_id()));
    EXPECT_TRUE(
        summary.metadata_logging_context.card_product_description_shown);
    EXPECT_TRUE(summary.metadata_logging_context.card_art_image_shown);

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
       GetCreditCardOrCvcFieldSuggestions_CustomCardIcon) {
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
  payments_data().CacheImage(card_art_url, fake_image);

  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  // Suggestions in `suggestions` are persisted in order of their presentation
  // to the user in the Autofill dropdown and currently virtual cards are shown
  // before their associated FPAN suggestion.
  Suggestion virtual_card_suggestion = suggestions[0];
  Suggestion fpan_card_suggestion = suggestions[1];

  // Verify that for virtual cards, the custom icon is shown.
  EXPECT_TRUE(VerifyCardArtImageExpectation(virtual_card_suggestion,
                                            card_art_url, fake_image));

  // Verify that for FPAN, the custom icon is shown if the card art is not the
  // Capital One virtual card art.
  EXPECT_EQ(VerifyCardArtImageExpectation(fpan_card_suggestion, card_art_url,
                                          fake_image),
            !card_has_capital_one_icon());
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
  // Create a server card.
  CreditCard server_card1 =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true,
      /*card_linked_offer_available=*/true);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));

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
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));

  if (keyboard_accessory_offer_enabled()) {
#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(real_card_suggestion.labels.size(), 1U);
    EXPECT_EQ(real_card_suggestion.iph_metadata.feature,
              &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
#endif
  } else {
    ASSERT_EQ(real_card_suggestion.labels.size(), 2U);
    ASSERT_EQ(real_card_suggestion.labels[1].size(), 1U);
    EXPECT_EQ(real_card_suggestion.labels[1][0].value,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK));
  }
}

// Tests suggestions with the new ranking algorithm experiment for Autofill
// suggestions enabled.
class PaymentsSuggestionGeneratorTestWithNewSuggestionRankingAlgorithm
    : public PaymentsSuggestionGeneratorTest {
 public:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableRankingFormulaCreditCards);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the ranking differences are appropriately set when suggestions are
// generated.
TEST_F(PaymentsSuggestionGeneratorTestWithNewSuggestionRankingAlgorithm,
       GetCreditCardOrCvcFieldSuggestions_SuggestionRankingContext) {
  // Ranked first stays first.
  CreditCard server_card1 = CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000001",
      /*server_id=*/"server_id1", /*instrument_id=*/1);
  server_card1.usage_history().set_use_count(100);
  server_card1.usage_history().set_use_date(AutofillClock::Now() -
                                            base::Days(5));
  Suggestion suggestion1 = CreateCreditCardSuggestionForTest(
      server_card1, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option*/ false,
      /*card_linked_offer_available*/ false);

  // Ranked second then third with new algorithm.
  CreditCard server_card2 = CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000002",
      /*server_id=*/"server_id2", /*instrument_id=*/2);
  server_card2.usage_history().set_use_count(200);
  server_card2.usage_history().set_use_date(AutofillClock::Now() -
                                            base::Days(40));
  Suggestion suggestion2 = CreateCreditCardSuggestionForTest(
      server_card2, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option*/ false,
      /*card_linked_offer_available*/ false);

  // Ranked third then second with new algorithm.
  CreditCard server_card3 = CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000003",
      /*server_id=*/"server_id3", /*instrument_id=*/3);
  server_card3.usage_history().set_use_count(10);
  server_card3.usage_history().set_use_date(AutofillClock::Now() -
                                            base::Days(10));
  Suggestion suggestion3 = CreateCreditCardSuggestionForTest(
      server_card3, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option*/ false,
      /*card_linked_offer_available*/ false);

  payments_data().AddServerCreditCard(server_card1);
  payments_data().AddServerCreditCard(server_card2);
  payments_data().AddServerCreditCard(server_card3);

  CreditCardSuggestionSummary summary;
  GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  base::flat_map<Suggestion::Guid,
                 autofill_metrics::SuggestionRankingContext::RelativePosition>
      expected_ranking_context = {{suggestion1.GetPayload<Suggestion::Guid>(),
                                   autofill_metrics::SuggestionRankingContext::
                                       RelativePosition::kRankedSame},
                                  {suggestion2.GetPayload<Suggestion::Guid>(),
                                   autofill_metrics::SuggestionRankingContext::
                                       RelativePosition::kRankedLower},
                                  {suggestion3.GetPayload<Suggestion::Guid>(),
                                   autofill_metrics::SuggestionRankingContext::
                                       RelativePosition::kRankedHigher}};

  EXPECT_EQ(summary.ranking_context.suggestion_rankings_difference_map,
            expected_ranking_context);
  EXPECT_TRUE(summary.ranking_context.RankingsAreDifferent());
}

// Checks that the suggestion ranking context is empty if the ranking experiment
// is disabled.
TEST_F(
    PaymentsSuggestionGeneratorTest,
    GetCreditCardOrCvcFieldSuggestions_SuggestionRankingContext_ExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableRankingFormulaCreditCards);
  CreditCard card = test::GetMaskedServerCard();
  payments_data().AddServerCreditCard(card);
  CreditCardSuggestionSummary summary;
  GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);
  EXPECT_TRUE(
      summary.ranking_context.suggestion_rankings_difference_map.empty());
}

// Tests that PaymentsSuggestionGenerator correctly returns virtual cards with
// usage data and VCN last four for a standalone cvc field.
TEST_F(
    PaymentsSuggestionGeneratorTest,
    GetCreditCardOrCvcFieldSuggestions_GetVirtualCreditCardsForStandaloneCvcField) {
  // Set up virtual card usage data and credit cards.
  payments_data().ClearCreditCards();
  CreditCard masked_server_card = test::GetVirtualCard();
  masked_server_card.set_guid("1234");
  VirtualCardUsageData virtual_card_usage_data =
      test::GetVirtualCardUsageData1();
  masked_server_card.set_instrument_id(
      *virtual_card_usage_data.instrument_id());

  // Add credit card and usage data to personal data manager.
  payments_data().AddVirtualCardUsageData(virtual_card_usage_data);
  payments_data().AddServerCreditCard(masked_server_card);

  FormFieldData field;
  field.set_origin(virtual_card_usage_data.merchant_origin());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field,
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kVirtualCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

// Params of SuggestionIphBubbleTest:
// -- CardInfoRetrievalEnrollmentState card_info_retrieval_enrollment_state:
class SuggestionIphBubbleTest
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<
          CreditCard::CardInfoRetrievalEnrollmentState> {
 public:
  CreditCard::CardInfoRetrievalEnrollmentState
  card_info_retrieval_enrollment_state() {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    PaymentsSuggestionGeneratorTest,
    SuggestionIphBubbleTest,
    testing::Values(
        CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalUnspecified,
        CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
        CreditCard::CardInfoRetrievalEnrollmentState::
            kRetrievalUnenrolledAndNotEligible,
        CreditCard::CardInfoRetrievalEnrollmentState::
            kRetrievalUnenrolledAndEligible));

// Verify that the card info retrieval enrolled suggestion `feature`,
// `iph_params` and `iph_description text` are set when card info retrieval
// enrollment state is enrolled.
TEST_P(SuggestionIphBubbleTest,
       CreateCreditCardSuggestion_CardInfoRetrievalSuggestion) {
  CreditCard server_card = CreateServerCard();
  std::string kIssuerId = {"paypay"};
  std::vector<std::u16string> kDiplayName = {u"PayPay"};
  server_card.set_issuer_id(kIssuerId);
  server_card.set_card_info_retrieval_enrollment_state(
      card_info_retrieval_enrollment_state());

  Suggestion card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(
      card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
      card_number_field_suggestion.iph_metadata.feature ==
          &feature_engagement::kIPHAutofillCardInfoRetrievalSuggestionFeature);
  EXPECT_EQ(
      card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
      card_number_field_suggestion.iph_metadata.iph_params == kDiplayName);

#if BUILDFLAG(IS_ANDROID)
  std::u16string expected_description =
      u"You can autofill this card because your PayPay account is linked to "
      u"Google Pay";
  EXPECT_EQ(
      card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
      expected_description ==
          card_number_field_suggestion.iph_description_text);
#endif
}

// Verify that the card info retrieval enrolled suggestion `feature` and
// `iph_description text` are set when card info retrieval enrollment state is
// enrolled but issuer_id is not set.
TEST_P(SuggestionIphBubbleTest,
       CreateCreditCardSuggestion_CardInfoRetrievalSuggestion_WithoutIssuerId) {
  CreditCard server_card = CreateServerCard();
  server_card.set_card_info_retrieval_enrollment_state(
      card_info_retrieval_enrollment_state());

  Suggestion card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false,
      /*card_linked_offer_available=*/false);

  EXPECT_EQ(
      card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
      card_number_field_suggestion.iph_metadata.feature ==
          &feature_engagement::kIPHAutofillCardInfoRetrievalSuggestionFeature);

#if BUILDFLAG(IS_ANDROID)
  std::u16string expected_description =
      u"You can autofill this card because your account is linked to "
      u"Google Pay";
  EXPECT_EQ(
      card_info_retrieval_enrollment_state() ==
          CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled,
      expected_description ==
          card_number_field_suggestion.iph_description_text);
#endif
}

// Params of GetFilteredCardsToSuggestTest:
// -- bool IsCvcStorageEnhancementEnabled: Indicates if the flag is enabled.
// -- FieldType get_trigger_field_type: Indicates triggered field type.
class GetFilteredCardsToSuggestTest
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<std::tuple<bool, FieldType>> {
 public:
  bool IsCvcStorageEnhancementEnabled() { return std::get<0>(GetParam()); }
  FieldType get_trigger_field_type() { return std::get<1>(GetParam()); }

 private:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillEnableCvcStorageAndFillingEnhancement,
        IsCvcStorageEnhancementEnabled());
    // Create 2 local cards and 2 server cards.
    payments_data().ClearCreditCards();
    CreditCard local_card_1 =
        CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card_1.SetNumber(u"4111111111111111");
    CreditCard local_card_2 =
        CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");
    local_card_2.SetNumber(u"4111111111111112");
    CreditCard server_card_1 = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000003",
        /*server_id=*/"server_i3", /*instrument_id=*/1);
    server_card_1.SetNumber(u"4111111111111113");
    CreditCard server_card_2 = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000004",
        /*server_id=*/"server_i4", /*instrument_id=*/1);
    server_card_2.SetNumber(u"4111111111111114");
    payments_data().AddCreditCard(local_card_1);
    payments_data().AddCreditCard(local_card_2);
    payments_data().AddServerCreditCard(server_card_1);
    payments_data().AddServerCreditCard(server_card_2);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    PaymentsSuggestionGeneratorTest,
    GetFilteredCardsToSuggestTest,
    testing::Combine(testing::Bool(),
                     testing::Values(FieldType::CREDIT_CARD_VERIFICATION_CODE,
                                     FieldType::CREDIT_CARD_NUMBER)));

// Verify that suggestions are filtered based on
// `autofilled_last_four_digits_in_form_for_filtering` when flag is
// on and triggered field type is CVC.
TEST_P(GetFilteredCardsToSuggestTest, GetFilteredCardsToSuggest) {
  FormFieldData field;
  CreditCardSuggestionSummary summary;

  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"1111",
      get_trigger_field_type(),
      /*should_show_scan_credit_card=*/false, summary);

  if (IsCvcStorageEnhancementEnabled() &&
      get_trigger_field_type() == FieldType::CREDIT_CARD_VERIFICATION_CODE) {
    // There are 3 suggestions, 1 for local card suggestion, followed by a
    // separator, and followed by "Manage payment methods..." which redirects to
    // the Chrome payment methods settings page.
    EXPECT_THAT(
        suggestions,
        UnorderedElementsAre(
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")),
            EqualsSuggestion(SuggestionType::kSeparator),
            EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/false)));
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
  } else {
    // There are 6 suggestions, 4 for card suggestions, followed by a separator,
    // and followed by "Manage payment methods..." which redirects to the Chrome
    // payment methods settings page.
    EXPECT_THAT(
        suggestions,
        UnorderedElementsAre(
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000003")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000004")),
            EqualsSuggestion(SuggestionType::kSeparator),
            EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/false)));
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
  }
}

// Verify that suggestions are not filtered if
// `autofilled_last_four_digits_in_form_for_filtering` is empty.
TEST_P(GetFilteredCardsToSuggestTest, EmptyFilteringSet) {
  FormFieldData field;
  CreditCardSuggestionSummary summary;

  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      get_trigger_field_type(),
      /*should_show_scan_credit_card=*/false, summary);

  // There are 6 suggestions, 4 for card suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  EXPECT_THAT(
      suggestions,
      UnorderedElementsAre(
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000001")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000002")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000003")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000004")),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/false)));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Verify that suggestions are not filtered if triggered field is not CVC.
TEST_P(GetFilteredCardsToSuggestTest, TriggerFieldIsNotCvc) {
  FormFieldData field;
  CreditCardSuggestionSummary summary;

  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"1111",
      FieldType::CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);

  // There are 6 suggestions, 4 for card suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  EXPECT_THAT(
      suggestions,
      UnorderedElementsAre(
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000001")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000002")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000003")),
          SuggestionWithGuidPayload(
              Suggestion::Guid("00000000-0000-0000-0000-000000000004")),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/false)));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

// Verify that suggestions are empty if there is no card matched.
TEST_P(GetFilteredCardsToSuggestTest, NoMatchCard) {
  FormFieldData field;
  CreditCardSuggestionSummary summary;

  std::vector<Suggestion> suggestions = GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), field, /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      u"9999", get_trigger_field_type(),
      /*should_show_scan_credit_card=*/false, summary);

  if (IsCvcStorageEnhancementEnabled() &&
      get_trigger_field_type() == FieldType::CREDIT_CARD_VERIFICATION_CODE) {
    // There are no suggestions.
    EXPECT_EQ(suggestions.size(), 0U);
  } else {
    EXPECT_THAT(
        suggestions,
        UnorderedElementsAre(
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000001")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000002")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000003")),
            SuggestionWithGuidPayload(
                Suggestion::Guid("00000000-0000-0000-0000-000000000004")),
            EqualsSuggestion(SuggestionType::kSeparator),
            EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/false)));
  }
}

// Params of CvcStorageAndFillingStandaloneFormEnhancementTest:
// -- bool IsCvcStorageStandaloneFormEnhancementEnabled: Indicates if the flag
// is enabled.
class CvcStorageAndFillingStandaloneFormEnhancementTest
    : public PaymentsSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsCvcStorageStandaloneFormEnhancementEnabled() { return GetParam(); }

 private:
  void SetUp() override {
    PaymentsSuggestionGeneratorTest::SetUp();
    if (IsCvcStorageStandaloneFormEnhancementEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kAutofillEnableCvcStorageAndFillingEnhancement,
           features::
               kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::kAutofillEnableCvcStorageAndFillingEnhancement},
          /*disabled_features=*/
          {features::
               kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement});
    }
    // Create 2 local cards and 2 server cards.
    payments_data().ClearCreditCards();
    CreditCard local_card_1 =
        CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
    local_card_1.SetNumber(u"4111111111111111");
    CreditCard local_card_2 =
        CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");
    local_card_2.SetNumber(u"4111111111111112");
    CreditCard server_card_1 = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000003",
        /*server_id=*/"server_i3", /*instrument_id=*/1);
    server_card_1.SetNumber(u"4111111111111113");
    CreditCard server_card_2 = CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000004",
        /*server_id=*/"server_i4", /*instrument_id=*/1);
    server_card_2.SetNumber(u"4111111111111114");
    payments_data().AddCreditCard(local_card_1);
    payments_data().AddCreditCard(local_card_2);
    payments_data().AddServerCreditCard(server_card_1);
    payments_data().AddServerCreditCard(server_card_2);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(PaymentsSuggestionGeneratorTest,
                         CvcStorageAndFillingStandaloneFormEnhancementTest,
                         testing::Bool());

// Tests that GetCreditCardSuggestions function correctly returns masked server
// card suggestions when no VCN suggestions for a standalone cvc field.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards) {
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(),
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"1111", "1113"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  if (IsCvcStorageStandaloneFormEnhancementEnabled()) {
    // There are 4 suggestions, 2 for card suggestions, followed by a separator,
    // and followed by "Manage payment methods..." which redirects to the Chrome
    // payment methods settings page.
    EXPECT_EQ(suggestions.size(), 4U);
    EXPECT_THAT(suggestions, UnorderedElementsAre(
                                 SuggestionWithGuidPayload(Suggestion::Guid(
                                     "00000000-0000-0000-0000-000000000001")),
                                 SuggestionWithGuidPayload(Suggestion::Guid(
                                     "00000000-0000-0000-0000-000000000003")),
                                 EqualsSuggestion(SuggestionType::kSeparator),
                                 EqualsManagePaymentsMethodsSuggestion(
                                     /*with_gpay_logo=*/false)));
  } else {
    EXPECT_EQ(suggestions.size(), 0U);
  }
}

// Tests that GetCreditCardSuggestions function correctly returns masked server
// card suggestions matching
// `autofilled_last_four_digits_in_form_for_filtering` for normal
// credit card form. It also tests that `four_digit_combinations_in_dom` doesn't
// affect suggestions.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_NormalCreditCardForm) {
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(),
      FieldType::CREDIT_CARD_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"1113"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/
      u"1111");

  // There are 3 suggestions, 1 for card suggestions, followed by a separator,
  // and followed by "Manage payment methods..." which redirects to the Chrome
  // payment methods settings page.
  EXPECT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              UnorderedElementsAre(SuggestionWithGuidPayload(Suggestion::Guid(
                                       "00000000-0000-0000-0000-000000000001")),
                                   EqualsSuggestion(SuggestionType::kSeparator),
                                   EqualsManagePaymentsMethodsSuggestion(
                                       /*with_gpay_logo=*/false)));
}

// Tests that GetCreditCardSuggestions function correctly returns no standalone
// CVC field suggestions when there is no last four in DOM.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_NoDomLastFour) {
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(),
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_EQ(suggestions.size(), 0U);
}

// Tests that GetCreditCardSuggestions function correctly returns no standalone
// CVC field suggestion when there is last four match in the DOM.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_NoLastFourMatch) {
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(),
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"0000", "9999"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_EQ(suggestions.size(), 0U);
}

// Tests that GetCreditCardSuggestions function correctly returns no standalone
// CVC field suggestion when the matched card doesn't have CVC.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_NoCvc) {
  CreditCard server_card_3 = CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000005",
      /*server_id=*/"server_i5", /*instrument_id=*/1);
  server_card_3.SetNumber(u"4111111111111234");
  server_card_3.set_cvc(u"");
  payments_data().AddServerCreditCard(server_card_3);
  CreditCardSuggestionSummary summary;
  std::vector<std::string> four_digit_combinations_in_dom = {"1234"};
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), FormFieldData(),
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");
  EXPECT_EQ(suggestions.size(), 0U);
}

// Tests that GetCreditCardSuggestions correctly only returns virtual cards with
// usage data and VCN last four for a standalone cvc field when there are both
// server and virtual card.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_BothServerAndVirtualCard) {
  // Set up virtual card usage data and credit cards.
  payments_data().ClearCreditCards();
  CreditCard virtual_card = test::GetVirtualCard();
  CreditCard masked_server_card = test::GetMaskedServerCard();
  virtual_card.SetNumber(u"4111111111111234");
  masked_server_card.SetNumber(u"4111111111111234");
  VirtualCardUsageData virtual_card_usage_data =
      test::GetVirtualCardUsageData1();
  virtual_card.set_instrument_id(*virtual_card_usage_data.instrument_id());

  // Add credit card and usage data to personal data manager.
  payments_data().AddVirtualCardUsageData(virtual_card_usage_data);
  payments_data().AddServerCreditCard(virtual_card);
  payments_data().AddServerCreditCard(masked_server_card);

  FormFieldData field;
  field.set_origin(virtual_card_usage_data.merchant_origin());
  CreditCardSuggestionSummary summary;
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      *autofill_client(), field,
      FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE, summary,
      /*is_complete_form=*/false,
      /*should_show_scan_credit_card=*/false,
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"");

  EXPECT_THAT(
      suggestions,
      UnorderedElementsAre(
          EqualsSuggestion(SuggestionType::kVirtualCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

TEST_F(
    PaymentsSuggestionGeneratorTestWithNewSuggestionRankingAlgorithm,
    GetCreditCardOrCvcFieldSuggestions_SuggestionRankingContext_NoRankingDifference) {
  CreditCard card = CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000001",
      /*server_id=*/"server_id1", /*instrument_id=*/1);
  Suggestion suggestion1 = CreateCreditCardSuggestionForTest(
      card, *autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option*/ false,
      /*card_linked_offer_available*/ false);
  payments_data().AddServerCreditCard(card);

  CreditCardSuggestionSummary summary;
  GetCreditCardOrCvcFieldSuggestions(
      *autofill_client(), FormFieldData(),
      /*four_digit_combinations_in_dom=*/{},
      /*autofilled_last_four_digits_in_form_for_filtering=*/u"",
      CREDIT_CARD_NUMBER,
      /*should_show_scan_credit_card=*/false, summary);
  EXPECT_FALSE(summary.ranking_context.RankingsAreDifferent());
}

class AutofillCreditCardSuggestionContentForTouchToFillTest
    : public AutofillCreditCardSuggestionContentTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_merchant_opted_out() { return GetParam(); }

 private:
  void SetUp() override {
    AutofillCreditCardSuggestionContentTest::SetUp();
    ON_CALL(*static_cast<MockAutofillOptimizationGuide*>(
                autofill_client()->GetAutofillOptimizationGuide()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(is_merchant_opted_out()));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillCreditCardSuggestionContentForTouchToFillTest,
                         testing::Bool());

// Verify that the suggestion's `main_text` and `minor_text` are populated
// correctly for both the virtual card and the real card. Furthermore, it
// verifies that if the merchant has opted out of VCN, `apply_deactivated_style`
// is set only for the virtual card, not for the real card.
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_MainTextMinorTextMerchantOptOut) {
  CreditCard virtual_card = test::GetVirtualCard();
  CreditCard server_card = CreateServerCard();
  std::vector<CreditCard> cards = {virtual_card, server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].main_text.value,
            virtual_card.CardNameForAutofillDisplay(virtual_card.nickname()));
  EXPECT_EQ(suggestions[0].minor_texts[0].value,
            virtual_card.ObfuscatedNumberWithVisibleLastFourDigits());
  // `HasDeactivatedStyle()` returns true only when merchant has opted out of
  // VCN.
  EXPECT_EQ(suggestions[0].HasDeactivatedStyle(), is_merchant_opted_out());

  EXPECT_EQ(suggestions[1].main_text.value,
            server_card.CardNameForAutofillDisplay(server_card.nickname()));
  EXPECT_EQ(suggestions[1].minor_texts[0].value,
            server_card.ObfuscatedNumberWithVisibleLastFourDigits());
  // `HasDeactivatedStyle()` is false for the real card.
  EXPECT_EQ(suggestions[1].HasDeactivatedStyle(), false);
}

TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_Labels) {
  CreditCard virtual_card = test::GetVirtualCard();
  CreditCard server_card = CreateServerCard();
  std::vector<CreditCard> cards = {virtual_card, server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 2U);
  // Virtual card displays `Virtual card` label. If the merchant has opted out
  // of accepting virtual cards, it displays `Merchant doesn't accept this
  // virtual card` label.
  if (is_merchant_opted_out()) {
    EXPECT_THAT(
        suggestions[0],
        EqualLabels({{l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE)}}));
  } else {
    EXPECT_THAT(suggestions[0],
                EqualLabels({{l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)}}));
  }
  // Server card displays the expiration date in MM/YY format.
  EXPECT_THAT(suggestions[1],
              EqualLabels({{server_card.GetInfo(
                  CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale())}}));
}

// Verify that the suggestion's `main_text_content_description` appends the
// network name if the card name and network name differ.
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_MainTextDescriptionWithNetwork) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNickname(u"NickName");
  server_card.SetNetworkForMaskedCard(kVisaCard);
  std::vector<CreditCard> cards = {server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(
      suggestions[0]
          .GetPayload<Suggestion::PaymentsPayload>()
          .main_text_content_description,
      base::StrCat({server_card.CardNameForAutofillDisplay(), u" ", u"visa"}));
}

// Verify that the suggestion's `main_text_content_description` does not include
// the network name if it is identical to the card name.
TEST_P(
    AutofillCreditCardSuggestionContentForTouchToFillTest,
    GetCreditCardSuggestionsForTouchToFill_MainTextDescriptionWithoutNetwork) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNetworkForMaskedCard(kVisaCard);
  std::vector<CreditCard> cards = {server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0]
                .GetPayload<Suggestion::PaymentsPayload>()
                .main_text_content_description,
            server_card.CardNameForAutofillDisplay());
}

TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_Icon) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNetworkForMaskedCard(kVisaCard);
  std::vector<CreditCard> cards = {server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0].icon, Suggestion::Icon::kCardVisa);
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_CustomIcon) {
  CreditCard server_card = CreateServerCard();
  GURL expected_custom_icon_url("https://www.example.com/card-art");
  server_card.set_card_art_url(expected_custom_icon_url);
  std::vector<CreditCard> cards = {server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 1U);
  const Suggestion::CustomIconUrl* custom_icon_url =
      std::get_if<Suggestion::CustomIconUrl>(&suggestions[0].custom_icon);
  EXPECT_EQ(**custom_icon_url, expected_custom_icon_url);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that the suggestion's payment payload includes credit card guid and
// local card status.
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_PaymentsPayloadGuid) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNetworkForMaskedCard(kVisaCard);
  CreditCard local_card = test::GetCreditCard();
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(local_card);
  std::vector<CreditCard> cards = {server_card, local_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, *autofill_client(), *credit_card_form_event_logger_);

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(
      suggestions[0].GetPayload<Suggestion::PaymentsPayload>().guid.value(),
      server_card.guid());
  EXPECT_FALSE(suggestions[0]
                   .GetPayload<Suggestion::PaymentsPayload>()
                   .is_local_payments_method);
  EXPECT_EQ(
      suggestions[1].GetPayload<Suggestion::PaymentsPayload>().guid.value(),
      local_card.guid());
  EXPECT_TRUE(suggestions[1]
                  .GetPayload<Suggestion::PaymentsPayload>()
                  .is_local_payments_method);
}

}  // namespace
}  // namespace autofill
