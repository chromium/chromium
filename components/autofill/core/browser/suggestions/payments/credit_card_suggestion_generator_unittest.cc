// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <memory>
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
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/integrators/optimization_guide/mock_autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager_test_api.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace autofill {
namespace {

using ::gfx::test::AreImagesEqual;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Return;
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

Matcher<Suggestion> EqualSuggestionTabIndex(SuggestionTabIndex tab_index) {
  return Field("tab_index", &Suggestion::tab_index, tab_index);
}

// This function is currently only used for BNPL unittests, and BNPL is
// currently only available for desktop and android platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
Matcher<Suggestion> EqualsSuggestion(const Suggestion& suggestion) {
  return AllOf(Field(&Suggestion::type, suggestion.type),
               Field(&Suggestion::main_text, suggestion.main_text),
               Field(&Suggestion::minor_texts, suggestion.minor_texts),
               Field(&Suggestion::icon, suggestion.icon),
               Field(&Suggestion::labels, suggestion.labels),
               EqualSuggestionTabIndex(suggestion.tab_index));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

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

testing::Matcher<const std::vector<Suggestion>&>
ContainsCreditCardFooterSuggestions(bool with_gpay_logo,
                                    bool with_bnpl_footnote = false) {
  if (with_bnpl_footnote) {
    return AllOf(
        SizeIs(Ge(3)),
        ResultOf(
            [](const std::vector<Suggestion>& container) {
              return base::span(container).template last<3>();
            },
            ElementsAre(
                AllOf(EqualsSuggestion(SuggestionType::kBnplFootnote),
                      EqualSuggestionTabIndex(kPayLaterSuggestionTabIndex)),
                AllOf(EqualsSuggestion(SuggestionType::kSeparator),
                      EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)),
                AllOf(EqualsManagePaymentsMethodsSuggestion(with_gpay_logo),
                      EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)))));
  }
  return AllOf(
      SizeIs(Ge(2)),
      ResultOf(
          [](const std::vector<Suggestion>& container) {
            return base::span(container).template last<2>();
          },
          ElementsAre(
              AllOf(EqualsSuggestion(SuggestionType::kSeparator),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)),
              AllOf(EqualsManagePaymentsMethodsSuggestion(with_gpay_logo),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)))));
}

auto SuggestionWithGuidPayload(const Suggestion::Guid& guid) {
  return Property(&Suggestion::GetPayload<Suggestion::Guid>, guid);
}

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(TestAutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;
  ~MockBrowserAutofillManager() override = default;

  MOCK_METHOD(autofill_metrics::CreditCardFormEventLogger&,
              GetCreditCardFormEventLogger,
              (),
              (override));
};

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  MOCK_METHOD(bool, HasCreditCardScanFeature, (), (const, override));
};

class MockCreditCardFormEventLogger
    : public autofill_metrics::CreditCardFormEventLogger {
 public:
  using autofill_metrics::CreditCardFormEventLogger::CreditCardFormEventLogger;
  MOCK_METHOD(
      void,
      OnMetadataLoggingContextReceived,
      (autofill_metrics::CardMetadataLoggingContext metadata_logging_context),
      (override));
  MOCK_METHOD(void, OnBnplSuggestionShown, (), (override));
};

// TODO(crbug.com/40176273): Move GetSuggestionsForCreditCard tests and
// BrowserAutofillManagerTestForSharingNickname here from
// browser_autofill_manager_unittest.cc.
class CreditCardSuggestionGeneratorTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver,
                                                 MockBrowserAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    payments_data().SetPrefService(autofill_client().GetPrefs());
    payments_data().SetSyncServiceForTest(&sync_service_);
    auto mock_payments_autofill_client =
        std::make_unique<NiceMock<MockPaymentsAutofillClient>>(
            &autofill_client());
    mock_payments_autofill_client_ = mock_payments_autofill_client.get();
    autofill_client().set_payments_autofill_client(
        std::move(mock_payments_autofill_client));
    CreateAutofillDriver();
    credit_card_form_event_logger_ =
        std::make_unique<NiceMock<MockCreditCardFormEventLogger>>(
            &autofill_manager());
    ON_CALL(autofill_manager(), GetCreditCardFormEventLogger())
        .WillByDefault(testing::ReturnRef(*credit_card_form_event_logger_));
  }

  void TearDown() override {
    mock_payments_autofill_client_ = nullptr;
    credit_card_form_event_logger_->OnDestroyed();
    credit_card_form_event_logger_.reset();
    DestroyAutofillClient();
  }

  MockCreditCardFormEventLogger& credit_card_form_event_logger() {
    return *credit_card_form_event_logger_;
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

  struct FormBundle {
    FormData form;
    std::unique_ptr<FormStructure> form_structure;
    FormFieldData trigger_field;
    raw_ptr<AutofillField> trigger_autofill_field;
  };

  FormBundle GetFormWithTypes(const test::FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    auto form_structure = std::make_unique<FormStructure>(form);
    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      form_structure->field(i)->SetTypeTo(
          AutofillType(form_description.fields[i].role), std::nullopt);
    }

    AutofillField* trigger_autofill_field = form_structure->field(0);
    FormFieldData trigger_field = form.fields()[0];

    return {form, std::move(form_structure), trigger_field,
            trigger_autofill_field};
  }

  TestPaymentsDataManager& payments_data() {
    return autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager();
  }

  payments::AmountExtractionManager& amount_extraction_manager() {
    return autofill_manager().GetAmountExtractionManager();
  }

  const std::string& app_locale() { return payments_data().app_locale(); }

  void SetCreditCardUploadEnabledForTest(bool credit_card_upload_enabled) {
    autofill_client().set_is_credit_card_upload_enabled(
        credit_card_upload_enabled);
  }

 protected:
  raw_ptr<MockPaymentsAutofillClient> mock_payments_autofill_client_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<MockCreditCardFormEventLogger> credit_card_form_event_logger_;
};

// The card benefits label generation currently varies across operating systems.
// On iOS, the label is not displayed at present. For Desktop, the benefit is
// shown along with a "terms apply" message (e.g., "5% cash back on all
// purchases (terms apply)"). On Android(Clank), the benefits label is displayed
// without the "(terms apply)" message (e.g., "5% cash back on all purchases").
// Therefore, it is necessary to separate the tests for these methods based on
// the specific operating system.
// Params:
// 1. Function reference to call which creates the appropriate credit card
// benefit for the unittest.
// 2. Benefit source which is set for the credit card with benefits.
#if !BUILDFLAG(IS_IOS)
// TODO(crbug.com/325646493): Clean up
// CreditCardSuggestionGeneratorTest.AutofillCreditCardBenefitsLabelTest setup
// and parameters.
class AutofillCreditCardBenefitsLabelTest
    : public CreditCardSuggestionGeneratorTest,
      public ::testing::WithParamInterface<
          std::tuple<base::FunctionRef<CreditCardBenefit()>, std::string>> {
 public:
  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();

    scoped_feature_list_.InitWithFeatureStates(
        {{features::kAutofillEnableFlatRateCardBenefitsBlocklist, true}});

    std::u16string benefit_description;
    int64_t instrument_id;

    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            ShouldBlockFlatRateBenefitSuggestionLabelsForUrl)
        .WillByDefault(testing::Return(false));

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
      autofill_client().set_last_committed_primary_main_frame_url(
          benefit.merchant_domains().begin()->GetURL());
    } else if (std::holds_alternative<CreditCardCategoryBenefit>(
                   GetBenefit())) {
      ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                  autofill_client().GetAutofillOptimizationGuideDecider()),
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

    card_.set_benefit_source(GetBenefitSource());
    payments_data().AddServerCreditCard(card_);
  }

  CreditCardBenefit GetBenefit() const { return std::get<0>(GetParam())(); }

  const std::string& GetBenefitSource() const {
    return std::get<1>(GetParam());
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
        card(), autofill_client(), CREDIT_CARD_NUMBER,
        /*virtual_card_option=*/false, &metadata_logging_context);

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
    CreditCardSuggestionGeneratorTest,
    AutofillCreditCardBenefitsLabelTest,
    testing::Combine(testing::Values(&test::GetActiveCreditCardFlatRateBenefit,
                                     &test::GetActiveCreditCardCategoryBenefit,
                                     &test::GetActiveCreditCardMerchantBenefit),
                     ::testing::Values("amex", "bmo", "curinos")));

#if !BUILDFLAG(IS_ANDROID)
// Checks that for FPAN suggestions that the benefit description is displayed.
TEST_P(AutofillCreditCardBenefitsLabelTest, BenefitSuggestionLabel_Fpan) {
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);
  if (GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_TRUE(suggestion.labels.empty());
  } else {
    EXPECT_THAT(suggestion.labels,
                ElementsAre(std::vector<Suggestion::Text>{
                    Suggestion::Text(expected_benefit_text())}));
  }
}

// Checks that for FPAN suggestions that the benefit description is displayed
// when optimization guide is null except category benefits.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_Fpan_NoOptimizationGuide) {
  autofill_client().ResetAutofillOptimizationGuideDecider();
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);
  ASSERT_FALSE(autofill_client().GetAutofillOptimizationGuideDecider());
  if (GetBenefitSource() == "curinos" &&
      !std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_TRUE(suggestion.labels.empty());
  } else if (std::holds_alternative<CreditCardCategoryBenefit>(GetBenefit())) {
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
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);
  if (GetBenefitSource() == "curinos" &&
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
  EXPECT_EQ(CreateCreditCardSuggestionForTest(card(), autofill_client(),
                                              CREDIT_CARD_NUMBER,
                                              /*virtual_card_option=*/true)
                .iph_metadata.feature,
            &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
}

// Checks that `feature` is set to null when the card is not eligible
// for the benefits.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionFeatureForIph_IsNullWhenCardNotEligibleForBenefits) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFlatRateBenefitSuggestionLabelsForUrl)
      .WillByDefault(testing::Return(true));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  if (std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_EQ(suggestion.iph_metadata.feature, nullptr);
  } else if (GetBenefitSource() == "curinos") {
    // Currently, Curinos only supports flat rate benefit, so when benefit
    // source is curinos, and the benefit is not a flat rate benefit, no
    // benefit suggestion will be shown.
    EXPECT_EQ(suggestion.iph_metadata.feature, nullptr);
  } else {
    EXPECT_EQ(suggestion.iph_metadata.feature,
              &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
  }
}

// Checks that for virtual cards suggestion the benefit description is shown
// as a label.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabel_VirtualCard) {
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true);
  if (GetBenefitSource() == "curinos" &&
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
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_THAT(
      CreateCreditCardSuggestionForTest(virtual_card, autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true)
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

// Checks that the merchant benefit description is not displayed for suggestions
// where the webpage's URL is different from the benefit's applicable URL.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_MerchantUrlIsDifferent) {
  if (!std::holds_alternative<CreditCardMerchantBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-merchant benefits.";
  }
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

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

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  // Category benefit description is not returned.
  EXPECT_TRUE(suggestion.labels.empty());
}

// Checks that the benefit description is not displayed when benefit suggestions
// are disabled for the given card and url.
TEST_P(AutofillCreditCardBenefitsLabelTest,
       BenefitSuggestionLabelNotDisplayed_BlockedUrl) {
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFlatRateBenefitSuggestionLabelsForUrl)
      .WillByDefault(testing::Return(true));
  Suggestion suggestion = CreateCreditCardSuggestionForTest(
      card(), autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  // Benefit description is not returned for flat rate benefits.
  if (std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    EXPECT_TRUE(suggestion.labels.empty());
  } else if (GetBenefitSource() == "curinos") {
    // Currently, Curinos only supports flat rate benefit, so when benefit
    // source is curinos, and the beneit is not a flat rate benefit, no
    // benefit suggestion will be shown.
    EXPECT_TRUE(suggestion.labels.empty());
  } else {
    EXPECT_FALSE(suggestion.labels.empty());
  }
}

#else

TEST_P(AutofillCreditCardBenefitsLabelTest,
       GetCreditCardSuggestionsForTouchToFill_BenefitsAdded_RealCard) {
  std::vector<CreditCard> cards = {card()};
  base::flat_map<int64_t, std::string>
      expected_instrument_ids_to_available_benefit_sources = {
          {cards[0].instrument_id(), cards[0].benefit_source()}};
  EXPECT_CALL(credit_card_form_event_logger(),
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  if (GetBenefitSource() == "curinos" &&
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
  EXPECT_CALL(credit_card_form_event_logger(),
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

  EXPECT_EQ(suggestions[0].type, SuggestionType::kVirtualCreditCardEntry);
  if (GetBenefitSource() == "curinos" &&
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
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://random-url.com"));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

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

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          AttemptToGetEligibleCreditCardBenefitCategory)
      .WillByDefault(testing::Return(
          CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

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
  if (!std::holds_alternative<CreditCardFlatRateBenefit>(GetBenefit())) {
    GTEST_SKIP() << "This test should not run for non-flat-rate benefits.";
  }

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFlatRateBenefitSuggestionLabelsForUrl)
      .WillByDefault(testing::Return(true));
  std::vector<CreditCard> cards = {card()};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

  // Benefit description is not returned for flat rate benefits.
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
  EXPECT_CALL(credit_card_form_event_logger(),
              OnMetadataLoggingContextReceived(
                  Field(&autofill_metrics::CardMetadataLoggingContext::
                            instrument_ids_to_available_benefit_sources,
                        expected_instrument_ids_to_available_benefit_sources)))
      .Times(1);

  GetCreditCardSuggestionsForTouchToFill(cards, autofill_manager(),
                                         test::MakeFormGlobalId());
}

#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(CreditCardSuggestionGeneratorTest,
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
      autofill_client(), FormFieldData(), UNKNOWN_TYPE,
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
TEST_F(CreditCardSuggestionGeneratorTest, NoPrefixMatchingForCreditCards) {
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
        autofill_client(), field, CREDIT_CARD_NUMBER,
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
TEST_F(CreditCardSuggestionGeneratorTest,
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
        autofill_client(), field, CREDIT_CARD_VERIFICATION_CODE,
        /*suppress_disused_cards=*/false,
        /*prefix_match=*/true,
        /*require_non_empty_value_on_trigger_field=*/true,
        /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u""), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"1"), ElementsAre(credit_card));
  EXPECT_THAT(get_cards(u"2"), ElementsAre(credit_card));
}

// Tests that all the credit card suggestions are shown when a credit card field
// was autofilled and focused if `kAutofillPaymentsFieldSwapping` is enabled.
TEST_F(CreditCardSuggestionGeneratorTest, PaymentsFieldSwapping) {
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
                       bool is_autofilled_according_to_renderer) {
    FormFieldData field;
    field.set_is_autofilled_according_to_renderer(
        is_autofilled_according_to_renderer);
    field.set_value(std::move(field_value));
    return GetOrderedCardsToSuggestForTest(
        autofill_client(), field, type,
        /*suppress_disused_cards=*/false,
        /*prefix_match=*/true,
        /*require_non_empty_value_on_trigger_field=*/true,
        /*include_virtual_cards=*/false);
  };

  EXPECT_THAT(get_cards(u"", CREDIT_CARD_NUMBER,
                        /*is_autofilled_according_to_renderer=*/false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1111222233334444", CREDIT_CARD_NUMBER,
                        /*is_autofilled_according_to_renderer=*/true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"4444333322221111", CREDIT_CARD_NUMBER,
                        /*is_autofilled_according_to_renderer=*/true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1111222233334444", CREDIT_CARD_NUMBER,
                        /*is_autofilled_according_to_renderer=*/false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"1", CREDIT_CARD_NUMBER,
                        /*is_autofilled_according_to_renderer=*/true),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"", CREDIT_CARD_NAME_FULL,
                        /*is_autofilled_according_to_renderer=*/false),
              ElementsAre(credit_card2, credit_card1));
  EXPECT_THAT(get_cards(u"Cardholder name", CREDIT_CARD_NAME_FULL,
                        /*is_autofilled_according_to_renderer=*/false),
              ElementsAre(credit_card2, credit_card1));
}

TEST_F(CreditCardSuggestionGeneratorTest, GetServerCardForLocalCard) {
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

// Ensures we appropriately generate suggestions for virtual cards on a
// standalone CVC field.
TEST_F(CreditCardSuggestionGeneratorTest,
       GetVirtualCardStandaloneCvcFieldSuggestions) {
  CreditCard server_card = CreateServerCard();
  payments_data().AddServerCreditCard(server_card);

  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  virtual_card_guid_to_last_four_map.insert(
      {server_card.guid(), VirtualCardUsageData::VirtualCardLastFour(u"1234")});

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE},
                  {.role = CREDIT_CARD_NUMBER, .value = u"1234"}}});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSuggestionGeneratorTest,
       GetVirtualCardStandaloneCvcFieldSuggestions_UndoAutofill) {
  // Set up a virtual card enrolled server card.
  CreditCard server_card = CreateServerCard();
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(server_card);
  // Add another card to ensure multiple cards exist (though only one matches).
  payments_data().AddServerCreditCard(CreateServerCard(
      /*guid=*/"00000000-0000-0000-0000-000000000002",
      /*server_id=*/"server_id2", /*instrument_id=*/2));

  // Create a form with a standalone CVC field on the matching origin.
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
                   .is_autofilled_according_to_renderer = true}},
       .url = "https://example.com"});

  // Add Usage Data matching the card and origin.
  VirtualCardUsageData virtual_card_usage_data(
      VirtualCardUsageData::UsageDataId("usage_data_id_1"),
      VirtualCardUsageData::InstrumentId(server_card.instrument_id()),
      VirtualCardUsageData::VirtualCardLastFour(u"4444"),
      form_bundle.trigger_field.origin());
  payments_data().AddVirtualCardUsageData(virtual_card_usage_data);

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{"4444"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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
TEST_F(CreditCardSuggestionGeneratorTest, GetCardSuggestionsWithCvc) {
  CreditCard card = test::WithCvc(test::GetMaskedServerCard2());
  payments_data().AddServerCreditCard(card);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  const CreditCardSuggestionSummary summary =
      credit_card_form_event_logger()
          .GetCreditCardSuggestionSummaryForTesting();

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(summary.with_cvc);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Ensures we appropriately generate suggestions for card info retrieval
// enrolled card.
TEST_F(CreditCardSuggestionGeneratorTest,
       GetCardSuggestionsWithCardInfoRetrievalEnrolled) {
  CreditCard card = test::GetMaskedServerCardEnrolledIntoRuntimeRetrieval();
  payments_data().AddServerCreditCard(card);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  const CreditCardSuggestionSummary summary =
      credit_card_form_event_logger()
          .GetCreditCardSuggestionSummaryForTesting();

  // There are 3 suggestions, 1 for card info retrieval enrolled card
  // suggestions, followed by a separator, and followed by "Manage payment
  // methods..." which redirects to the Chrome payment methods settings page.
  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_TRUE(summary.with_card_info_retrieval_enrolled);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

// Verifies that the GPay logo is set correctly.
TEST_F(CreditCardSuggestionGeneratorTest, ShouldDisplayGpayLogo) {
  // GPay logo should be displayed if suggestions were all for server cards;
  {
    // Create two server cards.
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000001",
        /*server_id=*/"server_id1", /*instrument_id=*/1));
    payments_data().AddServerCreditCard(CreateServerCard(
        /*guid=*/"00000000-0000-0000-0000-000000000002",
        /*server_id=*/"server_id2", /*instrument_id=*/2));

    FormBundle form_bundle =
        GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

    const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        form_bundle.form, *form_bundle.form_structure,
        form_bundle.trigger_field, *form_bundle.trigger_autofill_field,
        autofill_client(),
        /*four_digit_combinations_in_dom=*/{},
        /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
        credit_card_form_event_logger(),
        AutofillMetrics::PaymentsSigninState::kUnknown,
        /*exclude_virtual_cards=*/false);

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

    FormBundle form_bundle =
        GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

    const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        form_bundle.form, *form_bundle.form_structure,
        form_bundle.trigger_field, *form_bundle.trigger_autofill_field,
        autofill_client(),
        /*four_digit_combinations_in_dom=*/{},
        /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
        credit_card_form_event_logger(),
        AutofillMetrics::PaymentsSigninState::kUnknown,
        /*exclude_virtual_cards=*/false);

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

    FormBundle form_bundle =
        GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

    const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
        form_bundle.form, *form_bundle.form_structure,
        form_bundle.trigger_field, *form_bundle.trigger_autofill_field,
        autofill_client(),
        /*four_digit_combinations_in_dom=*/{},
        /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
        credit_card_form_event_logger(),
        AutofillMetrics::PaymentsSigninState::kUnknown,
        /*exclude_virtual_cards=*/false);

    EXPECT_EQ(suggestions.size(), 3U);
    EXPECT_THAT(suggestions,
                ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
  }
}

TEST_F(CreditCardSuggestionGeneratorTest, NoSuggestionsWhenNoUserData) {
  FormFieldData field;
  field.set_is_autofilled_according_to_renderer(true);
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(CreditCardSuggestionGeneratorTest, ShouldShowScanCreditCard) {
  payments_data().AddCreditCard(test::GetCreditCard());
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(true));

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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

// Test that 'Scan New Card' suggestion is shown based on whether autofill
// credit card is enabled or disabled.
TEST_F(CreditCardSuggestionGeneratorTest,
       ScanCreditCardBasedOnAutofillPreference) {
  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle = GetFormWithTypes({
      .fields = {{.role = CREDIT_CARD_NUMBER}},
      .url = "https://example.com/",
  });

  // Test case where autofill is enabled.
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);
  EXPECT_TRUE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                       *form_bundle.trigger_autofill_field,
                                       autofill_client()));

  // Test case where autofill is disabled.
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(false);
  EXPECT_FALSE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                        *form_bundle.trigger_autofill_field,
                                        autofill_client()));
}

// Test that 'Scan New Card' suggestion is shown based on whether platform
// supports card scanning.
TEST_F(CreditCardSuggestionGeneratorTest,
       ScanCreditCardBasedOnPlatformSupport) {
  FormBundle form_bundle = GetFormWithTypes({
      .fields = {{.role = CREDIT_CARD_NUMBER}},
      .url = "https://example.com/",
  });

  payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);

  // Test case where device and platform support scanning credit cards.
  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(true));
  EXPECT_TRUE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                       *form_bundle.trigger_autofill_field,
                                       autofill_client()));

  // Test case where device and platform do not support scanning credit cards.
  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(false));
  EXPECT_FALSE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                        *form_bundle.trigger_autofill_field,
                                        autofill_client()));
}

// Test that 'Scan New Card' suggestion is shown based on whether form field
// chosen is a credit card number field.
TEST_F(CreditCardSuggestionGeneratorTest,
       ScanCreditCardBasedOnCreditCardNumberField) {
  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(true));
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);

  FormBundle form_bundle = GetFormWithTypes({
      .fields = {{.role = CREDIT_CARD_NUMBER},
                 {.role = CREDIT_CARD_VERIFICATION_CODE}},
      .url = "https://example.com/",
  });

  // Test case for credit-card-number field.
  EXPECT_TRUE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                       *form_bundle.form_structure->field(0),
                                       autofill_client()));

  // Test case for non-credit-card-number field.
  EXPECT_FALSE(ShouldShowScanCreditCard(*form_bundle.form_structure,
                                        *form_bundle.form_structure->field(1),
                                        autofill_client()));
}

// Test that 'Scan New Card' suggestion is shown based on whether the form is
// secure.
TEST_F(CreditCardSuggestionGeneratorTest, ScanCreditCardBasedOnIsFormSecure) {
  ON_CALL(*mock_payments_autofill_client_, HasCreditCardScanFeature)
      .WillByDefault(testing::Return(true));
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(true);

  // Test case for HTTPS form.
  FormBundle https_form_bundle = GetFormWithTypes({
      .fields = {{.role = CREDIT_CARD_NUMBER}},
      .url = "https://example.com/",
  });

  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com/"));
  EXPECT_TRUE(ShouldShowScanCreditCard(
      *https_form_bundle.form_structure,
      *https_form_bundle.form_structure->field(0), autofill_client()));

  // Test case for HTTP form.
  FormBundle http_form_bundle = GetFormWithTypes({
      .fields = {{.role = CREDIT_CARD_NUMBER}},
      .url = "http://example.com/",
  });

  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("http://example.com/"));
  EXPECT_FALSE(ShouldShowScanCreditCard(
      *http_form_bundle.form_structure,
      *http_form_bundle.form_structure->field(0), autofill_client()));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSuggestionGeneratorTest,
       FieldWasAutofilled_UndoAutofillOnCreditCardForm) {
  payments_data().AddCreditCard(test::GetCreditCard());

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER,
                   .is_autofilled_according_to_renderer = true}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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
TEST_F(CreditCardSuggestionGeneratorTest, ShouldShowVirtualCardOption) {
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
      ShouldShowVirtualCardOptionForTest(server_card, autofill_client()));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(local_card, autofill_client()));
}

// Ensures that all suggestions generated by `GetCreditCardFooterSuggestions`
// can be recognized by `IsCreditCardFooterSuggestion.
// False should be returned if the index is out of range.
TEST_F(CreditCardSuggestionGeneratorTest, IsCreditCardFooterSuggestion) {
  std::vector<Suggestion> footer_suggestions =
      GetCreditCardFooterSuggestionsForTest(
          autofill_client(), /*should_show_pay_later_tab_suggestions=*/false,
          /*should_append_bnpl_suggestion=*/false,
          /*should_show_scan_credit_card=*/true, /*is_autofilled=*/true,
          /*with_gpay_logo=*/true, payments::AmountExtractionStatus());

  for (size_t index = 0; index < footer_suggestions.size(); index++) {
    EXPECT_TRUE(IsCreditCardFooterSuggestion(footer_suggestions, index));
  }

  EXPECT_FALSE(IsCreditCardFooterSuggestion(footer_suggestions,
                                            footer_suggestions.size()));
}

// BNPL is currently only available for desktop and android platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
class CreditCardSuggestionGeneratorBnplTest
    : public CreditCardSuggestionGeneratorTest {
 public:
  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableAmountExtraction,
         features::kAutofillEnableBuyNowPayLater,
         features::kAutofillEnableBuyNowPayLaterSyncing},
        /*disabled_features=*/{
            features::kAutofillEnableAiBasedAmountExtraction});

    autofill_client().GetPrefs()->SetBoolean(
        prefs::kAutofillAmountExtractionAiTermsSeen, false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_ANDROID)
// Ensures that the pay over time option is generated with expected content
// and inserted as the last entry before the footer suggestions.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl) {
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
          autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
          /*suppress_disused_cards=*/false,
          /*prefix_match=*/false,
          /*require_non_empty_value_on_trigger_field=*/false,
          /*include_virtual_cards=*/true);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  int64_t extracted_amount_in_micros = 50'000'000;

  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(suggestions,
                                            payments_data().GetBnplIssuers(),
                                            extracted_amount_in_micros);

  // `updated_suggesions` should contains 7 suggestions which are 4 credit
  // card suggestions, 1 BNPL suggestion, 1 separator, and 1 manage card
  // footer.
  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  std::vector<Suggestion>& updated_suggestions =
      update_suggestions_result.suggestions;
  ASSERT_EQ(updated_suggestions.size(), 8U);
  size_t current_suggestion_index = 0;
  // Checks card suggestions stayed in the same order after the insertion.
  for (const CreditCard& card : ordered_cards_for_suggestions) {
    EXPECT_THAT(updated_suggestions[current_suggestion_index++],
                EqualsSuggestion(CreateCreditCardSuggestionForTest(
                    card, autofill_client(), CREDIT_CARD_NUMBER,
                    /*virtual_card_option=*/card.record_type() ==
                        CreditCard::RecordType::kVirtualCard)));
  }
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              EqualsSuggestion(SuggestionType::kSeparator));

  // Checks BNPL suggestion is inserted.
  EXPECT_EQ(updated_suggestions[current_suggestion_index]
                .GetPayload<Suggestion::PaymentsPayload>()
                .extracted_amount_in_micros,
            extracted_amount_in_micros);
  std::vector<BnplIssuer> bnpl_issuers = payments_data().GetBnplIssuers();
  EXPECT_THAT(
      updated_suggestions[current_suggestion_index],
      AllOf(EqualsSuggestion(
                SuggestionType::kBnplEntry,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
                Suggestion::Icon::kBnplGeneric,
                {{Suggestion::Text(l10n_util::GetStringFUTF16(
                    IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL_TWO_ISSUERS,
                    bnpl_issuers[1].GetDisplayName(),
                    bnpl_issuers[0].GetDisplayName()))}}),
            EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_EQ(updated_suggestions[current_suggestion_index++].acceptability,
            Suggestion::Acceptability::kAcceptable);

  // Checks the footer suggestions stayed in the same order after the insertion.
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              AllOf(EqualsSuggestion(SuggestionType::kSeparator),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              AllOf(EqualsSuggestion(SuggestionType::kManageCreditCard),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_EQ(current_suggestion_index, updated_suggestions.size());
}

// Ensures that `GetSuggestionsForBnpl` sets the acceptability to
// `kUnacceptableWithDeactivatedStyle` when there is an amount extraction error.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GetSuggestionsForBnpl_AmountExtractionError) {
  payments::BnplIssuerContext issuer_context(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
      payments::BnplIssuerEligibilityForPage::
          kNotEligibleAmountExtractionErrorUnsupportedCurrency);

  std::vector<Suggestion> suggestions =
      GetSuggestionsForBnpl({issuer_context}, /*app_locale=*/"en-US",
                            /*is_card_number_field_empty=*/true);

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0].acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
}

// Ensures that the separator and pay over time option is generated with
// expected content and inserted as the last entry before the footer
// suggestions.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_SeparatorAndBnplSuggestion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableAmountExtraction,
       features::kAutofillEnableBuyNowPayLater,
       features::kAutofillEnableBuyNowPayLaterSyncing,
       features::
           kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString},
      /*disabled_features=*/{});

  // Add a server card with vcn enrolled.
  payments_data().AddServerCreditCard(
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber());

  BnplIssuer bnpl_issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip);

  // Add BNPL issuers.
  payments_data().AddBnplIssuer(bnpl_issuer);

  std::vector<CreditCard> ordered_cards_for_suggestions =
      GetOrderedCardsToSuggestForTest(
          autofill_client(), FormFieldData(), CREDIT_CARD_NUMBER,
          /*suppress_disused_cards=*/false,
          /*prefix_match=*/false,
          /*require_non_empty_value_on_trigger_field=*/false,
          /*include_virtual_cards=*/true);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  int64_t extracted_amount_in_micros = 50'000'000;

  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(suggestions,
                                            payments_data().GetBnplIssuers(),
                                            extracted_amount_in_micros);

  // `updated_suggesions` should contains 6 suggestions which are 2 credit
  // card suggestions, 1 BNPL suggestion, 2 separators, and 1 manage card
  // footer.
  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  std::vector<Suggestion>& updated_suggestions =
      update_suggestions_result.suggestions;
  ASSERT_EQ(updated_suggestions.size(), 6U);
  size_t current_suggestion_index = 0;
  // Checks card suggestions stayed in the same order after the insertion.
  for (const CreditCard& card : ordered_cards_for_suggestions) {
    EXPECT_THAT(updated_suggestions[current_suggestion_index++],
                EqualsSuggestion(CreateCreditCardSuggestionForTest(
                    card, autofill_client(), CREDIT_CARD_NUMBER,
                    /*virtual_card_option=*/card.record_type() ==
                        CreditCard::RecordType::kVirtualCard)));
  }

  EXPECT_EQ(updated_suggestions[current_suggestion_index++].type,
            SuggestionType::kSeparator);

  // Checks BNPL suggestion is inserted.
  EXPECT_EQ(updated_suggestions[current_suggestion_index]
                .GetPayload<Suggestion::PaymentsPayload>()
                .extracted_amount_in_micros,
            extracted_amount_in_micros);
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              AllOf(EqualsSuggestion(
                        SuggestionType::kBnplEntry,
                        l10n_util::GetStringUTF16(
                            IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
                        Suggestion::Icon::kBnplGeneric,
                        {{Suggestion::Text(bnpl_issuer.GetDisplayName())}}),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));

  // Checks the footer suggestions stayed in the same order after the insertion.
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              AllOf(EqualsSuggestion(SuggestionType::kSeparator),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_THAT(updated_suggestions[current_suggestion_index++],
              AllOf(EqualsSuggestion(SuggestionType::kManageCreditCard),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_EQ(current_suggestion_index, updated_suggestions.size());
}

// Ensures the pay over time option is added if `ShouldAppendBnplOption` returns
// true.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeAppendDesktopSuggestionsWithBnpl_ShouldAppendBnplOption) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableAmountExtraction,
       features::kAutofillEnableBuyNowPayLaterSyncing,
       features::kAutofillEnableBuyNowPayLater,
       features::kAutofillEnableAiBasedAmountExtraction,
       features::
           kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString},
      /*disabled_features=*/{});

  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Setup a scenario where BNPL suggestion should be appended to the credit
  // card suggestions.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kBnplEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
}

// Ensures the pay over time option is added if `ShouldAppendBnplOption` returns
// true, and a separator is not added if the updated suggestion flag is off.
TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    MaybeAppendDesktopSuggestionsWithBnpl_ShouldAppendBnplOption_UpdatedSuggestionFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{
          features::
              kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString});

  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Setup a scenario where BNPL suggestion should be appended to the credit
  // card suggestions.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kBnplEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
}

// Ensures that pay over time option is not added if the card number field is
// not empty.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeAppendDesktopSuggestionsWithBnpl_CardNumberFieldNotEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Setup a scenario where BNPL suggestion should be appended to the credit
  // card suggestions.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER, .value = u"1111"}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
}

// Ensures that pay over time option is not added if `ShouldAppendBnplOption`
// returns false.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeAppendDesktopSuggestionsWithBnpl_ShouldNotAppendBnplOption) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Setup a scenario where BNPL suggestion should NOT be appended to the credit
  // card suggestions. More specifically, here put CVC field as the triggereing
  // field, which should NOT trigger BNPL option appending.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, testing::IsEmpty());
}

// Ensures that pay over time option is not added if the feature flag
// `kAutofillEnableAiBasedAmountExtraction` is disabled.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeAppendDesktopSuggestionsWithBnpl_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater},
      /*disabled_features=*/{features::kAutofillEnableAiBasedAmountExtraction});

  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Setup a scenario where BNPL suggestion should be appended to the credit
  // card suggestions.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
}

// Ensures that the pay over time option is not added if the suggestion list
// is empty.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_EmptySuggestionList) {
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  EXPECT_FALSE(MaybeUpdateDesktopSuggestionsWithBnpl(
                   {}, payments_data().GetBnplIssuers(),
                   /*extracted_amount_in_micros=*/50'000'000)
                   .is_bnpl_suggestion_added);
}

// Ensures that the pay over time option is not added if the suggestion list
// already contains a BNPL suggestion.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_SuggestionListWithBnplInserted) {
  payments_data().AddServerCreditCard(test::GetMaskedServerCard2());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  // Add suggestions with BNPL suggestion inserted to client.
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(
          suggestions, payments_data().GetBnplIssuers(),
          /*extracted_amount_in_micros=*/50'000'000);

  EXPECT_THAT(update_suggestions_result.suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kBnplEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
  EXPECT_FALSE(MaybeUpdateDesktopSuggestionsWithBnpl(
                   update_suggestions_result.suggestions,
                   payments_data().GetBnplIssuers(),
                   /*extracted_amount_in_micros=*/50'000'000)
                   .is_bnpl_suggestion_added);
}

TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_IphBubbleNotShown_IssuerMissing) {
  // Add a server card.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add BNPL issuers.
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(
          suggestions, payments_data().GetBnplIssuers(),
          /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  Suggestion bnpl_suggestion = *std::ranges::find_if(
      update_suggestions_result.suggestions,
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  EXPECT_EQ(bnpl_suggestion.iph_metadata.feature, nullptr);
}

TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_IphBubble_KlarnaDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/
      {features::kAutofillEnableBuyNowPayLaterForKlarna});

  // Add a server card.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add BNPL issuers.
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(
          suggestions, payments_data().GetBnplIssuers(),
          /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  Suggestion bnpl_suggestion = *std::ranges::find_if(
      update_suggestions_result.suggestions,
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  EXPECT_EQ(bnpl_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillBnplAffirmOrZipSuggestionFeature);
}

TEST_F(CreditCardSuggestionGeneratorBnplTest,
       MaybeUpdateDesktopSuggestionsWithBnpl_IphBubble_KlarnaEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForKlarna},
      /*disabled_features=*/{});

  // Add a server card.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add BNPL issuers.
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(
          suggestions, payments_data().GetBnplIssuers(),
          /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  Suggestion bnpl_suggestion = *std::ranges::find_if(
      update_suggestions_result.suggestions,
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  EXPECT_EQ(
      bnpl_suggestion.iph_metadata.feature,
      &feature_engagement::kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    MaybeUpdateDesktopSuggestionsWithBnpl_IphBubble_KlarnaEnabled_KlarnaMissing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLaterForKlarna},
      /*disabled_features=*/{});

  // Add a server card.
  payments_data().AddServerCreditCard(test::GetMaskedServerCardAmex());
  // Add BNPL issuers.
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  BnplSuggestionUpdateResult update_suggestions_result =
      MaybeUpdateDesktopSuggestionsWithBnpl(
          suggestions, payments_data().GetBnplIssuers(),
          /*extracted_amount_in_micros=*/50'000'000);

  ASSERT_TRUE(update_suggestions_result.is_bnpl_suggestion_added);
  Suggestion bnpl_suggestion = *std::ranges::find_if(
      update_suggestions_result.suggestions,
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  EXPECT_EQ(bnpl_suggestion.iph_metadata.feature,
            &feature_engagement::kIPHAutofillBnplAffirmOrZipSuggestionFeature);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_CardNumberFieldNotEmpty_PayLaterIssuerDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAiBasedAmountExtraction,
                            features::kAutofillEnablePayNowPayLaterTabs},
      /*disabled_features=*/{});

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, false);

  payments_data().AddCreditCard(test::GetCreditCard());
  BnplIssuer linked_issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip);
  payments_data().AddBnplIssuer(linked_issuer);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER, .value = u"1111"}}});

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto disabled_bnpl_suggestion = std::find_if(
      suggestions.begin(), suggestions.end(),
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  ASSERT_NE(disabled_bnpl_suggestion, suggestions.end())
      << "Expected a BNPL suggestion to be generated.";

  EXPECT_EQ(disabled_bnpl_suggestion->main_text.value,
            linked_issuer.GetDisplayName());
  EXPECT_EQ(disabled_bnpl_suggestion->icon, Suggestion::Icon::kBnplZip);
  EXPECT_THAT(
      disabled_bnpl_suggestion->labels,
      ElementsAre(std::vector<Suggestion::Text>{
          Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_CARD_BNPL_PAY_LATER_CLEAR_FORM_TO_ENABLE))}));
  EXPECT_EQ(disabled_bnpl_suggestion->acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
  EXPECT_TRUE(std::holds_alternative<Suggestion::BnplIssuer>(
      disabled_bnpl_suggestion->payload));
  EXPECT_EQ(std::get<Suggestion::BnplIssuer>(disabled_bnpl_suggestion->payload)
                .value(),
            linked_issuer);
  EXPECT_EQ(disabled_bnpl_suggestion->tab_index, kPayLaterSuggestionTabIndex);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_CardNumberFieldNotEmpty_AiTermsSeen_ShowsDisabledPayLaterIssuer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAiBasedAmountExtraction,
                            features::kAutofillEnablePayNowPayLaterTabs},
      /*disabled_features=*/{});

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, true);

  payments_data().AddCreditCard(test::GetCreditCard());
  BnplIssuer linked_issuer =
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip);
  payments_data().AddBnplIssuer(linked_issuer);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER, .value = u"1111"}}});

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto loading_throbber_finder = std::find_if(
      suggestions.begin(), suggestions.end(), [](const Suggestion& s) {
        return s.type == SuggestionType::kLoadingThrobber;
      });

  EXPECT_EQ(loading_throbber_finder, suggestions.end())
      << "Expected no Loading Throbber suggestion to be generated when "
         "card number field is not empty.";

  auto disabled_bnpl_suggestion = std::find_if(
      suggestions.begin(), suggestions.end(),
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  ASSERT_NE(disabled_bnpl_suggestion, suggestions.end())
      << "Expected a BNPL suggestion to be generated.";

  EXPECT_EQ(disabled_bnpl_suggestion->acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_PayLaterTabsEnabled_AiTermsNotSeen_MultipleIssuers) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableBuyNowPayLaterForKlarna,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, false);

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna));
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  ASSERT_EQ(suggestions.size(), 7U);

  EXPECT_THAT(suggestions[0],
              AllOf(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                    EqualSuggestionTabIndex(kDefaultSuggestionTabIndex)));
  EXPECT_THAT(suggestions[1],
              AllOf(EqualsSuggestion(SuggestionType::kBnplEntry),
                    EqualSuggestionTabIndex(kPayLaterSuggestionTabIndex)));
  EXPECT_THAT(suggestions[2],
              AllOf(EqualsSuggestion(SuggestionType::kBnplEntry),
                    EqualSuggestionTabIndex(kPayLaterSuggestionTabIndex)));
  EXPECT_THAT(suggestions[3],
              AllOf(EqualsSuggestion(SuggestionType::kBnplEntry),
                    EqualSuggestionTabIndex(kPayLaterSuggestionTabIndex)));
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false,
                                                  /*with_bnpl_footnote=*/true));
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_PayLaterTabsEnabled_AiTermsSeen_LoadingThrobber) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableBuyNowPayLaterForKlarna,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, true);

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna));
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto loading_throbber_finder = std::find_if(
      suggestions.begin(), suggestions.end(), [](const Suggestion& s) {
        return s.type == SuggestionType::kLoadingThrobber;
      });

  ASSERT_NE(loading_throbber_finder, suggestions.end())
      << "Expected a Loading Throbber suggestion to be generated.";

  EXPECT_EQ(loading_throbber_finder->acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_EQ(loading_throbber_finder->tab_index, kPayLaterSuggestionTabIndex);

  // 3 BNPL issuers were added.
  EXPECT_EQ(loading_throbber_finder->expected_number_of_suggestions, 3u);

  bool has_bnpl_entry = std::any_of(
      suggestions.begin(), suggestions.end(),
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });
  EXPECT_FALSE(has_bnpl_entry);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_PayLaterTabsDisabled_BnplFooterSuggestionAdded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableBuyNowPayLaterForKlarna,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs});

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm));
  payments_data().AddBnplIssuer(
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna));
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  ASSERT_EQ(suggestions.size(), 5U);
  EXPECT_THAT(suggestions,
              ElementsAre(EqualsSuggestion(SuggestionType::kCreditCardEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kBnplEntry),
                          EqualsSuggestion(SuggestionType::kSeparator),
                          EqualsSuggestion(SuggestionType::kManageCreditCard)));
}

TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GenerateSuggestions_CardNumberFieldEmpty_UsesCacheIfAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> cached_suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kLoadingThrobber),
      Suggestion(SuggestionType::kBnplFootnote),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageCreditCard)};

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  payments::test_api(*bnpl_manager).SetCachedSuggestions(cached_suggestions);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  // Without the cache, this would generate `kBnplEntry` suggestions for each
  // BnplIssuer instead of a `kLoadingThrobber` suggestion.
  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, false);
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // Check that we reuse the cached `kLoadingThrobber` suggestion instead of
  // generating `kBnplEntry` suggestions.
  ASSERT_EQ(suggestions.size(), 5U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kLoadingThrobber);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kBnplFootnote);
  EXPECT_EQ(suggestions[3].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[4].type, SuggestionType::kManageCreditCard);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateSuggestions_CardNumberFieldNotEmpty_CancelsOngoingRequestsAndIgnoresCache) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableBuyNowPayLaterSyncing,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> cached_suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry),
      Suggestion(SuggestionType::kLoadingThrobber),
      Suggestion(SuggestionType::kBnplFootnote),
      Suggestion(SuggestionType::kSeparator),
      Suggestion(SuggestionType::kManageCreditCard)};

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  payments::test_api(*bnpl_manager).SetCachedSuggestions(cached_suggestions);

  EXPECT_CALL(*bnpl_manager, CancelOngoingRequests);

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER, .value = u"4111"}}});

  autofill_client().GetPrefs()->SetBoolean(
      prefs::kAutofillAmountExtractionAiTermsSeen, false);
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // Check that we ignore the cached suggestions and generate a disabled
  // `kBnplEntry` suggestion instead when the card number field is not empty.
  ASSERT_EQ(suggestions.size(), 5U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kBnplEntry);
  EXPECT_EQ(suggestions[1].acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kBnplFootnote);
  EXPECT_EQ(suggestions[3].type, SuggestionType::kSeparator);
  EXPECT_EQ(suggestions[4].type, SuggestionType::kManageCreditCard);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_PayLaterTabsEnabled_BnplFootnote) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                            features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto bnpl_footnote = std::find_if(
      suggestions.begin(), suggestions.end(), [](const Suggestion& s) {
        return s.type == SuggestionType::kBnplFootnote;
      });

  ASSERT_NE(bnpl_footnote, suggestions.end())
      << "Expected a BNPL footnote suggestion to be generated.";

  EXPECT_EQ(bnpl_footnote->acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_EQ(bnpl_footnote->tab_index, kPayLaterSuggestionTabIndex);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GenerateCreditCardOrCvcFieldSuggestionsSync_PayLaterTabsDisabled_NoBnplFootnote) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillEnableBuyNowPayLater,
                            features::kAutofillEnableAmountExtraction,
                            features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs});

  payments_data().AddCreditCard(test::GetCreditCard());
  payments_data().AddBnplIssuer(test::GetTestLinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_FALSE(std::any_of(suggestions.begin(), suggestions.end(),
                           [](const Suggestion& s) {
                             return s.type == SuggestionType::kBnplFootnote;
                           }));
}

// Params of CreditCardSuggestionGeneratorPnplTabTestForIssuer:
// -- BnplIssuer::IssuerId IssuerId: The Buy Now Pay Later issuer which the
// suggestion is generated for.
// -- int ExpectedLabelTextId: The id of the expected text that should be
// displayed in the suggestion label.
class CreditCardSuggestionGeneratorPnplTabTestForIssuer
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<
          std::tuple<BnplIssuer::IssuerId, int>> {
 public:
  BnplIssuer::IssuerId IssuerId() { return std::get<0>(GetParam()); }
  Suggestion::Text ExpectedLabelText() {
    return Suggestion::Text(l10n_util::GetStringUTF16(std::get<1>(GetParam())));
  }

  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillEnablePayNowPayLaterTabs,
                              features::kAutofillEnableBuyNowPayLater,
                              features::kAutofillEnableBuyNowPayLaterForKlarna,
                              features::kAutofillEnableAmountExtraction,
                              features::kAutofillEnableAiBasedAmountExtraction},
        /*disabled_features=*/{});
    autofill_client().GetPrefs()->SetBoolean(
        prefs::kAutofillAmountExtractionAiTermsSeen, false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    CreditCardSuggestionGeneratorPnplTabTestForIssuer,
    testing::Values(
        std::make_tuple(
            BnplIssuer::IssuerId::kBnplAffirm,
            IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY),
        std::make_tuple(BnplIssuer::IssuerId::kBnplZip,
                        IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_ZIP),
        std::make_tuple(
            BnplIssuer::IssuerId::kBnplKlarna,
            IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_KLARNA)));

TEST_P(CreditCardSuggestionGeneratorPnplTabTestForIssuer,
       GenerateCreditCardOrCvcFieldSuggestionsSync_LinkedIssuer) {
  payments_data().AddCreditCard(test::GetCreditCard());
  BnplIssuer issuer = test::GetTestLinkedBnplIssuer(IssuerId());
  payments_data().AddBnplIssuer(issuer);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  std::unique_ptr<MockBnplManager> bnpl_manager =
      std::make_unique<NiceMock<MockBnplManager>>(&autofill_manager());
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, bnpl_manager.get(),
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto bnpl_it = std::find_if(
      suggestions.begin(), suggestions.end(),
      [](const Suggestion& s) { return s.type == SuggestionType::kBnplEntry; });

  ASSERT_NE(bnpl_it, suggestions.end())
      << "Expected a BNPL suggestion to be generated.";

  EXPECT_EQ(bnpl_it->main_text.value, issuer.GetDisplayName());
  EXPECT_EQ(bnpl_it->icon, payments::GetBnplSuggestionIcon(IssuerId()));
  EXPECT_THAT(bnpl_it->labels,
              ElementsAre(std::vector<Suggestion::Text>{ExpectedLabelText()}));
  EXPECT_TRUE(std::holds_alternative<Suggestion::BnplIssuer>(bnpl_it->payload));
  EXPECT_EQ(std::get<Suggestion::BnplIssuer>(bnpl_it->payload).value(), issuer);
  EXPECT_EQ(bnpl_it->tab_index, kPayLaterSuggestionTabIndex);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Ensures that `GetBnplPriceLowerBound()` returns the minimum lower price
// bound among all given issuers.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
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
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GetBnplPriceLowerBound_AmountWithMoreThanNintyNineCents) {
  std::vector<BnplIssuer> bnpl_issuers = {BnplIssuer(
      /*instrument_id=*/5678, BnplIssuer::IssuerId::kBnplAffirm,
      {BnplIssuer::EligiblePriceRange("USD",
                                      /*price_lower_bound=*/34'996'666,
                                      /*price_upper_bound=*/200'000'000)})};

  EXPECT_EQ(GetBnplPriceLowerBoundForTest(bnpl_issuers), u"$35");
}

// Verifies that `GetLoadingSuggestionForPayLaterTab()` creates suggestions with
// expected values.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GetLoadingSuggestionForPayLaterTab) {
  Suggestion loading_suggestion = GetLoadingSuggestionForPayLaterTab(2);

  EXPECT_EQ(loading_suggestion.type, SuggestionType::kLoadingThrobber);
  EXPECT_EQ(loading_suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptable);
  EXPECT_EQ(loading_suggestion.tab_index, kPayLaterSuggestionTabIndex);
  EXPECT_EQ(loading_suggestion.expected_number_of_suggestions, 2u);
}

#if BUILDFLAG(IS_ANDROID)
// Verifies that a BNPL suggestion is added to Touch to Fill suggestions when
// BNPL is eligible and there are credit card suggestions.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GetCreditCardSuggestionsForTouchToFill_BnplSuggestionAdded) {
  BnplIssuer bnpl_issuer = test::GetTestUnlinkedBnplIssuer();
  payments_data().AddBnplIssuer(bnpl_issuer);

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      {CreateServerCard(), CreateLocalCard()}, autofill_manager(),
      test::MakeFormGlobalId());

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kBnplEntry);
  EXPECT_THAT(
      suggestions[2],
      EqualsSuggestion(
          SuggestionType::kBnplEntry,
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          Suggestion::Icon::kBnplGeneric,
          {{Suggestion::Text(bnpl_issuer.GetDisplayName())}}));
  EXPECT_TRUE(payments_data().IsAutofillHasSeenBnplPrefEnabled());
}

// Verifies that a BNPL suggestion is not added to Touch to Fill suggestions
// when BNPL is not eligible.
TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_BnplSuggestionNotAdded_BnplNotEligible) {
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(false));

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      {CreateServerCard()}, autofill_manager(), test::MakeFormGlobalId());

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
}

// Verifies that a BNPL suggestion is not added to Touch to Fill suggestions
// when `kAutofillEnableBuyNowPayLater` is disabled.
TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_BnplSuggestionNotAdded_FlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableBuyNowPayLater);

  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());

  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      {CreateServerCard()}, autofill_manager(), test::MakeFormGlobalId());

  ASSERT_EQ(suggestions.size(), 1U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
}

// Verifies that OnBnplSuggestionShown is called when a BNPL suggestion is added
// to Touch to Fill suggestions.
TEST_F(CreditCardSuggestionGeneratorBnplTest,
       GetCreditCardSuggestionsForTouchToFill_OnBnplSuggestionShownCalled) {
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(credit_card_form_event_logger(), OnBnplSuggestionShown())
      .Times(1);

  GetCreditCardSuggestionsForTouchToFill(/*credit_cards=*/{CreateServerCard()},
                                         autofill_manager(),
                                         test::MakeFormGlobalId());
}

// Verifies that OnBnplSuggestionShown is not called when a BNPL suggestion is
// not added to Touch to Fill suggestions.
TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_BnplSuggestionNotShown_NotLogged) {
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(false));

  EXPECT_CALL(credit_card_form_event_logger(), OnBnplSuggestionShown())
      .Times(0);

  GetCreditCardSuggestionsForTouchToFill(/*credit_cards=*/{CreateServerCard()},
                                         autofill_manager(),
                                         test::MakeFormGlobalId());
}

// Verifies that OnBnplSuggestionShown is not called when a BNPL suggestion is
// not added to Touch to Fill suggestions because the
// `kAutofillEnableBuyNowPayLater` feature flag is disabled.
TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_BnplSuggestionNotShown_NotLogged_FlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutofillEnableBuyNowPayLater});

  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(credit_card_form_event_logger(), OnBnplSuggestionShown())
      .Times(0);

  GetCreditCardSuggestionsForTouchToFill(/*credit_cards=*/{CreateServerCard()},
                                         autofill_manager(),
                                         test::MakeFormGlobalId());
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_EmptyCardNumberField_IncludesBnpl) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableBuyNowPayLater,
       features::kAutofillEnableAmountExtraction,
       features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());
  FormData form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FULL,
                   .value = u"Card Name",
                   .is_autofilled_according_to_renderer = true},
                  {.role = CREDIT_CARD_NUMBER, .value = u""}}});
  autofill_manager().AddSeenForm(form,
                                 {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER});
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      {CreateServerCard(), CreateLocalCard()}, autofill_manager(),
      form.global_id());

  ASSERT_EQ(suggestions.size(), 3U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[2].type, SuggestionType::kBnplEntry);
}

TEST_F(
    CreditCardSuggestionGeneratorBnplTest,
    GetCreditCardSuggestionsForTouchToFill_NonEmptyCardNumberField_ExcludesBnpl) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {features::kAutofillEnableBuyNowPayLater,
       features::kAutofillEnableAmountExtraction,
       features::kAutofillEnableAiBasedAmountExtraction},
      /*disabled_features=*/{});
  payments_data().AddBnplIssuer(test::GetTestUnlinkedBnplIssuer());
  FormData form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FULL,
                   .value = u"Card Name",
                   .is_autofilled_according_to_renderer = true},
                  {.role = CREDIT_CARD_NUMBER, .value = u"01230123012399"}}});
  autofill_manager().AddSeenForm(form,
                                 {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER});
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          IsUrlEligibleForBnplIssuer)
      .WillByDefault(testing::Return(true));

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      {CreateServerCard(), CreateLocalCard()}, autofill_manager(),
      form.global_id());

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(suggestions[0].type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(suggestions[1].type, SuggestionType::kCreditCardEntry);
}

#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

TEST_F(CreditCardSuggestionGeneratorTest, CreateBnplSuggestion_OneIssuer) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString};

  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000);

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*type=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric,
          /*labels=*/
          {{Suggestion::Text(bnpl_issuers[0].GetDisplayName())}}));
  EXPECT_EQ(suggestion.acceptability, Suggestion::Acceptability::kAcceptable);
}

TEST_F(CreditCardSuggestionGeneratorTest, CreateBnplSuggestion_TwoIssuers) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString};

  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000);

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*type=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric,
          /*labels=*/
          {{Suggestion::Text(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL_TWO_ISSUERS,
              // Affirm comes before Zip.
              bnpl_issuers[1].GetDisplayName(),
              bnpl_issuers[0].GetDisplayName()))}}));
  EXPECT_EQ(suggestion.acceptability, Suggestion::Acceptability::kAcceptable);
}

TEST_F(CreditCardSuggestionGeneratorTest, CreateBnplSuggestion_ThreeIssuers) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString};

  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna),
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000);

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*type=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric,
          /*labels=*/
          {{Suggestion::Text(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL_THREE_ISSUERS,
              // Affirm comes before Klarna, and Klarna comes before Zip.
              bnpl_issuers[2].GetDisplayName(),
              bnpl_issuers[0].GetDisplayName(),
              bnpl_issuers[1].GetDisplayName()))}}));
  EXPECT_EQ(suggestion.acceptability, Suggestion::Acceptability::kAcceptable);
}

TEST_F(CreditCardSuggestionGeneratorTest,
       CreateBnplSuggestion_DeactivatedStyle_Timeout) {
  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000,
      payments::AmountExtractionStatus{
          .has_timed_out_for_page_load = true,
          .seen_unsupported_currency_for_page_load = false});

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*id=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric));
  EXPECT_EQ(suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
}

TEST_F(CreditCardSuggestionGeneratorTest,
       CreateBnplSuggestion_DeactivatedStyle_UnsupportedCurrency) {
  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000,
      payments::AmountExtractionStatus{
          .has_timed_out_for_page_load = false,
          .seen_unsupported_currency_for_page_load = true});

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*id=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric));
  EXPECT_EQ(suggestion.acceptability,
            Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle);
}

TEST_F(CreditCardSuggestionGeneratorTest, CreateBnplSuggestion_FlagDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString);

  std::vector<BnplIssuer> bnpl_issuers = {
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplKlarna),
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplZip),
      test::GetTestLinkedBnplIssuer(BnplIssuer::IssuerId::kBnplAffirm)};
  Suggestion suggestion = CreateBnplSuggestion(
      bnpl_issuers, /*extracted_amount_in_micros=*/55'000'000);

  EXPECT_THAT(
      suggestion,
      EqualsSuggestion(
          /*type=*/SuggestionType::kBnplEntry,
          /*main_text=*/
          l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
          /*icon=*/Suggestion::Icon::kBnplGeneric,
          /*labels=*/
          {{Suggestion::Text(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL, u"$50"))}}));
}

// Test that the virtual card option is shown when the autofill optimization
// guide is not present.
TEST_F(CreditCardSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_AutofillOptimizationGuideDeciderNotPresent) {
  // Create a server card.
  CreditCard server_card =
      CreateServerCard(/*guid=*/"00000000-0000-0000-0000-000000000001");
  server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  payments_data().AddServerCreditCard(server_card);
  autofill_client().ResetAutofillOptimizationGuideDecider();

  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // If all prerequisites are met, it should return true.
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(server_card, autofill_client()));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(local_card, autofill_client()));
}

// Test that the virtual card option is shown even if the merchant is opted-out
// of virtual cards.
TEST_F(CreditCardSuggestionGeneratorTest,
       ShouldShowVirtualCardOption_InDisabledStateForOptedOutMerchants) {
  // Create an enrolled server card.
  CreditCard server_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  payments_data().AddServerCreditCard(server_card);

  // Even if the URL is opted-out of virtual cards for `server_card`, display
  // the virtual card suggestion.
  ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
              autofill_client().GetAutofillOptimizationGuideDecider()),
          ShouldBlockFormFieldSuggestion)
      .WillByDefault(testing::Return(true));
  EXPECT_TRUE(
      ShouldShowVirtualCardOptionForTest(server_card, autofill_client()));
}

// Test that the virtual card option is not shown if the server card we might be
// showing a virtual card option for is not enrolled into virtual card.
TEST_F(CreditCardSuggestionGeneratorTest,
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
      ShouldShowVirtualCardOptionForTest(server_card, autofill_client()));
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(local_card, autofill_client()));
}

// Test that the virtual card option is not shown for a local card with no
// server card duplicate.
TEST_F(CreditCardSuggestionGeneratorTest,
       ShouldNotShowVirtualCardOption_LocalCardWithoutServerCardDuplicate) {
  // Create a local card with same information.
  CreditCard local_card =
      CreateLocalCard(/*guid=*/"00000000-0000-0000-0000-000000000002");

  // The local card does not have a server duplicate, should return false.
  EXPECT_FALSE(
      ShouldShowVirtualCardOptionForTest(local_card, autofill_client()));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
TEST_F(CreditCardSuggestionGeneratorTest,
       GenerateLocalSaveAndFillSuggestion_CreditCardUploadDisabled) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/false);

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager, ShouldBlockFeature())
      .WillOnce(testing::Return(false));

  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // `suggestions` should contain 3 suggestions which are save and fill
  // suggestion, separator, and manage cards footer.
  ASSERT_GE(suggestions.size(), 3ul);
  EXPECT_THAT(suggestions[0],
#if BUILDFLAG(IS_IOS)
              EqualsSuggestion(SuggestionType::kSaveAndFillCreditCardEntry)
#else
              EqualsSuggestion(
                  SuggestionType::kSaveAndFillCreditCardEntry,
                  l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
                  Suggestion::Icon::kSaveAndFill,
                  {{Suggestion::Text(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_LOCAL_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}})
#endif  // BUILDFLAG(IS_IOS)
  );
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

TEST_F(CreditCardSuggestionGeneratorTest,
       GenerateServerSaveAndFillSuggestion_CreditCardUploadEnabled) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/true);

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager, ShouldBlockFeature())
      .WillOnce(testing::Return(false));

  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // `suggestions` should contain 3 suggestions which are save and fill
  // suggestion, separator, and manage cards footer.
  ASSERT_GE(suggestions.size(), 3ul);
  EXPECT_THAT(suggestions[0],
#if BUILDFLAG(IS_IOS)
              EqualsSuggestion(SuggestionType::kSaveAndFillCreditCardEntry)
#else
              EqualsSuggestion(
                  SuggestionType::kSaveAndFillCreditCardEntry,
                  l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
                  Suggestion::Icon::kSaveAndFill,
                  {{Suggestion::Text(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_SERVER_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}})
#endif  // BUILDFLAG(IS_IOS)
  );
  EXPECT_THAT(suggestions, ContainsCreditCardFooterSuggestions(
                               /*with_gpay_logo=*/!BUILDFLAG(IS_IOS)));
}

TEST_F(CreditCardSuggestionGeneratorTest,
       GenerateLocalSaveAndFillSuggestion_FlagDisabled) {
  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  ASSERT_GE(suggestions.size(), 0ul);
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenCreditCardIsSavedInProfile) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager,
              MaybeLogSaveAndFillSuggestionNotShownReason(
                  autofill_metrics::SaveAndFillSuggestionNotShownReason::
                      kHasSavedCards))
      .Times(1);

  payments_data().AddCreditCard(test::GetCreditCard());
  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_EQ(suggestions.size(), 3ul);
  EXPECT_THAT(suggestions[0],
              EqualsSuggestion(SuggestionType::kCreditCardEntry));
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenCreditCardFormIsIncomplete) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager,
              MaybeLogSaveAndFillSuggestionNotShownReason(
                  autofill_metrics::SaveAndFillSuggestionNotShownReason::
                      kIncompleteCreditCardForm))
      .Times(1);

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenIncognito) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)
  autofill_client().set_is_off_the_record(true);

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager,
              MaybeLogSaveAndFillSuggestionNotShownReason(
                  autofill_metrics::SaveAndFillSuggestionNotShownReason::
                      kUserInIncognito))
      .Times(1);

  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenFieldHasMoreThanThreeChars) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"4111"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NotOfferedWhenCachedFormAndFieldUnavailable) {
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
           {.role = FieldType::CREDIT_CARD_NUMBER},
           {.role = FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
       }});

  std::vector<Suggestion> suggestions;
  CreditCardSuggestionGenerator credit_card_suggestion_generator(
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      &credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions` will hold the correct value.
  credit_card_suggestion_generator.GenerateSuggestions(
      form_bundle.form, form_bundle.trigger_field, /*form_structure=*/nullptr,
      /*trigger_autofill_field=*/nullptr, autofill_client(),
      on_suggestions_generated);

  EXPECT_THAT(suggestions, IsEmpty());
}

TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_NoOfferedWhenSaveAndFillFeatureIsBlocked) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/true);

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager, ShouldBlockFeature())
      .WillOnce(testing::Return(true));

  EXPECT_CALL(mock_save_and_fill_manager,
              MaybeLogSaveAndFillSuggestionNotShownReason(
                  autofill_metrics::SaveAndFillSuggestionNotShownReason::
                      kBlockedByStrikeDatabase))
      .Times(1);

  ASSERT_FALSE(autofill_client().IsOffTheRecord());
  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(suggestions, IsEmpty());
}

// Verify "Save and Fill" suggestion is offered when the user is not in
// incognito mode, has no saved credit cards, the credit card form is complete
// (has CVC, cardholder name, card number, and expiration date fields), and a
// field within the form group is focused with no more than 3 characters
// entered, and the strike database limit is not exceeded.
TEST_F(CreditCardSuggestionGeneratorTest,
       SaveAndFillSuggestion_OfferedWhenCriteriaMet) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableBottomSheetScanCardAndFill);
#else
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSaveAndFill);
#endif  // BUILDFLAG(IS_IOS)
  SetCreditCardUploadEnabledForTest(/*credit_card_upload_enabled=*/true);

  MockSaveAndFillManager& mock_save_and_fill_manager =
      static_cast<MockSaveAndFillManager&>(
          *payments_autofill_client().GetSaveAndFillManager());

  EXPECT_CALL(mock_save_and_fill_manager, ShouldBlockFeature())
      .WillOnce(testing::Return(false));

  // Verify user is not in incognito mode.
  ASSERT_FALSE(autofill_client().IsOffTheRecord());
  // Complete credit card form (passes FormStructure::IsCompleteCreditCardForm)
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_NUMBER, .value = u"411"},
           {.role = FieldType::CREDIT_CARD_EXP_MONTH},
           {.role = FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR},
           {.role = FieldType::CREDIT_CARD_VERIFICATION_CODE},
           {.role = FieldType::CREDIT_CARD_NAME_FULL},
       }});
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(
      suggestions,
      ElementsAre(EqualsSuggestion(SuggestionType::kSaveAndFillCreditCardEntry),
                  EqualsSuggestion(SuggestionType::kSeparator),
                  EqualsManagePaymentsMethodsSuggestion(
                      /*with_gpay_logo=*/!BUILDFLAG(IS_IOS))));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
TEST_F(CreditCardSuggestionGeneratorTest, CreateSaveAndFillSuggestion_IOS) {
  bool display_gpay_logo = false;
  Suggestion suggestion =
      CreateSaveAndFillSuggestion(autofill_client(), display_gpay_logo);

  EXPECT_EQ(suggestion.type, SuggestionType::kSaveAndFillCreditCardEntry);
  EXPECT_TRUE(suggestion.main_text.value.empty());
  EXPECT_TRUE(suggestion.labels.empty());
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kNoIcon);
  EXPECT_FALSE(display_gpay_logo);
}
#endif  // BUILDFLAG(IS_IOS)

// This class helps test the credit card contents that are displayed in
// Autofill suggestions. It covers suggestions on Desktop/Android dropdown,
// and on Android keyboard accessory.
class AutofillCreditCardSuggestionContentTest
    : public CreditCardSuggestionGeneratorTest {
 public:
  AutofillCreditCardSuggestionContentTest() = default;

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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true);

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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true);

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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/false);

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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/false);

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
  autofill_client().set_is_cvc_saving_supported(true);
  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  // We used last 4 to deduplicate local card and server card so we should set
  // local card with different last 4.
  local_card.SetNumber(u"5454545454545454");
  payments_data().AddCreditCard(std::move(local_card));
  payments_data().AddServerCreditCard(CreateServerCard());

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // Both local card and server card suggestion should be shown when CVC field
  // is focused.
  ASSERT_EQ(suggestions.size(), 4U);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC for Visa");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC for Mastercard");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ(suggestions[0].main_text.value, u"Security code");
  EXPECT_EQ(suggestions[1].main_text.value, u"Security code");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#else  // For all other platforms
  EXPECT_EQ(suggestions[0].main_text.value, u"CVC");
  EXPECT_EQ(suggestions[1].main_text.value, u"CVC");
  EXPECT_TRUE(suggestions[0].minor_texts.empty());
  EXPECT_TRUE(suggestions[1].minor_texts.empty());
#endif
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/false));
}

#if BUILDFLAG(IS_IOS)
// Verifies that no CVC suggestions are generated in an iOS WebView.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_CvcField_UnsupportedClient) {
  // Simulate the iOS WebView context.
  autofill_client().set_is_cvc_saving_supported(false);

  // Create one server card and one local card with CVC.
  CreditCard local_card = CreateLocalCard();
  local_card.SetNumber(u"5454545454545454");
  payments_data().AddCreditCard(std::move(local_card));
  payments_data().AddServerCreditCard(CreateServerCard());

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // No suggestions should be returned in an iOS WebView for a CVC field.
  EXPECT_THAT(suggestions, IsEmpty());
}
#endif

// Verify that the suggestion's texts are populated correctly for a duplicate
// local and server card suggestion when the CVC field is focused.
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_Duplicate_CvcField) {
  autofill_client().set_is_cvc_saving_supported(true);
  // Create 2 duplicate local and server card with same last 4.
  payments_data().AddCreditCard(CreateLocalCard());
  payments_data().AddServerCreditCard(CreateServerCard());

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_VERIFICATION_CODE}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  // Both FPAN and VCN suggestion should be shown when CVC field is focused.
  ASSERT_EQ(suggestions.size(), 4U);
  EXPECT_THAT(suggestions,
              ContainsCreditCardFooterSuggestions(/*with_gpay_logo=*/true));
}

#if BUILDFLAG(IS_IOS)
TEST_F(AutofillCreditCardSuggestionContentTest,
       GetCreditCardOrCvcFieldSuggestions_LargeKeyboardAccessoryFormat) {
  // Enable formatting for large keyboard accessories.
  autofill_client().set_format_for_large_keyboard_accessory(true);

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
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  // From the credit card number field, the suggestion should show the card type
  // and number and the label should show the expiration date.
  EXPECT_EQ(card_number_field_suggestion.main_text.value, type_and_number);
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{exp_date}}));

  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_NAME_FULL,
      /*virtual_card_option=*/false);

  // From the credit card name field, the suggestion should show the full name
  // and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({name_full}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_EXP_MONTH,
      /*virtual_card_option=*/false);

  // From a credit card expiry field, the suggestion should show the expiration
  // date and the label should show the card type and number.
  EXPECT_EQ(card_number_field_suggestion.main_text.value,
            base::StrCat({exp_date}));
  EXPECT_THAT(card_number_field_suggestion, EqualLabels({{type_and_number}}));

  server_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true);

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
    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NAME_FULL,
                                        /*virtual_card_option=*/true);

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
      CreateCreditCardSuggestionForTest(server_card, autofill_client(),
                                        CREDIT_CARD_NUMBER,
                                        /*virtual_card_option=*/true);

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

class CreditCardSuggestionGeneratorTestForMetadata
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<bool> {
 public:
  CreditCardSuggestionGeneratorTestForMetadata() = default;

  ~CreditCardSuggestionGeneratorTestForMetadata() override = default;

  bool card_has_capital_one_icon() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         CreditCardSuggestionGeneratorTestForMetadata,
                         testing::Bool());

TEST_P(CreditCardSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_ServerCard) {
  // Create a server card.
  CreditCard server_card = CreateServerCard();
  GURL card_art_url = GURL("https://www.example.com/card-art");
  server_card.set_card_art_url(card_art_url);
  gfx::Image fake_image = CustomIconForTest();
  payments_data().CacheImage(card_art_url, fake_image);

  Suggestion virtual_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(virtual_card_suggestion,
                                            card_art_url, fake_image));

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                            fake_image));
}

TEST_P(CreditCardSuggestionGeneratorTestForMetadata,
       CreateCreditCardSuggestion_LocalCard_NoServerDuplicate) {
  // Create a local card.
  CreditCard local_card = CreateLocalCard();

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, GURL(),
                                            gfx::Image()));
}

TEST_P(CreditCardSuggestionGeneratorTestForMetadata,
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
      local_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/true);

  EXPECT_EQ(virtual_card_suggestion.type,
            SuggestionType::kVirtualCreditCardEntry);
  EXPECT_EQ(virtual_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000001"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(virtual_card_suggestion,
                                            card_art_url, fake_image));

  Suggestion real_card_suggestion = CreateCreditCardSuggestionForTest(
      local_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  EXPECT_EQ(real_card_suggestion.type, SuggestionType::kCreditCardEntry);
  EXPECT_EQ(real_card_suggestion.GetPayload<Suggestion::Guid>(),
            Suggestion::Guid("00000000-0000-0000-0000-000000000002"));
  EXPECT_TRUE(VerifyCardArtImageExpectation(real_card_suggestion, card_art_url,
                                            fake_image));
}

// Verifies that the `metadata_logging_context` is correctly set.
TEST_P(CreditCardSuggestionGeneratorTestForMetadata,
       GetCreditCardOrCvcFieldSuggestions_MetadataLoggingContext) {
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});

  {
    // Create one server card with no metadata.
    CreditCard server_card = CreateServerCard();
    server_card.set_issuer_id(kCapitalOneCardIssuerId);
    if (card_has_capital_one_icon()) {
      server_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
    }
    payments_data().AddServerCreditCard(server_card);

    GetSuggestionsForCreditCards(
        form_bundle.form, *form_bundle.form_structure,
        form_bundle.trigger_field, *form_bundle.trigger_autofill_field,
        autofill_client(),
        /*four_digit_combinations_in_dom=*/{},
        /*amount_extraction_manager=*/nullptr,
        /*bnpl_manager=*/nullptr, credit_card_form_event_logger(),
        AutofillMetrics::PaymentsSigninState::kUnknown,
        /*exclude_virtual_cards=*/false);

    const CreditCardSuggestionSummary summary =
        credit_card_form_event_logger()
            .GetCreditCardSuggestionSummaryForTesting();

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

    GetSuggestionsForCreditCards(
        form_bundle.form, *form_bundle.form_structure,
        form_bundle.trigger_field, *form_bundle.trigger_autofill_field,
        autofill_client(),
        /*four_digit_combinations_in_dom=*/{},
        /*amount_extraction_manager=*/nullptr,
        /*bnpl_manager=*/nullptr, credit_card_form_event_logger(),
        AutofillMetrics::PaymentsSigninState::kUnknown,
        /*exclude_virtual_cards=*/false);

    const CreditCardSuggestionSummary summary =
        credit_card_form_event_logger()
            .GetCreditCardSuggestionSummaryForTesting();

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
TEST_P(CreditCardSuggestionGeneratorTestForMetadata,
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

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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

// Tests that CreditCardSuggestionGenerator correctly returns virtual cards with
// usage data and VCN last four for a standalone cvc field.
TEST_F(
    CreditCardSuggestionGeneratorTest,
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

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
            .origin = virtual_card_usage_data.merchant_origin()},
       }});
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          EqualsSuggestion(SuggestionType::kVirtualCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

// Tests that `exclude_virtual_cards` flag correctly blocks the
// VirtualStandaloneCvc flow.
TEST_F(
    CreditCardSuggestionGeneratorTest,
    GetCreditCardOrCvcFieldSuggestions_GetVirtualCreditCardsForStandaloneCvcField_ExcludeVCN) {
  // Set up virtual card usage data and credit cards.
  payments_data().ClearCreditCards();
  CreditCard masked_server_card = test::GetMaskedServerCard();
  masked_server_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);

  masked_server_card.set_guid("1234");
  VirtualCardUsageData virtual_card_usage_data =
      test::GetVirtualCardUsageData1();
  masked_server_card.set_instrument_id(
      *virtual_card_usage_data.instrument_id());

  // Add credit card and usage data to personal data manager.
  payments_data().AddVirtualCardUsageData(virtual_card_usage_data);
  payments_data().AddServerCreditCard(masked_server_card);

  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {
           {.role = FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
            .origin = virtual_card_usage_data.merchant_origin()},
       }});

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  ASSERT_FALSE(suggestions.empty());

  suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/true);

  EXPECT_TRUE(suggestions.empty());
}

// Params of SuggestionIphBubbleTest:
// -- CardInfoRetrievalEnrollmentState card_info_retrieval_enrollment_state:
class SuggestionIphBubbleTest
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<
          CreditCard::CardInfoRetrievalEnrollmentState> {
 public:
  CreditCard::CardInfoRetrievalEnrollmentState
  card_info_retrieval_enrollment_state() {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    CreditCardSuggestionGeneratorTest,
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
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

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
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

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

// Params of DownstreamCardAwarenessIphTest:
// -- `bool` is_downstream_card_awareness_iph_enabled: Indicates whether the
// downstream IPH feature is enabled.
// -- `CreditCard::CardCreationSource` enrollment_source: The source of the
// card's enrollment.
// -- `size_t` use_count: The number of times the card has been used.
class DownstreamCardAwarenessIphTest
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<
          std::tuple<bool, CreditCard::CardCreationSource, size_t>> {
 public:
  DownstreamCardAwarenessIphTest() = default;

  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();
    if (is_downstream_card_awareness_iph_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAutofillEnableDownstreamCardAwarenessIph);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillEnableDownstreamCardAwarenessIph);
    }
  }

  bool is_downstream_card_awareness_iph_enabled() const {
    return std::get<0>(GetParam());
  }
  CreditCard::CardCreationSource enrollment_source() const {
    return std::get<1>(GetParam());
  }
  size_t use_count() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    CreditCardSuggestionGeneratorTest,
    DownstreamCardAwarenessIphTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            CreditCard::CardCreationSource::kCreationSourceUnspecified,
            CreditCard::CardCreationSource::kCreationSourceChromePayments,
            CreditCard::CardCreationSource::kCreationSourceNonChromePayments),
        testing::Values(0, 1, 2)));

// Verify that the downstream card awareness suggestion `feature` is set ONLY
// when the feature flag is enabled, the card enrollment source is
// `kCreationSourceNonChromePayments`, and the card has a `use_count` of 1.
// Since `use_count` is initialized to 1, a value of 1 indicates that the card
// has not yet been used.
TEST_P(DownstreamCardAwarenessIphTest,
       CreateCreditCardSuggestion_DownstreamCardAwarenessIph) {
  CreditCard server_card = CreateServerCard();
  server_card.set_card_creation_source(enrollment_source());
  server_card.usage_history().set_use_count(use_count());

  Suggestion card_number_field_suggestion = CreateCreditCardSuggestionForTest(
      server_card, autofill_client(), CREDIT_CARD_NUMBER,
      /*virtual_card_option=*/false);

  bool should_show_iph =
      is_downstream_card_awareness_iph_enabled() &&
      enrollment_source() ==
          CreditCard::CardCreationSource::kCreationSourceNonChromePayments &&
      use_count() == 1;

  if (should_show_iph) {
    EXPECT_EQ(card_number_field_suggestion.iph_metadata.feature,
              &feature_engagement::kIPHAutofillDownstreamCardAwarenessFeature);
  } else {
    EXPECT_NE(card_number_field_suggestion.iph_metadata.feature,
              &feature_engagement::kIPHAutofillDownstreamCardAwarenessFeature);
  }
}

// Verify we correctly identify suggestions for externally-saved cards.
TEST_P(DownstreamCardAwarenessIphTest, WithExternallySavedCard) {
  CreditCard server_card = CreateServerCard();
  server_card.set_card_creation_source(enrollment_source());
  payments_data().AddServerCreditCard(server_card);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  const CreditCardSuggestionSummary summary =
      credit_card_form_event_logger()
          .GetCreditCardSuggestionSummaryForTesting();

  EXPECT_EQ(
      summary.with_externally_saved_card,
      enrollment_source() ==
          CreditCard::CardCreationSource::kCreationSourceNonChromePayments);
}

// Verify we correctly identify suggestions for never used cards.
TEST_P(DownstreamCardAwarenessIphTest, WithNeverUsedCard) {
  CreditCard server_card = CreateServerCard();
  server_card.usage_history().set_use_count(use_count());
  payments_data().AddServerCreditCard(server_card);

  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = CREDIT_CARD_NUMBER}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  const CreditCardSuggestionSummary summary =
      credit_card_form_event_logger()
          .GetCreditCardSuggestionSummaryForTesting();

  EXPECT_EQ(summary.with_never_used_card, use_count() == 1);
}

// Params of GetFilteredCardsToSuggestTest:
// -- FieldType get_trigger_field_type: Indicates triggered field type.
class GetFilteredCardsToSuggestTest
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<std::tuple<FieldType, bool>> {
 public:
  FieldType get_trigger_field_type() { return std::get<0>(GetParam()); }
  bool IsCvcSavingSupported() { return std::get<1>(GetParam()); }

 private:
  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();
    autofill_client().set_is_cvc_saving_supported(IsCvcSavingSupported());
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
};

INSTANTIATE_TEST_SUITE_P(
    CreditCardSuggestionGeneratorTest,
    GetFilteredCardsToSuggestTest,
    testing::Combine(testing::Values(FieldType::CREDIT_CARD_VERIFICATION_CODE,
                                     FieldType::CREDIT_CARD_NUMBER),
#if BUILDFLAG(IS_IOS)
                     testing::Bool()
#else
                     testing::Values(false)
#endif
                         ));

// Verify that suggestions are filtered based on
// `autofilled_last_four_digits_in_form_for_filtering` when flag is
// on and triggered field type is CVC.
TEST_P(GetFilteredCardsToSuggestTest, GetFilteredCardsToSuggest) {
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = get_trigger_field_type()},
                  {.role = CREDIT_CARD_NUMBER,
                   .value = u"1111",
                   .is_autofilled_according_to_renderer = true}}});

  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  if (get_trigger_field_type() == FieldType::CREDIT_CARD_VERIFICATION_CODE &&
      !IsCvcSavingSupported()) {
    EXPECT_THAT(suggestions, IsEmpty());
  } else if (get_trigger_field_type() ==
             FieldType::CREDIT_CARD_VERIFICATION_CODE) {
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
  FormBundle form_bundle =
      GetFormWithTypes({.fields = {{.role = get_trigger_field_type()}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  if (get_trigger_field_type() == FieldType::CREDIT_CARD_VERIFICATION_CODE &&
      !IsCvcSavingSupported()) {
    EXPECT_THAT(suggestions, IsEmpty());
    return;
  }
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
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = CREDIT_CARD_NUMBER, .value = u"1111"}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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
  FormBundle form_bundle = GetFormWithTypes(
      {.fields = {{.role = get_trigger_field_type()},
                  {.role = CREDIT_CARD_NUMBER,
                   .value = u"9999",
                   .is_autofilled_according_to_renderer = true}}});
  const std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form_bundle.form, *form_bundle.form_structure, form_bundle.trigger_field,
      *form_bundle.trigger_autofill_field, autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  if (get_trigger_field_type() == FieldType::CREDIT_CARD_VERIFICATION_CODE &&
      !IsCvcSavingSupported()) {
    EXPECT_THAT(suggestions, IsEmpty());
  } else if (get_trigger_field_type() ==
             FieldType::CREDIT_CARD_VERIFICATION_CODE) {
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
    : public CreditCardSuggestionGeneratorTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool IsCvcStorageStandaloneFormEnhancementEnabled() {
    return std::get<0>(GetParam());
  }
  bool IsCvcSavingSupported() { return std::get<1>(GetParam()); }

 private:
  void SetUp() override {
    CreditCardSuggestionGeneratorTest::SetUp();
    autofill_client().set_is_cvc_saving_supported(IsCvcSavingSupported());
#if !BUILDFLAG(IS_IOS)
    if (IsCvcStorageStandaloneFormEnhancementEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {features::
               kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {},
          /*disabled_features=*/
          {features::
               kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement});
    }
#endif
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

INSTANTIATE_TEST_SUITE_P(CreditCardSuggestionGeneratorTest,
                         CvcStorageAndFillingStandaloneFormEnhancementTest,
                         testing::Combine(
#if BUILDFLAG(IS_IOS)
                             testing::Values(true),
                             testing::Bool()
#else
                             testing::Bool(),
                             testing::Values(false)
#endif
                                 ));

// Tests that GetCreditCardSuggestions function correctly returns masked server
// card suggestions when no VCN suggestions for a standalone cvc field.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards) {
  FormData form;
  FormFieldData trigger_field;
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1111", "1113"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  if (IsCvcStorageStandaloneFormEnhancementEnabled() &&
      IsCvcSavingSupported()) {
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
  FormData form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NUMBER,
                   .value = u"4111111111111111",
                   .autocomplete_attribute = "cc-number",
                   .is_autofilled_according_to_renderer = true}}});

  FormFieldData trigger_field;
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1113"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
  if (!IsCvcSavingSupported()) {
    EXPECT_THAT(suggestions, IsEmpty());
    return;
  }
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
  FormData form;
  FormFieldData trigger_field;
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_EQ(suggestions.size(), 0U);
}

// Tests that GetCreditCardSuggestions function correctly returns no standalone
// CVC field suggestion when there is last four match in the DOM.
TEST_P(CvcStorageAndFillingStandaloneFormEnhancementTest,
       GetSuggestionsForCreditCards_NoLastFourMatch) {
  FormData form;
  FormFieldData trigger_field;
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{"0000", "9999"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

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
  FormData form;
  FormFieldData trigger_field;
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);
  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);
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

  FormData form;
  FormFieldData trigger_field;
  trigger_field.set_origin(virtual_card_usage_data.merchant_origin());
  AutofillField trigger_autofill_field(trigger_field);
  trigger_autofill_field.SetTypeTo(
      AutofillType(FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE),
      AutofillPredictionSource::kHeuristics);

  std::vector<Suggestion> suggestions = GetSuggestionsForCreditCards(
      form, FormStructure(form), trigger_field, trigger_autofill_field,
      autofill_client(),
      /*four_digit_combinations_in_dom=*/{"1234"},
      /*amount_extraction_manager=*/nullptr, /*bnpl_manager=*/nullptr,
      credit_card_form_event_logger(),
      AutofillMetrics::PaymentsSigninState::kUnknown,
      /*exclude_virtual_cards=*/false);

  EXPECT_THAT(
      suggestions,
      UnorderedElementsAre(
          EqualsSuggestion(SuggestionType::kVirtualCreditCardEntry),
          EqualsSuggestion(SuggestionType::kSeparator),
          EqualsManagePaymentsMethodsSuggestion(/*with_gpay_logo=*/true)));
}

class AutofillCreditCardSuggestionContentForTouchToFillTest
    : public AutofillCreditCardSuggestionContentTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_merchant_opted_out() { return GetParam(); }

 private:
  void SetUp() override {
    AutofillCreditCardSuggestionContentTest::SetUp();
    ON_CALL(*static_cast<MockAutofillOptimizationGuideDecider*>(
                autofill_client().GetAutofillOptimizationGuideDecider()),
            ShouldBlockFormFieldSuggestion)
        .WillByDefault(testing::Return(is_merchant_opted_out()));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillCreditCardSuggestionContentForTouchToFillTest,
                         testing::Bool());

// Verify that the suggestion's `main_text` and `minor_text` are populated
// correctly for both the virtual card and the real card. Furthermore, it
// verifies that if the merchant has opted out of VCN,
// `has_timed_out_for_page_load` is set only for the virtual card, not for the
// real card.
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_MainTextMinorTextMerchantOptOut) {
  CreditCard virtual_card = test::GetVirtualCard();
  CreditCard server_card = CreateServerCard();
  std::vector<CreditCard> cards = {virtual_card, server_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

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
      cards, autofill_manager(), test::MakeFormGlobalId());

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
      cards, autofill_manager(), test::MakeFormGlobalId());

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
      cards, autofill_manager(), test::MakeFormGlobalId());

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
      cards, autofill_manager(), test::MakeFormGlobalId());

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
      cards, autofill_manager(), test::MakeFormGlobalId());

  ASSERT_EQ(suggestions.size(), 1U);
  const Suggestion::CustomIconUrl* custom_icon_url =
      std::get_if<Suggestion::CustomIconUrl>(&suggestions[0].custom_icon);
  EXPECT_EQ(**custom_icon_url, expected_custom_icon_url);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Verify that the suggestion's payment payload includes credit card guid.
TEST_P(AutofillCreditCardSuggestionContentForTouchToFillTest,
       GetCreditCardSuggestionsForTouchToFill_PaymentsPayloadGuid) {
  CreditCard server_card = CreateServerCard();
  server_card.SetNetworkForMaskedCard(kVisaCard);
  CreditCard local_card = test::GetCreditCard();
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  payments_data().AddCreditCard(local_card);
  std::vector<CreditCard> cards = {server_card, local_card};

  std::vector<Suggestion> suggestions = GetCreditCardSuggestionsForTouchToFill(
      cards, autofill_manager(), test::MakeFormGlobalId());

  ASSERT_EQ(suggestions.size(), 2U);
  EXPECT_EQ(
      suggestions[0].GetPayload<Suggestion::PaymentsPayload>().guid.value(),
      server_card.guid());
  EXPECT_EQ(
      suggestions[1].GetPayload<Suggestion::PaymentsPayload>().guid.value(),
      local_card.guid());
}

}  // namespace
}  // namespace autofill
