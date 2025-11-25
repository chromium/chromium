// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/ukm_metrics_test_utils.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_autofill_clock.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/browser/ui/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webdata/common/web_data_results.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#endif

namespace autofill::autofill_metrics {
namespace {

using ::autofill::test::AddFieldPredictionToForm;
using ::autofill::test::CreateTestFormField;
using ::base::ASCIIToUTF16;
using ::base::Bucket;
using ::base::BucketsAre;
using ::base::BucketsInclude;
using ::base::TimeTicks;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::UnorderedPointwise;

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using PaymentsSigninState = AutofillMetrics::PaymentsSigninState;
using AutofillStatus = FormInteractionsUkmLogger::AutofillStatus;

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldValueChangedType = ukm::builders::Autofill_TextFieldDidChange;
using UkmFormEventType = ukm::builders::Autofill_FormEvent;
using UkmFieldInfoType = ukm::builders::Autofill2_FieldInfo;

void CreateSimpleForm(const GURL& origin, FormData& form) {
  form.set_host_frame(test::MakeLocalFrameToken());
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_name(u"TestForm");
  form.set_url(GURL("http://example.com/form.html"));
  form.set_action(GURL("http://example.com/submit.html"));
  form.set_main_frame_origin(url::Origin::Create(origin));
}

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  return base::Base64Encode(unencoded_response_string);
}

class AutofillMetricsTest : public AutofillMetricsBaseTest,
                            public testing::Test {
 public:
  using AutofillMetricsBaseTest::AutofillMetricsBaseTest;
  ~AutofillMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

TEST_F(AutofillMetricsTest, PerfectFilling_Addresses_CreditCards) {
  FormData address_form = test::GetFormData(
      {.fields = {{.role = NAME_FULL,
                   .value = u"Elvis Aaron Presley",
                   .is_autofilled = true},
                  {.role = ADDRESS_HOME_CITY, .value = u"Munich"}}});
  FormData payments_form = test::GetFormData(
      {.fields = {{.role = CREDIT_CARD_NAME_FULL,
                   .value = u"Elvis Aaron Presley",
                   .is_autofilled = true},
                  {.role = CREDIT_CARD_NUMBER, .value = u"01230123012399"}}});
  FormData autocompleted_form =
      test::GetFormData({.fields = {{.role = CREDIT_CARD_NUMBER,
                                     .value = u"01230123012399",
                                     .is_autofilled = true},
                                    {.role = ADDRESS_HOME_CITY,
                                     .value = u"Munich",
                                     .is_autofilled = true}}});
  test_api(payments_form).field(-1).set_is_user_edited(true);
  autofill_manager().AddSeenForm(address_form, {NAME_FULL, ADDRESS_HOME_LINE1});
  autofill_manager().AddSeenForm(payments_form,
                                 {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER});
  autofill_manager()
      .GetAutofillField(address_form.global_id(),
                        address_form.fields().front().global_id())
      ->set_filling_product(FillingProduct::kAddress);
  autofill_manager()
      .GetAutofillField(payments_form.global_id(),
                        payments_form.fields().front().global_id())
      ->set_filling_product(FillingProduct::kCreditCard);

  base::HistogramTester histogram_tester;
  // Upon submitting the address form, we expect logging a perfect address
  // filling.
  SubmitForm(address_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectTotalCount("Autofill.PerfectFilling.CreditCards", 0);
  // Upon submitting the payments form, we expect logging a perfect address
  // filling, since one of the fields was user edited.
  SubmitForm(payments_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.CreditCards", 0,
                                      1);
  // Upon submitting the autocompleted form, we expect not logging anything for
  // both metrics, since the product of filling the form is neither addresses
  // nor credit cards.
  SubmitForm(autocompleted_form);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.Addresses", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.PerfectFilling.CreditCards", 0,
                                      1);
}

// Ensures that metrics that measure timing some important Autofill functions
// actually are recorded and retrieved.
TEST_F(AutofillMetricsTest, TimingMetrics) {
  base::HistogramTester histogram_tester;
  FormData form = CreateForm(
      {CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                           FormControlType::kInputText),
       CreateTestFormField("Autofill Failed", "autofillfailed",
                           "buddy@gmail.com", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "2345678901",
                           FormControlType::kInputTelephone)});
  test_api(form).field(0).set_is_autofilled(true);
  test_api(form).field(1).set_is_autofilled(false);
  test_api(form).field(2).set_is_autofilled(false);

  SeeForm(form);

  // Because these metrics are related to timing, it is not possible to know in
  // advance which bucket the sample will fall into, so we just need to make
  // sure we have valid samples.
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.DetermineHeuristicTypes")
          .empty());
  EXPECT_FALSE(histogram_tester.GetAllSamples("Autofill.Timing.ParseFormsAsync")
                   .empty());
  EXPECT_FALSE(
      histogram_tester
          .GetAllSamples("Autofill.Timing.ParseFormsAsync.RunHeuristics")
          .empty());
  EXPECT_FALSE(histogram_tester
                   .GetAllSamples("Autofill.Timing.ParseFormsAsync.UpdateCache")
                   .empty());
}

// Verify that when submitting an autofillable form, the stored profile metric
// is logged.
TEST_F(AutofillMetricsTest, StoredProfileCountAutofillableFormSubmission) {
  // Three fields is enough to make it an autofillable form.
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "", FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  SubmitForm(form);

  // An autofillable form was submitted, and the number of stored profiles is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 2, 1);
}

// Verify that when submitting a non-autofillable form, the stored profile
// metric is not logged.
TEST_F(AutofillMetricsTest, StoredProfileCountNonAutofillableFormSubmission) {
  // Two non-email fields is not enough to make it an autofillable form.
  FormData form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Last Name", "last-name", "",
                           FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  SubmitForm(form);

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Tests the logging of type-specific field-wise correctness.
TEST_F(AutofillMetricsTest, EditedAutofilledFieldAtSubmission) {
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.role = NAME_FULL,
                  .value = u"Elvis Aaron Presley",
                  .is_autofilled = true},
                 {.role = ADDRESS_HOME_COUNTRY,
                  .value = u"United States",
                  .form_control_type = FormControlType::kSelectOne,
                  .is_autofilled = true},
                 {.role = ADDRESS_HOME_STATE,
                  .value = u"New York",
                  .form_control_type = FormControlType::kSelectOne,
                  .is_autofilled = true},
                 {.role = EMAIL_ADDRESS,
                  .value = u"buddy@gmail.com",
                  .is_autofilled = true}},
      .renderer_id = test::MakeFormRendererId(),
      .main_frame_origin = url::Origin::Create(autofill_driver().url())};

  FormData form = GetAndAddSeenForm(form_description);

  // Simulate user changing values in the first and second fields.
  SimulateUserChangedField(form, form.fields()[0]);
  SimulateUserChangedField(form, form.fields()[1]);

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  // The |NAME_FULL| field was edited (bucket 112).
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.EditedAutofilledFieldAtSubmission2.ByFieldType"),
              BucketsAre(
                  // The |NAME_FULL| field was edited (bucket 112).
                  Bucket(112, 1),
                  // The |EMAIL_ADDRESS| field was not edited (bucket 145).
                  Bucket(145, 1),
                  // The |ADDRESS_HOME_STATE| field was not edited (bucket 545).
                  Bucket(545, 1),
                  // The |ADDRESS_HOME_COUNTRY| field was edited (bucket 576).
                  Bucket(576, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.EditedAutofilledFieldAtSubmission2.Aggregate"),
              BucketsAre(
                  // There should be two counts on edited fields.
                  Bucket(0, 2),
                  // There should be one count on accepted fields.
                  Bucket(1, 2)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.Autocomplete.NotOff."
                  "EditedAutofilledFieldAtSubmission2.Address"),
              BucketsAre(
                  // There should be two counts on edited fields.
                  Bucket(0, 2),
                  // There should be one count on accepted fields.
                  Bucket(1, 2)));

  // The autocomplete!=off histogram should have no count on accepted fields.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.Off.EditedAutofilledFieldAtSubmission2.Address",
      0);
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardMetrics) {
  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded one_month_ago_exploded;
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type: 1 of each for local
  // and 2 of each for masked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
  int num_cards_of_type = 0;
  for (auto record_type : record_types) {
    num_cards_of_type += 1;
    for (int i = 0; i < num_cards_of_type; ++i) {
      // Create a card that's still in active use.
      CreditCard card_in_use = test::GetRandomCreditCard(record_type);
      card_in_use.usage_history().set_use_date(now - base::Days(30));
      card_in_use.usage_history().set_use_count(10);

      // Create a card that's not in active use.
      CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
      card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
      card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
      card_in_disuse.usage_history().set_use_date(now - base::Days(200));
      card_in_disuse.usage_history().set_use_count(10);

      // Add the cards to the personal data manager in the appropriate way.
      auto& repo = (record_type == CreditCard::RecordType::kLocalCard)
                       ? local_cards
                       : server_cards;
      repo.push_back(std::make_unique<CreditCard>(std::move(card_in_use)));
      repo.push_back(std::make_unique<CreditCard>(std::move(card_in_disuse)));
    }
  }

  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/2,
      base::Days(180));

  // Validate the basic count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);

  // Validate the disused count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardDisusedCount", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardDisusedCount", 3,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 2, 1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 6);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 2);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 4);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 30, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 200, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 30, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 200, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 200, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage", 2, 1);
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardWithNicknameMetrics) {
  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(4);

  // Create cards with and without nickname of each record type: 1 of each for
  // local, 2 of each for masked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
  int num_cards_of_type = 0;
  for (auto record_type : record_types) {
    num_cards_of_type += 1;
    for (int i = 0; i < num_cards_of_type; ++i) {
      // Create a card with a nickname.
      CreditCard card_with_nickname = test::GetRandomCreditCard(record_type);
      card_with_nickname.SetNickname(u"Valid nickname");

      // Create a card that doesn't have a nickname.
      CreditCard card_without_nickname = test::GetRandomCreditCard(record_type);
      // Set nickname to empty.
      card_without_nickname.SetNickname(u"");

      // Add the cards to the personal data manager in the appropriate way.
      auto& repo = (record_type == CreditCard::RecordType::kLocalCard)
                       ? local_cards
                       : server_cards;
      repo.push_back(
          std::make_unique<CreditCard>(std::move(card_with_nickname)));
      repo.push_back(
          std::make_unique<CreditCard>(std::move(card_without_nickname)));
    }
  }

  // Log the stored credit card metrics for the cards configured above.
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/0,
      base::Days(180));

  // Validate the count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Local.WithNickname", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.WithNickname", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 6, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Local.WithNickname", 1, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 4,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithNickname", 2, 1);
}

// Tests that local cards with invalid card numbers are correctly logged.
TEST_F(AutofillMetricsTest, LogStoredCreditCardWithInvalidCardNumberMetrics) {
  // Only local cards can have invalid card numbers.
  CreditCard card_with_valid_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_valid_card_number.SetNumber(u"4444333322221111");
  CreditCard card_with_invalid_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_invalid_card_number.SetNumber(u"4444333322221115");
  CreditCard card_with_non_digit_card_number =
      test::GetRandomCreditCard(CreditCard::RecordType::kLocalCard);
  card_with_non_digit_card_number.SetNumber(u"invalid_number");

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_valid_card_number));
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_non_digit_card_number));
  local_cards.push_back(
      std::make_unique<CreditCard>(card_with_invalid_card_number));
  std::vector<std::unique_ptr<CreditCard>> server_cards;

  // Log the stored credit card metrics for the cards configured above.
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(
      local_cards, server_cards, /*server_card_count_with_card_art_image=*/0,
      base::Days(180));

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredCreditCardCount.Local.WithInvalidCardNumber", 2, 1);
}

// Test that the credit card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, CreditCardCheckoutFlowUserActions) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Disable mandatory reauth as it is not part of this test and will
  // interfere with the card retrieval flow.
  paydm().SetPaymentMethodsMandatoryReauthEnabled(false);
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Name on card", "cc-name", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Expiration date", "expdate", "",
                                      FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate showing a credit card suggestion polled from "Name on card" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        /*update_datalist=*/false);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestLocalCardId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field along with a "Clear form" footer suggestion.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

#if !BUILDFLAG(IS_IOS)
  // Simulate selecting an "Undo autofill" suggestion.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        /*update_datalist=*/false);

    external_delegate().DidAcceptSuggestion(
        Suggestion(SuggestionType::kUndoOrClear),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(
        1, user_action_tester.GetActionCount("Autofill_UndoPaymentsAutofill"));
  }
#endif

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field, this time to submit the form.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1,
                               SuggestionType::kCreditCardEntry);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        /*update_datalist=*/false);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestLocalCardId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a credit card suggestion.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields().front().global_id(),
        paydm().GetCreditCardByGUID(kTestLocalCardId),
        AutofillTriggerSource::kPopup);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledCreditCardSuggestion"));
  }

  // Simulate submitting the credit card form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
  }

  // Expect one record for a click on the cardholder name field and one record
  // for each of the 3 clicks on the card number field.
  {
    using Ukm = UkmSuggestionsShownType;
    const std::vector<UkmMetricNameAndValue> name_field_record = {
        {Ukm::kMillisecondsSinceFormParsedName, 0},
        {Ukm::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
        {Ukm::kHtmlFieldTypeName, HtmlFieldType::kUnspecified},
        {Ukm::kServerTypeName, CREDIT_CARD_NAME_FULL},
        {Ukm::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[0])).value()},
        {Ukm::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}};
    const std::vector<UkmMetricNameAndValue> number_field_record = {
        {Ukm::kMillisecondsSinceFormParsedName, 0},
        {Ukm::kHeuristicTypeName, CREDIT_CARD_NUMBER},
        {Ukm::kHtmlFieldTypeName, HtmlFieldType::kUnspecified},
        {Ukm::kServerTypeName, CREDIT_CARD_NUMBER},
        {Ukm::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields()[1])).value()},
        {Ukm::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}};
    EXPECT_THAT(GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
                UkmEventsAre({name_field_record, number_field_record,
                              number_field_record, number_field_record}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }

  // Expect 3 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate().DidAcceptSuggestion|. Second and third, from
  // ExpectedUkmMetrics |autofill_manager().FillOrPreviewForm|.
  {
    using Ukm = UkmSuggestionFilledType;
    const std::vector<UkmMetricNameAndValue> from_did_accept_suggestion = {
        {Ukm::kRecordTypeName, CreditCard::RecordType::kLocalCard},
        {Ukm::kMillisecondsSinceFormParsedName, 0},
        {Ukm::kIsForCreditCardName, true},
        {Ukm::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields().front()))
             .value()},
        {Ukm::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}};
    const std::vector<UkmMetricNameAndValue> from_fill_or_preview_form = {
        {Ukm::kRecordTypeName, CreditCard::RecordType::kLocalCard},
        {Ukm::kMillisecondsSinceFormParsedName, 0},
        {Ukm::kIsForCreditCardName, true},
        {Ukm::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form.fields().front()))
             .value()},
        {Ukm::kFormSignatureName,
         Collapse(CalculateFormSignature(form)).value()}};
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({from_did_accept_suggestion, from_fill_or_preview_form,
                      from_fill_or_preview_form}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
}

// Test that the profile checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, ProfileCheckoutFlowUserActions) {
  RecreateProfile();

  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulate showing a profile suggestion polled from "State" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "City" field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate selecting a profile suggestions.
  {
    base::UserActionTester user_action_tester;
    external_delegate().OnQuery(
        form, form.fields().front(),
        /*caret_bounds=*/gfx::Rect(),
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        /*update_datalist=*/false);

    external_delegate().DidAcceptSuggestion(
        test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                       u"Test",
                                       Suggestion::Guid(kTestProfileId)),
        AutofillSuggestionDelegate::SuggestionMetadata{.row = 0});

    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a profile suggestion.
  {
    base::UserActionTester user_action_tester;
    FillTestProfile(form);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledProfileSuggestion"));
  }

  // Simulate submitting the profile form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
  }

  {
    using Ukm = UkmSuggestionsShownType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kMillisecondsSinceFormParsedName, 0},
              {Ukm::kHeuristicTypeName, ADDRESS_HOME_STATE},
              {Ukm::kHtmlFieldTypeName, HtmlFieldType::kUnspecified},
              {Ukm::kServerTypeName, ADDRESS_HOME_STATE},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields()[0]))
                   .value()},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}},
             {{Ukm::kMillisecondsSinceFormParsedName, 0},
              {Ukm::kHeuristicTypeName, ADDRESS_HOME_CITY},
              {Ukm::kHtmlFieldTypeName, HtmlFieldType::kUnspecified},
              {Ukm::kServerTypeName, ADDRESS_HOME_CITY},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields()[1]))
                   .value()},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
  {
    // Expect 2 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
    // call to |external_delegate().DidAcceptSuggestion|. Second, from call to
    // |autofill_manager().FillOrPreviewForm|.
    using Ukm = UkmSuggestionFilledType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kIsForCreditCardName, false},
              {Ukm::kMillisecondsSinceFormParsedName, 0},
              {Ukm::kFieldSignatureName,
               Collapse(CalculateFieldSignatureForField(form.fields().front()))
                   .value()},
              {Ukm::kFormSignatureName,
               Collapse(CalculateFormSignature(form)).value()}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
}

// Test that the loyalty card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, LoyaltyCardCheckoutFlowUserActions) {
  // Set up test loyalty cards.
  const LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(valuables_data_manager()).SetLoyaltyCards({loyalty_card});

  FormData form =
      CreateForm({CreateTestFormField("Loyalty Program", "loyalty-program", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Loyalty Number", "loyalty-number", "",
                                      FormControlType::kInputText)});
  // Simulate an Autofill query on a profile field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager().AddSeenForm(
        form, {LOYALTY_MEMBERSHIP_PROGRAM, LOYALTY_MEMBERSHIP_ID});
    EXPECT_EQ(
        1, user_action_tester.GetActionCount("Autofill_ParsedLoyaltyCardForm"));
  }

  // Simulate showing a loyalty card suggestion polled from "Loyalty Number"
  // field.
  {
    base::UserActionTester user_action_tester;
    DidShowAutofillSuggestions(form);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedLoyaltyCardSuggestions"));
  }
}

// Tests that the Autofill.QueriedCreditCardFormIsSecure histogram is logged
// properly.
TEST_F(AutofillMetricsTest, QueriedCreditCardFormIsSecure) {
  RecreateCreditCards(/*include_local_credit_card=*/true,
                      /*include_masked_server_credit_card=*/false,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  {
    // Simulate having seen this insecure form on page load.
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("http://example.com/form.html"));
    form.set_main_frame_origin(url::Origin::Create(form.url()));
    form.set_action(GURL("http://example.com/submit.html"));
    autofill_driver().set_url(form.url());
    autofill_client().set_last_committed_primary_main_frame_url(form.url());
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTP, non-secure
    // form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", false, 1);
  }

  {
    ResetAutofillDriver(autofill_driver());
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("https://example.com/form.html"));
    form.set_action(GURL("https://example.com/submit.html"));
    form.set_main_frame_origin(url::Origin::Create(form.url()));
    autofill_driver().set_url(form.url());
    autofill_client().set_last_committed_primary_main_frame_url(form.url());
    autofill_manager().AddSeenForm(form, field_types);

    // Simulate an Autofill query on a credit card field (HTTPS form).
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[1].global_id());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", true, 1);
  }
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_ServerCard) {
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  FormData form =
      CreateForm({CreateTestFormField("Month", "card_month", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Year", "card_year", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("Credit card", "cardnum", "",
                                      FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        paydm().GetCreditCardByGUID(kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kSuccess, "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Success", 1);
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        paydm().GetCreditCardByGUID(kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kPermanentFailure, std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.Failure", 1);
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);
  // Creating masked card
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        paydm().GetCreditCardByGUID(kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPan(PaymentsRpcResult::kClientSideTimeout, std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.ServerCard.ClientSideTimeout",
        1);
  }
}

// Test that a malformed or non-HTTP_OK response doesn't cause problems, per
// crbug/1267105.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration_BadServerResponse) {
  // Creating masked card.
  RecreateCreditCards(/*include_local_credit_card=*/false,
                      /*include_masked_server_credit_card=*/true,
                      /*masked_card_is_enrolled_for_virtual_card=*/false);
  // Set up our form data.
  FormData form = test::CreateTestCreditCardFormData(
      /*is_https=*/true,
      /*use_month_type=*/true,
      /*split_names=*/false);
  std::vector<FieldType> field_types{CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                     CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     CREDIT_CARD_VERIFICATION_CODE};
  ASSERT_EQ(form.fields().size(), field_types.size());

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form, form.fields().back().global_id(),
        paydm().GetCreditCardByGUID(kTestMaskedCardId),
        AutofillTriggerSource::kPopup);
    OnDidGetRealPanWithNonHttpOkResponse();
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.UnknownCard.Failure", 1);
  }
}

TEST_F(AutofillMetricsTest, CreditCardGetRealPanResult_ServerCard) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kPermanentFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_PERMANENT_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_PERMANENT_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kClientSideTimeout,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kSuccess,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kServerCard);

    histogram_tester.ExpectBucketCount("Autofill.UnmaskPrompt.GetRealPanResult",
                                       AutofillMetrics::PAYMENTS_RESULT_SUCCESS,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.ServerCard",
        AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
  }
}

TEST_F(AutofillMetricsTest, CreditCardGetRealPanResult_VirtualCard) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kTryAgainFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_TRY_AGAIN_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kClientSideTimeout,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_CLIENT_SIDE_TIMEOUT, 1);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogRealPanResult(
        PaymentsRpcResult::kSuccess,
        payments::PaymentsAutofillClient::PaymentsRpcCardType::kVirtualCard);

    histogram_tester.ExpectBucketCount("Autofill.UnmaskPrompt.GetRealPanResult",
                                       AutofillMetrics::PAYMENTS_RESULT_SUCCESS,
                                       1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UnmaskPrompt.GetRealPanResult.VirtualCard",
        AutofillMetrics::PAYMENTS_RESULT_SUCCESS, 1);
  }
}

TEST_F(AutofillMetricsTest, ShouldNotLogFormEventNoCardForAddressForm) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  // Simulating submission with no filled data.
  base::HistogramTester histogram_tester;
  DidShowAutofillSuggestions(form);
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  SubmitForm(form);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD, 0);
}

// Test that we log parsed form events for address.
TEST_F(AutofillMetricsTest, AddressParsedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  base::HistogramTester histogram_tester;
  SeeForm(form);
  histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                      FORM_EVENT_DID_PARSE_FORM, 1);

  // Check if FormEvent UKM is logged properly
  using Ukm = UkmFormEventType;
  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
      UkmEventsAre({{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
                     {Ukm::kFormTypesName,
                      AutofillMetrics::FormTypesToBitVector(
                          {FormTypeNameForLogging::kAddressForm,
                           FormTypeNameForLogging::kPostalAddressForm})},
                     {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
  EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
              Each(form.main_frame_origin().GetURL()));
}

// Test that we log interacted form events for address.
TEST_F(AutofillMetricsTest, AddressInteractedFormEvents) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);

    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
                UkmEventsAre(
                    {{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
                      {Ukm::kFormTypesName,
                       AutofillMetrics::FormTypesToBitVector(
                           {FormTypeNameForLogging::kAddressForm,
                            FormTypeNameForLogging::kPostalAddressForm})},
                      {Ukm::kMillisecondsSinceFormParsedName, 0}},
                     {{Ukm::kAutofillFormEventName, FORM_EVENT_INTERACTED_ONCE},
                      {Ukm::kFormTypesName,
                       AutofillMetrics::FormTypesToBitVector(
                           {FormTypeNameForLogging::kAddressForm,
                            FormTypeNameForLogging::kPostalAddressForm})},
                      {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[2].global_id());
    histogram_tester.ExpectUniqueSample("Autofill.FormEvents.Address",
                                        FORM_EVENT_INTERACTED_ONCE, 1);
    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
                UkmEventsAre(
                    {{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
                      {Ukm::kFormTypesName,
                       AutofillMetrics::FormTypesToBitVector(
                           {FormTypeNameForLogging::kAddressForm,
                            FormTypeNameForLogging::kPostalAddressForm})},
                      {Ukm::kMillisecondsSinceFormParsedName, 0}},
                     {{Ukm::kAutofillFormEventName, FORM_EVENT_INTERACTED_ONCE},
                      {Ukm::kFormTypesName,
                       AutofillMetrics::FormTypesToBitVector(
                           {FormTypeNameForLogging::kAddressForm,
                            FormTypeNameForLogging::kPostalAddressForm})},
                      {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
}

// Test that we log suggestion shown form events for address.
TEST_F(AutofillMetricsTest, AddressShownFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_SUGGESTIONS_SHOWN},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    DidShowAutofillSuggestions(form);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_SUGGESTIONS_SHOWN},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_SUGGESTIONS_SHOWN_ONCE},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_SUGGESTIONS_SHOWN},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating same popup being refreshed.
    // Suggestions not related to credit cards/addresses should not affect the
    // histograms.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form, /*field_index=*/0,
                               SuggestionType::kAutocompleteEntry);
    EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
                BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 0),
                               Bucket(FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0)));
    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre({{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
                       {Ukm::kFormTypesName,
                        AutofillMetrics::FormTypesToBitVector(
                            {FormTypeNameForLogging::kAddressForm,
                             FormTypeNameForLogging::kPostalAddressForm})},
                       {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }
}

// Test that we log filled form events for address.
TEST_F(AutofillMetricsTest, AddressFilledFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
    // Check if FormEvent UKM is logged properly
    using Ukm = UkmFormEventType;
    EXPECT_THAT(
        GetUkmEvents(test_ukm_recorder(), Ukm::kEntryName),
        UkmEventsAre(
            {{{Ukm::kAutofillFormEventName, FORM_EVENT_DID_PARSE_FORM},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName, FORM_EVENT_LOCAL_SUGGESTION_FILLED},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}},
             {{Ukm::kAutofillFormEventName,
               FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE},
              {Ukm::kFormTypesName,
               AutofillMetrics::FormTypesToBitVector(
                   {FormTypeNameForLogging::kAddressForm,
                    FormTypeNameForLogging::kPostalAddressForm})},
              {Ukm::kMillisecondsSinceFormParsedName, 0}}}));
    EXPECT_THAT(GetEventUrls(test_ukm_recorder(), Ukm::kEntryName),
                Each(form.main_frame_origin().GetURL()));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating selecting/filling a local profile suggestion more than once.
    base::HistogramTester histogram_tester;
    FillTestProfile(form);
    FillTestProfile(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1)));
  }
}

// Test that we log submitted form events for address.
TEST_F(AutofillMetricsTest, AddressSubmittedFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data. Form is submitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    // Trigger UploadFormDataAsyncCallback.
    ResetAutofillDriver(autofill_driver());
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state and purge UKM logs.
  PurgeUKM();

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion show but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);

    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));

    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
  }
}

// Test that we log "will submit" and "submitted" form events for address.
TEST_F(AutofillMetricsTest, AddressWillSubmitFormEvents) {
  RecreateProfile();
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    FillTestProfile(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1),
                       Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1)));
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager().OnAskForValuesToFillTest(form,
                                                form.fields()[0].global_id());
    SubmitForm(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(4u, entries.size());
  }

  // Reset the autofill manager state.
  ResetAutofillDriver(autofill_driver());
  PurgeUKM();
  autofill_manager().AddSeenForm(form, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    DidShowAutofillSuggestions(form);
    SubmitForm(form);
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
        BucketsInclude(
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
                   0),
            Bucket(FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0),
            Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
                   0)));
    // Check if FormEvent UKM is logged properly
    auto entries =
        test_ukm_recorder().GetEntriesByName(UkmFormEventType::kEntryName);
    EXPECT_EQ(3u, entries.size());
  }
}

// Test that we log the phone field.
TEST_F(AutofillMetricsTest, RecordStandalonePhoneField) {
  FormData form = CreateForm({CreateTestFormField(
      "Phone", "phone", "", FormControlType::kInputTelephone)});

  const std::vector<FieldType> field_types = {PHONE_HOME_NUMBER};
  autofill_manager().AddSeenForm(form, field_types);

  base::HistogramTester histogram_tester;
  autofill_manager().OnAskForValuesToFillTest(form,
                                              form.fields()[0].global_id());
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_INTERACTED_ONCE, 1);
}

// Test that we log the days since last use of a credit card when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_CreditCard) {
  base::HistogramTester histogram_tester;
  CreditCard credit_card;
  credit_card.usage_history().set_use_date(AutofillClock::Now() -
                                           base::Days(21));
  credit_card.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.CreditCard", 21,
                                     1);
}

// Test that we log the days since last use of a profile when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_Profile) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.usage_history().set_use_date(AutofillClock::Now() - base::Days(13));
  profile.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.Profile", 13,
                                     1);
}

// Test that we log the verification status of name tokens.
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfNameTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"First Last",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"First",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Last",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Last",
                                           VerificationStatus::kParsed);

  AutofillMetrics::LogVerificationStatusOfNameTokensOnProfileUsage(profile);

  std::string base_histogram =
      "Autofill.NameTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histogram + "Full",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "First",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "Last",
                                      VerificationStatus::kParsed, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "SecondLast",
                                      VerificationStatus::kParsed, 1);

  histogram_tester.ExpectTotalCount(base_histogram + "Middle", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "FirstLast", 0);

  histogram_tester.ExpectTotalCount(base_histogram + "Any", 4);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kObserved, 1);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kParsed, 3);
}

// Test that we log the verification status of address tokens..
TEST_F(AutofillMetricsTest, LogVerificationStatusesOfAddressTokens) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"123 StreetName",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"123",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"StreetName", VerificationStatus::kObserved);

  AutofillMetrics::LogVerificationStatusOfAddressTokensOnProfileUsage(profile);

  std::string base_histogram =
      "Autofill.AddressTokenVerificationStatusAtProfileUsage.";

  histogram_tester.ExpectUniqueSample(base_histogram + "StreetAddress",
                                      VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "StreetName",
                                      VerificationStatus::kObserved, 1);
  histogram_tester.ExpectUniqueSample(base_histogram + "HouseNumber",
                                      VerificationStatus::kObserved, 1);

  histogram_tester.ExpectTotalCount(base_histogram + "FloorNumber", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "ApartmentNumber", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "Premise", 0);
  histogram_tester.ExpectTotalCount(base_histogram + "SubPremise", 0);

  histogram_tester.ExpectTotalCount(base_histogram + "Any", 3);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kFormatted, 1);
  histogram_tester.ExpectBucketCount(base_histogram + "Any",
                                     VerificationStatus::kObserved, 2);
}

// Verify that we correctly log metrics tracking the duration of form fill.
// TODO(crbug.com/442816527): Reenable test on ios.
#if BUILDFLAG(IS_IOS)
#define MAYBE_FormFillDuration DISABLED_FormFillDuration
#else
#define MAYBE_FormFillDuration FormFillDuration
#endif
TEST_F(AutofillMetricsTest, MAYBE_FormFillDuration) {
  base::TimeTicks beginning = base::TimeTicks::Now();
  FormData empty_form = CreateForm(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Email", "email", "", FormControlType::kInputText),
       CreateTestFormField("Phone", "phone", "", FormControlType::kInputText)});

  FormData filled_form = empty_form;
  test_api(filled_form).field(0).set_value(u"Elvis Aaron Presley");
  test_api(filled_form).field(1).set_value(u"theking@gmail.com");
  test_api(filled_form).field(2).set_value(u"12345678901");

  // Fill additional form.
  FormData second_form = empty_form;
  second_form.set_host_frame(test::MakeLocalFrameToken());
  second_form.set_renderer_id(test::MakeFormRendererId());
  test_api(second_form)
      .Append(CreateTestFormField("Second Phone", "second_phone", "",
                                  FormControlType::kInputText));

  // Fill the field values for form submission.
  test_api(second_form).field(0).set_value(u"Elvis Aaron Presley");
  test_api(second_form).field(1).set_value(u"theking@gmail.com");
  test_api(second_form).field(2).set_value(u"12345678901");
  test_api(second_form).field(3).set_value(u"51512345678");

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    SCOPED_TRACE("Test 1 - no interaction, fields are prefilled");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);
    ASSERT_EQ(base::TimeTicks::Now(), beginning);
    task_environment_.FastForwardBy(base::Microseconds(17));
    ASSERT_EQ(base::TimeTicks::Now(), beginning + base::Microseconds(17));
    SubmitForm(filled_form);
    ASSERT_EQ(base::TimeTicks::Now(), beginning + base::Microseconds(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    ResetAutofillDriver(autofill_driver());
  }

  // Expect metric to be logged if the user manually edited a form field.
  {
    SCOPED_TRACE("Test 2 - all fields are filled by the user");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);

    FormData user_filled_form = filled_form;
    task_environment_.FastForwardBy(base::Microseconds(3));
    SimulateUserChangedField(user_filled_form,
                             user_filled_form.fields().front(),
                             base::TimeTicks::Now());
    task_environment_.FastForwardBy(base::Microseconds(14));
    SubmitForm(filled_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 14, 1);

    // We expected an upload to be triggered when the manager is reset.
    ResetAutofillDriver(autofill_driver());
  }

  // Expect metric to be logged if the user autofilled the form.
  {
    SCOPED_TRACE("Test 3 - all fields are autofilled");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);

    FormData autofilled_form = test::AsAutofilled(filled_form);
    task_environment_.FastForwardBy(base::Microseconds(5));
    AutofillForm(autofilled_form);
    task_environment_.FastForwardBy(base::Microseconds(12));
    SubmitForm(autofilled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    ResetAutofillDriver(autofill_driver());
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    SCOPED_TRACE(
        "Test 4 - mixed case: some fields are autofilled, some fields are "
        "edited.");
    base::HistogramTester histogram_tester;

    SeeForm(empty_form);

    FormData mixed_filled_form = test::AsAutofilled(filled_form);
    task_environment_.FastForwardBy(base::Microseconds(5));
    AutofillForm(mixed_filled_form);
    task_environment_.FastForwardBy(base::Microseconds(3));
    SimulateUserChangedField(mixed_filled_form,
                             mixed_filled_form.fields().front(),
                             base::TimeTicks::Now());

    task_environment_.FastForwardBy(base::Microseconds(9));
    SubmitForm(mixed_filled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    ResetAutofillDriver(autofill_driver());
  }

  // Make sure that loading another form doesn't affect metrics from the first
  // form.
  {
    SCOPED_TRACE("Test 5 - load a second form before submitting the first");
    base::HistogramTester histogram_tester;
    SeeForm(empty_form);

    SeeForm(test::WithoutValues(second_form));

    FormData mixed_filled_form = test::AsAutofilled(filled_form);
    task_environment_.FastForwardBy(base::Microseconds(5));
    AutofillForm(mixed_filled_form);
    task_environment_.FastForwardBy(base::Microseconds(3));
    SimulateUserChangedField(mixed_filled_form,
                             mixed_filled_form.fields().front(),
                             base::TimeTicks::Now());

    task_environment_.FastForwardBy(base::Microseconds(9));
    SubmitForm(mixed_filled_form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    ResetAutofillDriver(autofill_driver());
  }

  // Make sure that submitting a form that was loaded later will report the
  // later loading time.
  {
    SCOPED_TRACE("Test 6 - submit the second seen form first");
    base::HistogramTester histogram_tester;
    SeeForm(test::WithoutValues(empty_form));
    SeeForm(test::WithoutValues(second_form));

    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(second_form);

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    ResetAutofillDriver(autofill_driver());
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_CreditCardForm) {
  // Should log time duration with autofill for credit card form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard", 0);
  }

  // Should log time duration without autofill for credit card form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard", 0);
  }

  // Should not log time duration for credit card form if credit card form is
  // not detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_AddressForm) {
  // Should log time duration with autofill for address form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address", 0);
  }

  // Should log time duration without autofill for address form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address", 0);
  }

  // Should not log time duration for address form if address form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_PasswordForm) {
  // Should log time duration with autofill for password form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kPasswordForm}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password", 0);
  }

  // Should log time duration without autofill for password form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kPasswordForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password", 0);
  }

  // Should not log time duration for password form if password form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_UnknownForm) {
  // Should log time duration with autofill for unknown form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/true,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown", 0);
  }

  // Should log time duration without autofill for unknown form.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kUnknownFormType}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown", 0);
  }

  // Should not log time duration for unknown form if unknown form is not
  // detected.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kAddressForm}, /*used_autofill=*/false,
        base::Milliseconds(2000));
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown", 0);
  }
}

TEST_F(AutofillMetricsTest, FormFillDurationFromInteraction_MultipleForms) {
  // Should log time duration with autofill for all forms.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm, FormType::kAddressForm,
         FormType::kPasswordForm, FormType::kUnknownFormType},
        /*used_autofill=*/true, base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill.Unknown",
        base::Milliseconds(2000), 1);
  }

  // Should log time duration without autofill for all forms.
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogFormFillDurationFromInteraction(
        {FormType::kCreditCardForm, FormType::kAddressForm,
         FormType::kPasswordForm, FormType::kUnknownFormType},
        /*used_autofill=*/false, base::Milliseconds(2000));
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.CreditCard",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Address",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Password",
        base::Milliseconds(2000), 1);
    histogram_tester.ExpectTimeBucketCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill.Unknown",
        base::Milliseconds(2000), 1);
  }
}

// Test class that shares setup code for testing ParseQueryResponse.
class AutofillMetricsParseQueryResponseTest : public testing::Test {
 public:
  void SetUp() override {
    FormData form;
    form.set_host_frame(test::MakeLocalFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_url(GURL("http://foo.com"));
    form.set_main_frame_origin(
        url::Origin::Create(GURL("http://foo_root.com")));
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);

    field.set_label(u"fullname");
    field.set_name(u"fullname");
    test_api(form).Append(field);

    field.set_label(u"address");
    field.set_name(u"address");
    test_api(form).Append(field);

    // Checkable fields should be ignored in parsing.
    FormFieldData checkable_field;
    checkable_field.set_label(u"radio_button");
    checkable_field.set_form_control_type(FormControlType::kInputRadio);
    checkable_field.set_check_status(
        FormFieldData::CheckStatus::kCheckableButUnchecked);
    test_api(form).Append(checkable_field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());

    field.set_label(u"email");
    field.set_name(u"email");
    test_api(form).Append(field);

    field.set_label(u"password");
    field.set_name(u"password");
    field.set_form_control_type(FormControlType::kInputPassword);
    test_api(form).Append(field);

    owned_forms_.push_back(std::make_unique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::vector<std::unique_ptr<FormStructure>> owned_forms_;
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms_;
};

TEST_F(AutofillMetricsParseQueryResponseTest, ServerHasData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NAME_FULL, form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), ADDRESS_HOME_LINE1,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), EMAIL_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), NO_SERVER_DATA,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  ParseServerPredictionsQueryResponse(
      response_string, forms_, test::GetEncodedSignatures(forms_), nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// If the server returns NO_SERVER_DATA for one of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, OneFormNoServerData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), NO_SERVER_DATA,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), EMAIL_ADDRESS,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), NO_SERVER_DATA,
                           form_suggestion);
  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  ParseServerPredictionsQueryResponse(
      response_string, forms_, test::GetEncodedSignatures(forms_), nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 1), Bucket(true, 1)));
}

// If the server returns NO_SERVER_DATA for both of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, AllFormsNoServerData) {
  AutofillQueryResponse response;
  for (int form_idx = 0; form_idx < 2; ++form_idx) {
    auto* form_suggestion = response.add_form_suggestions();
    for (int field_idx = 0; field_idx < 2; ++field_idx) {
      AddFieldPredictionToForm(*forms_[form_idx]->field(field_idx),
                               NO_SERVER_DATA, form_suggestion);
    }
  }

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  ParseServerPredictionsQueryResponse(
      response_string, forms_, test::GetEncodedSignatures(forms_), nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 2)));
}

// If the server returns NO_SERVER_DATA for only some of the fields, expect the
// UMA metric to say there is data.
TEST_F(AutofillMetricsParseQueryResponseTest, PartialNoServerData) {
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[0]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[0]->field(1), PHONE_HOME_NUMBER,
                           form_suggestion);
  form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(*forms_[1]->field(0), NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(*forms_[1]->field(1), PHONE_HOME_CITY_CODE,
                           form_suggestion);

  std::string response_string = SerializeAndEncode(response);
  base::HistogramTester histogram_tester;
  ParseServerPredictionsQueryResponse(
      response_string, forms_, test::GetEncodedSignatures(forms_), nullptr);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// Tests that logging CardUploadDecision UKM works as expected.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric) {
  int upload_decision = 1;
  autofill_driver().set_url(GURL("https://www.google.com"));

  LogCardUploadDecisionsUkm(&test_ukm_recorder(),
                            autofill_driver().GetPageUkmSourceId(),
                            autofill_driver().url(), upload_decision);
  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), UkmCardUploadDecisionType::kEntryName),
      UkmEventsAre({{{UkmCardUploadDecisionType::kUploadDecisionName,
                      upload_decision}}}));
  EXPECT_THAT(
      GetEventUrls(test_ukm_recorder(), UkmCardUploadDecisionType::kEntryName),
      Each(autofill_driver().url()));
}

// Tests that no UKM is logged when the URL is not valid.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_InvalidUrl) {
  GURL url("");
  test_ukm_recorder().Purge();
  LogCardUploadDecisionsUkm(&test_ukm_recorder(), -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
}

// Tests that no UKM is logged when the ukm service is null.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_NoUkmService) {
  GURL url("https://www.google.com");
  test_ukm_recorder().Purge();
  LogCardUploadDecisionsUkm(nullptr, -1, url, 1);
  EXPECT_EQ(0ul, test_ukm_recorder().sources_count());
  EXPECT_EQ(0ul, test_ukm_recorder().entries_count());
}

TEST_F(AutofillMetricsTest, DynamicFormMetrics) {
  FormData form = CreateForm(
      {CreateTestFormField("State", "state", "", FormControlType::kInputText),
       CreateTestFormField("City", "city", "", FormControlType::kInputText),
       CreateTestFormField("Street", "street", "",
                           FormControlType::kInputText)});

  const std::vector<FieldType> field_types = {
      ADDRESS_HOME_STATE, ADDRESS_HOME_CITY, ADDRESS_HOME_STREET_ADDRESS};

  // Simulate seeing.
  base::HistogramTester histogram_tester;
  autofill_manager().AddSeenForm(form, field_types);

  // Simulate filling the form.
  FillTestProfile(form);

  // Dynamically change the form.
  test_api(form).Remove(-1);

  // Trigger a refill, the refill metric should be updated.
  autofill_manager().OnFormsSeen({form}, /*removed_forms=*/{});
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.Address"),
              BucketsInclude(Bucket(FORM_EVENT_DID_DYNAMIC_REFILL, 1)));
}

// Verify that we don't log Autofill.WebOTP.OneTimeCode.FromAutocomplete if the
// frame has no form.
TEST_F(AutofillMetricsTest, FrameHasNoForm) {
  base::HistogramTester histogram_tester;
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 0);
}

// Verify that we correctly log metrics if a frame has
// autocomplete="one-time-code".
TEST_F(AutofillMetricsTest, FrameHasAutocompleteOneTimeCode) {
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword,
                           "one-time-code"),
       CreateTestFormField("", "", "", FormControlType::kInputPassword)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  // Verifies that autocomplete="one-time-code" in a form is correctly recorded.
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete",
      /* has_one_time_code */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 1);
}

// Verify that we correctly log metrics if a frame does not have
// autocomplete="one-time-code".
TEST_F(AutofillMetricsTest, FrameDoesNotHaveAutocompleteOneTimeCode) {
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete",
      /* has_one_time_code */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.OneTimeCode.FromAutocomplete", 1);
}

// Verify that we correctly log metrics when a phone number field does not have
// autocomplete attribute but there are at least 3 fields in the form.
TEST_F(AutofillMetricsTest, FrameHasPhoneNumberFieldWithoutAutocomplete) {
  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  FormData form =
      CreateForm({CreateTestFormField("Phone", "phone", "",
                                      FormControlType::kInputTelephone),
                  CreateTestFormField("Last Name", "lastname", "",
                                      FormControlType::kInputText),
                  CreateTestFormField("First Name", "firstname", "",
                                      FormControlType::kInputText)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a phone number field does not have
// autocomplete attribute and there are less than 3 fields in the form.
TEST_F(AutofillMetricsTest, FrameHasSinglePhoneNumberFieldWithoutAutocomplete) {
  // At least 3 fields are necessary for FormStructure to compute proper field
  // types if autocomplete attribute value is not available.
  FormData form = CreateForm({CreateTestFormField(
      "Phone", "phone", "", FormControlType::kInputTelephone)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a phone number field has
// autocomplete attribute.
TEST_F(AutofillMetricsTest, FrameHasPhoneNumberFieldWithAutocomplete) {
  FormData form;  // Form with phone number.
  CreateSimpleForm(autofill_driver().url(), form);
  form.set_fields(
      {CreateTestFormField("", "", "", FormControlType::kInputText, "phone")});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 1,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// Verify that we correctly log metrics when a form does not have phone number
// field.
TEST_F(AutofillMetricsTest, FrameDoesNotHavePhoneNumberField) {
  FormData form = CreateForm(
      {CreateTestFormField("", "", "", FormControlType::kInputPassword)});

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_client().GetAutofillDriverFactory().Delete(autofill_driver());
  histogram_tester.ExpectBucketCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult",
      /* has_phone_number_field */ 0,
      /* sample count */ 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.WebOTP.PhoneNumberCollection.ParseResult", 1);
}

// ContentAutofillDriver is not visible to TestAutofillDriver on iOS.
// In addition, WebOTP will not ship on iOS.
#if !BUILDFLAG(IS_IOS)

// Use <Phone><WebOTP><OTC> as the bit pattern to identify the metrics state.
enum class PhoneCollectionMetricState {
  kNone = 0,    // Site did not collect phone, not use OTC, not use WebOTP
  kOTC = 1,     // Site used OTC only
  kWebOTP = 2,  // Site used WebOTP only
  kWebOTPPlusOTC = 3,  // Site used WebOTP and OTC
  kPhone = 4,          // Site collected phone, not used neither WebOTP nor OTC
  kPhonePlusOTC = 5,   // Site collected phone number and used OTC
  kPhonePlusWebOTP = 6,         // Site collected phone number and used WebOTP
  kPhonePlusWebOTPPlusOTC = 7,  // Site collected phone number and used both
  kMaxValue = kPhonePlusWebOTPPlusOTC,
};

struct WebOTPPhoneCollectionMetricsTestCase {
  std::vector<const char*> autocomplete_field;
  PhoneCollectionMetricState phone_collection_metric_state;
  bool report_autofill_web_otp_metrics = false;
};

class WebOTPPhoneCollectionMetricsTest
    : public AutofillMetricsTest,
      public ::testing::WithParamInterface<
          WebOTPPhoneCollectionMetricsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    WebOTPPhoneCollectionMetricsTest,
    WebOTPPhoneCollectionMetricsTest,
    testing::Values(
        // Verify that we correctly log PhoneCollectionMetricState::kNone.
        WebOTPPhoneCollectionMetricsTestCase{{"password"},
                                             PhoneCollectionMetricState::kNone},
        // Verify that we correctly log PhoneCollectionMetricState::kOTC.
        WebOTPPhoneCollectionMetricsTestCase{{"one-time-code"},
                                             PhoneCollectionMetricState::kOTC},
        // Verify that we correctly log PhoneCollectionMetricState::kWebOTP.
        WebOTPPhoneCollectionMetricsTestCase{
            {},
            PhoneCollectionMetricState::kWebOTP,
            true},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kWebOTPPlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"one-time-code"},
            PhoneCollectionMetricState::kWebOTPPlusOTC,
            true},
        // Verify that we correctly log PhoneCollectionMetricState::kPhone.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel"},
            PhoneCollectionMetricState::kPhone},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel", "one-time-code"},
            PhoneCollectionMetricState::kPhonePlusOTC},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusWebOTP.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel"},
            PhoneCollectionMetricState::kPhonePlusWebOTP,
            true},
        // Verify that we correctly log
        // PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC.
        WebOTPPhoneCollectionMetricsTestCase{
            {"tel", "one-time-code"},
            PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC,
            true}));

TEST_P(WebOTPPhoneCollectionMetricsTest,
       TestWebOTPPhoneCollectionMetricsState) {
  auto test_case = GetParam();

  if (!test_case.autocomplete_field.empty()) {
    FormData form;
    CreateSimpleForm(autofill_driver().url(), form);
    for (const char* autocomplete : test_case.autocomplete_field) {
      test_api(form).Append(CreateTestFormField(
          "", "", "", FormControlType::kInputText, autocomplete));
    }

    SeeForm(form);
  }

  base::HistogramTester histogram_tester;
  autofill_manager().ReportAutofillWebOTPMetrics(
      test_case.report_autofill_web_otp_metrics);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.WebOTP.PhonePlusWebOTPPlusOTC"),
      BucketsAre(Bucket(test_case.phone_collection_metric_state, 1)));
}

// Verify that proper PhoneCollectionMetricsState is logged to UKM.
TEST_F(AutofillMetricsTest, WebOTPPhoneCollectionMetricsStateLoggedToUKM) {
  EXPECT_THAT(GetUkmEvents(test_ukm_recorder(),
                           ukm::builders::WebOTPImpact::kEntryName),
              UkmEventsAre({}));

  FormData form;
  CreateSimpleForm(autofill_driver().url(), form);
  // Document collects phone number
  test_api(form).Append(
      CreateTestFormField("", "", "", FormControlType::kInputTelephone, "tel"));
  // Document uses OntTimeCode
  test_api(form).Append(CreateTestFormField(
      "", "", "", FormControlType::kInputText, "one-time-code"));

  base::HistogramTester histogram_tester;
  SeeForm(form);
  autofill_manager().ReportAutofillWebOTPMetrics(true);

  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(),
                   ukm::builders::WebOTPImpact::kEntryName),
      UkmEventsAre({{{"PhoneCollection",
                      PhoneCollectionMetricState::kPhonePlusWebOTPPlusOTC}}}));
}

TEST_F(AutofillMetricsTest, AutocompleteOneTimeCodeFormFilledDuration) {
  FormData form = CreateForm({CreateTestFormField(
      "", "", "", FormControlType::kInputPassword, "one-time-code")});
  test_api(form).field(0).set_value(u"123456");

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    task_environment_.FastForwardBy(base::Microseconds(17));
    SubmitForm(form);

    histogram_tester.ExpectTotalCount(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromLoad", 16, 1);
    ResetAutofillDriver(autofill_driver());
  }

  {
    base::HistogramTester histogram_tester;
    SeeForm(form);
    task_environment_.FastForwardBy(base::Microseconds(5));
    AutofillForm(form);
    task_environment_.FastForwardBy(base::Microseconds(3));
    SimulateUserChangedField(form, form.fields().front(),
                             base::TimeTicks::Now());
    task_environment_.FastForwardBy(base::Microseconds(9));
    SubmitForm(form);

    histogram_tester.ExpectUniqueSample(
        "Autofill.WebOTP.OneTimeCode.FillDuration.FromInteraction", 14, 1);
    ResetAutofillDriver(autofill_driver());
  }
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(AutofillMetricsTest, OnAutocompleteSuggestionsShown) {
  base::HistogramTester histogram_tester;
  AutofillMetrics::OnAutocompleteSuggestionsShown();
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events3", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      /*expected_count=*/1);
}

TEST_F(AutofillMetricsTest, LogServerCardLinkClicked) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState::kSignedIn);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       PaymentsSigninState::kSignedIn, 1);
  }
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogServerCardLinkClicked(PaymentsSigninState::kSignedOut);
    histogram_tester.ExpectTotalCount("Autofill.ServerCardLinkClicked", 1);
    histogram_tester.ExpectBucketCount("Autofill.ServerCardLinkClicked",
                                       PaymentsSigninState::kSignedOut, 1);
  }
}

TEST_F(AutofillMetricsTest, GetFieldTypeUserEditStatusMetric) {
  // The id of ADDRESS_HOME_COUNTRY is 36 = 0b10'0100.
  FieldType server_type = ADDRESS_HOME_COUNTRY;
  // The id of AUTOFILL_FIELD_WAS_NOT_EDITED is 1.
  AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric =
      AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
          AUTOFILLED_FIELD_WAS_NOT_EDITED;

  int expected_result = 0b10'0100'0001;
  int actual_result = GetFieldTypeUserEditStatusMetric(server_type, metric);
  EXPECT_EQ(expected_result, actual_result);
}

// Base class for cross-frame filling metrics, in particular for
// Autofill.CreditCard.SeamlessFills.*.
class AutofillMetricsCrossFrameFormTest : public AutofillMetricsTest {
 public:
  struct CreditCardAndCvc {
    CreditCard credit_card;
    std::u16string cvc;
  };

  AutofillMetricsCrossFrameFormTest() = default;
  ~AutofillMetricsCrossFrameFormTest() override = default;

  void SetUp() override {
    AutofillMetricsTest::SetUp();

    RecreateCreditCards(/*include_local_credit_card=*/true,
                        /*include_masked_server_credit_card=*/false,
                        /*masked_card_is_enrolled_for_virtual_card=*/false);

    credit_card_ =
        *GetCreditCardsToSuggest(
             autofill_client().GetPersonalDataManager().payments_data_manager())
             .front();
    credit_card_.set_cvc(u"123");

    url::Origin main_origin =
        url::Origin::Create(GURL("https://example.test/"));
    url::Origin other_origin = url::Origin::Create(GURL("https://other.test/"));
    form_ = test::GetFormData(
        {.description_for_logging = "CrossFrameFillingMetrics",
         .fields =
             {
                 {.label = u"Cardholder name",
                  .name = u"card_name",
                  .is_autofilled = false},
                 {.label = u"CCNumber",
                  .name = u"ccnumber",
                  .is_autofilled = false,
                  .origin = other_origin},
                 {.label = u"ExpDate",
                  .name = u"expdate",
                  .is_autofilled = false},
                 {.is_visible = false,
                  .label = u"CVC",
                  .name = u"cvc",
                  .is_autofilled = false,
                  .origin = other_origin},
             },
         .renderer_id = test::MakeFormRendererId(),
         .main_frame_origin = main_origin});

    ASSERT_EQ(form_.main_frame_origin(), form_.fields()[0].origin());
    ASSERT_EQ(form_.main_frame_origin(), form_.fields()[2].origin());
    ASSERT_NE(form_.main_frame_origin(), form_.fields()[1].origin());
    ASSERT_NE(form_.main_frame_origin(), form_.fields()[3].origin());
    ASSERT_EQ(form_.fields()[1].origin(), form_.fields()[3].origin());

    // Mock a simplified security model which allows to filter (only) fields
    // from the same origin.
    autofill_driver().SetFieldTypeMapFilter(base::BindRepeating(
        [](AutofillMetricsCrossFrameFormTest* self,
           const url::Origin& triggered_origin, FieldGlobalId field,
           FieldType) {
          return triggered_origin == self->GetFieldById(field).origin();
        },
        this));
  }

  CreditCard& credit_card() { return credit_card_; }

  // Any call to FillForm() should be followed by a SetFormValues() call to
  // mimic its effect on |form_|.
  void FillForm(const FormFieldData& triggering_field) {
    EXPECT_CALL(credit_card_access_manager(), FetchCreditCard)
        .WillOnce(base::test::RunOnceCallback<1>(credit_card()));
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form_, triggering_field.global_id(),
        &credit_card_, AutofillTriggerSource::kPopup);
  }

  // Sets the field values of |form_| according to the parameters.
  //
  // Since this test suite doesn't use mocks, we can't intercept the autofilled
  // form. Therefore, after each manual fill or autofill, we shall call
  // SetFormValues()
  void SetFormValues(const FieldTypeSet& fill_field_types,
                     bool is_autofilled,
                     bool is_user_typed) {
    auto type_to_index = base::MakeFixedFlatMap<FieldType, size_t>(
        {{CREDIT_CARD_NAME_FULL, 0},
         {CREDIT_CARD_NUMBER, 1},
         {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, 2},
         {CREDIT_CARD_VERIFICATION_CODE, 3}});

    for (FieldType fill_type : fill_field_types) {
      auto index_it = type_to_index.find(fill_type);
      ASSERT_NE(index_it, type_to_index.end());
      FormFieldData& field = test_api(form_).field(index_it->second);
      field.set_value(credit_card().GetRawInfo(fill_type));
      field.set_is_autofilled(is_autofilled);
      field.set_properties_mask((field.properties_mask() & ~kUserTyped) |
                                (is_user_typed ? kUserTyped : 0));
    }
  }

  FormFieldData& GetFieldById(FieldGlobalId field) {
    auto it = std::ranges::find(test_api(form_).fields(), field,
                                &FormFieldData::global_id);
    CHECK(it != form_.fields().end());
    return *it;
  }

  FormData form_;
  CreditCard credit_card_;
};

// This fixture adds utilities for the seamlessness metric names.
//
// These metric names get very long, and with >16 variants the tests become
// unreadable otherwise.
class AutofillMetricsSeamlessnessTest
    : public AutofillMetricsCrossFrameFormTest {
 public:
  struct MetricName {
    enum class Fill { kFills, kFillable };
    enum class Time { kBefore, kAfter, kSubmission };
    enum class Visibility { kAll, kVisible };
    enum class Variant { kQualitative, kBitmask };

    Fill fill;
    Time time;
    Visibility visibility;
    Variant variant;

    std::string str() const {
      return base::StringPrintf(
          "Autofill.CreditCard.Seamless%s.%s%s%s",
          fill == Fill::kFills ? "Fills" : "Fillable",
          time == Time::kSubmission ? "AtSubmissionTime"
          : time == Time::kBefore   ? "AtFillTimeBeforeSecurityPolicy"
                                    : "AtFillTimeAfterSecurityPolicy",
          visibility == Visibility::kAll ? "" : ".Visible",
          variant == Variant::kQualitative ? "" : ".Bitmask");
    }
  };

  static constexpr auto kFills = MetricName::Fill::kFills;
  static constexpr auto kFillable = MetricName::Fill::kFillable;
  static constexpr auto kBefore = MetricName::Time::kBefore;
  static constexpr auto kAfter = MetricName::Time::kAfter;
  static constexpr auto kSubmission = MetricName::Time::kSubmission;
  static constexpr auto kAll = MetricName::Visibility::kAll;
  static constexpr auto kVisible = MetricName::Visibility::kVisible;
  static constexpr auto kQualitative = MetricName::Variant::kQualitative;
  static constexpr auto kBitmask = MetricName::Variant::kBitmask;

 protected:
  AutofillMetricsSeamlessnessTest() {
    scoped_features_.InitAndEnableFeatureWithParameters(
        features::kAutofillLogUKMEventsWithSamplingOnSession,
        {{features::kAutofillLogUKMEventsWithSamplingOnSessionRate.name,
          "100"}});
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Tests that Autofill.CreditCard.SeamlessFills.* is not emitted for manual
// fills.
TEST_F(AutofillMetricsSeamlessnessTest,
       DoNotLogCreditCardSeamlessFillsMetricIfNotAutofilled) {
  using UkmBuilder = ukm::builders::Autofill_CreditCardFill;
  base::HistogramTester histogram_tester;
  SeeForm(form_);

  // Fake manual fill.
  SetFormValues(
      {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE},
      /*is_autofilled=*/false, /*is_user_typed=*/true);

  SubmitForm(form_);
  DeleteDriverToCommitMetrics();

  for (auto fill : {kFills, kFillable}) {
    for (auto time : {kBefore, kAfter, kSubmission}) {
      for (auto visibility : {kAll, kVisible}) {
        for (auto variant : {kQualitative, kBitmask}) {
          histogram_tester.ExpectTotalCount(
              MetricName{fill, time, visibility, variant}.str(), 0);
        }
      }
    }
  }

  EXPECT_THAT(GetUkmEvents(test_ukm_recorder(), UkmBuilder::kEntryName),
              UkmEventsAre({}));
}

// Tests that Autofill.CreditCard.SeamlessFills.* are emitted.
TEST_F(AutofillMetricsSeamlessnessTest,
       LogCreditCardSeamlessFillsMetricIfAutofilledWithoutCvc) {
  using Metric = AutofillMetrics::CreditCardSeamlessness::Metric;
  using UkmBuilder = ukm::builders::Autofill_CreditCardFill;

  // `Metric` as raw integer for UKM.
  constexpr auto kFullFill = static_cast<uint64_t>(Metric::kFullFill);
  constexpr auto kOptionalCvcMissing =
      static_cast<uint64_t>(Metric::kOptionalCvcMissing);
  constexpr auto kPartialFill = static_cast<uint64_t>(Metric::kPartialFill);
  // Bits of the bitmask.
  constexpr uint8_t kName = true << 3;
  constexpr uint8_t kNumber = true << 2;
  constexpr uint8_t kExp = true << 1;
  constexpr uint8_t kCvc = true << 0;
  // The metric for the policy-controlled feature "autofill".
  enum SharedAutofillMetric : uint64_t {
    kSharedAutofillIsIrrelevant = 0,
    kSharedAutofillWouldHelp = 1,
    kSharedAutofillDidHelp = 2,
  };

  base::HistogramTester histogram_tester;
  auto SamplesOf = [&histogram_tester](MetricName metric) {
    return histogram_tester.GetAllSamples(metric.str());
  };

  SeeForm(form_);

  credit_card().clear_cvc();

  // Fakes an Autofill with the following behavior:
  // - before security and assuming a complete profile: kFullFill;
  // - before security and without a CVC:               kOptionalCvcMissing;
  // - after security  and assuming a complete profile: kPartialFill;
  // - after security  and without a CVC:               kPartialFill;
  // because due to the security policy, only NAME and EXP_DATE are filled.
  // The CVC field is invisible.
  FillForm(form_.fields()[0]);
  SetFormValues({CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Fakes an Autofill with the following behavior:
  // - before security and assuming a complete profile: kFullFill;
  // - before security and without a CVC:               kPartialFill;
  // - after security  and assuming a complete profile: kPartialFill;
  // - after security  and without a CVC:               kPartialFill;
  // because the due to the security policy, only NUMBER and CVC could be
  // filled.
  // The CVC field is invisible.
  FillForm(form_.fields()[1]);
  SetFormValues({CREDIT_CARD_NUMBER},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  SubmitForm(form_);
  DeleteDriverToCommitMetrics();

  // Bitmask metrics.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp | kCvc, 2)));
  EXPECT_THAT(
      SamplesOf({kFills, kBefore, kAll, kBitmask}),
      BucketsAre(Bucket(kName | kNumber | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kSubmission, kAll, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp, 1)));
  // Bitmask metrics restricted to visible fields.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kVisible, kBitmask}),
              BucketsAre(Bucket(kName | kNumber | kExp, 2)));
  EXPECT_THAT(
      SamplesOf({kFills, kBefore, kVisible, kBitmask}),
      BucketsAre(Bucket(kName | kNumber | kExp, 1), Bucket(kNumber, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kVisible, kBitmask}),
              BucketsAre(Bucket(kName | kExp, 1), Bucket(kNumber, 1)));

  // Qualitative metrics.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kFullFill, 2)));
  EXPECT_THAT(SamplesOf({kFills, kBefore, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1),
                         Bucket(Metric::kPartialFill, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));
  EXPECT_THAT(SamplesOf({kFills, kSubmission, kAll, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1)));
  // Qualitative metrics restricted to visible fields.
  EXPECT_THAT(SamplesOf({kFillable, kBefore, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 2)));
  EXPECT_THAT(SamplesOf({kFills, kBefore, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kOptionalCvcMissing, 1),
                         Bucket(Metric::kPartialFill, 1)));
  EXPECT_THAT(SamplesOf({kFills, kAfter, kVisible, kQualitative}),
              BucketsAre(Bucket(Metric::kPartialFill, 2)));

  EXPECT_THAT(
      GetUkmEvents(test_ukm_recorder(), UkmBuilder::kEntryName),
      UkmEventsAre(
          {{
               // First event.
               {UkmBuilder::kFillable_BeforeSecurity_QualitativeName,
                kFullFill},
               {UkmBuilder::kFilled_BeforeSecurity_QualitativeName,
                kOptionalCvcMissing},
               {UkmBuilder::kFilled_AfterSecurity_QualitativeName,
                kPartialFill},

               {UkmBuilder::kFillable_BeforeSecurity_BitmaskName,
                kName | kNumber | kExp | kCvc},
               {UkmBuilder::kFilled_BeforeSecurity_BitmaskName,
                kName | kNumber | kExp},
               {UkmBuilder::kFilled_AfterSecurity_BitmaskName, kName | kExp},

               {UkmBuilder::kFillable_BeforeSecurity_Visible_QualitativeName,
                kOptionalCvcMissing},
               {UkmBuilder::kFilled_BeforeSecurity_Visible_QualitativeName,
                kOptionalCvcMissing},
               {UkmBuilder::kFilled_AfterSecurity_Visible_QualitativeName,
                kPartialFill},

               {UkmBuilder::kFillable_BeforeSecurity_Visible_BitmaskName,
                kName | kNumber | kExp},
               {UkmBuilder::kFilled_BeforeSecurity_Visible_BitmaskName,
                kName | kNumber | kExp},
               {UkmBuilder::kFilled_AfterSecurity_Visible_BitmaskName,
                kName | kExp},

               {UkmBuilder::kSharedAutofillName, kSharedAutofillWouldHelp},

               {UkmBuilder::kFormSignatureName,
                *Collapse(CalculateFormSignature(form_))},
           },
           {
               // Second event.
               {UkmBuilder::kFillable_BeforeSecurity_QualitativeName,
                kFullFill},
               {UkmBuilder::kFilled_BeforeSecurity_QualitativeName,
                kPartialFill},
               {UkmBuilder::kFilled_AfterSecurity_QualitativeName,
                kPartialFill},

               {UkmBuilder::kFillable_BeforeSecurity_BitmaskName,
                kName | kNumber | kExp | kCvc},
               {UkmBuilder::kFilled_BeforeSecurity_BitmaskName, kNumber},
               {UkmBuilder::kFilled_AfterSecurity_BitmaskName, kNumber},

               {UkmBuilder::kFillable_BeforeSecurity_Visible_QualitativeName,
                kOptionalCvcMissing},
               {UkmBuilder::kFilled_BeforeSecurity_Visible_QualitativeName,
                kPartialFill},
               {UkmBuilder::kFilled_AfterSecurity_Visible_QualitativeName,
                kPartialFill},

               {UkmBuilder::kFillable_BeforeSecurity_Visible_BitmaskName,
                kName | kNumber | kExp},
               {UkmBuilder::kFilled_BeforeSecurity_Visible_BitmaskName,
                kNumber},
               {UkmBuilder::kFilled_AfterSecurity_Visible_BitmaskName, kNumber},

               {UkmBuilder::kSharedAutofillName, kSharedAutofillIsIrrelevant},

               {UkmBuilder::kFormSignatureName,
                *Collapse(CalculateFormSignature(form_))},
           }}));
  EXPECT_THAT(GetEventUrls(test_ukm_recorder(), UkmBuilder::kEntryName),
              Each(form_.main_frame_origin().GetURL()));
}

// Test if we have correctly recorded the filling status of fields in an unsafe
// iframe.
TEST_F(AutofillMetricsSeamlessnessTest, CreditCardFormRecordOnIFrames) {
  // Create a form with the credit card number and CVC code fields in an
  // iframe with a different origin.
  SeeForm(form_);

  // Triggering autofill from the credit card name field cannot fill the credit
  // card number and CVC code fields, which are in an unsafe iframe.
  FillForm(form_.fields()[0]);
  SetFormValues({CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Triggering autofill from the credit card number field can fill all the
  // credit card fields with values.
  FillForm(form_.fields()[1]);
  SetFormValues({CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                 CREDIT_CARD_VERIFICATION_CODE},
                /*is_autofilled=*/true, /*is_user_typed=*/false);

  // Record Autofill2.FieldInfo UKM event at autofill manager reset.
  SubmitForm(form_);
  DeleteDriverToCommitMetrics();

  static constexpr auto field_types = std::to_array<FieldType>(
      {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
       CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_VERIFICATION_CODE});

  // Verify FieldInfo UKM event for every field.
  using UFIT = UkmFieldInfoType;
  std::vector<std::vector<UkmMetricNameAndValue>> expected_events;
  for (size_t i = 0; i < 4; ++i) {
    DenseSet<FieldFillingSkipReason> skipped_status_vector;
    if (i == 0 || i == 2) {
      skipped_status_vector = {FieldFillingSkipReason::kNotSkipped,
                               FieldFillingSkipReason::kAlreadyAutofilled};
    } else {
      skipped_status_vector = {FieldFillingSkipReason::kNotSkipped};
    }
    DenseSet<AutofillStatus> autofill_status_vector;
    int field_log_events_count = 0;
    if (i == 0 || i == 2) {
      autofill_status_vector = {
          AutofillStatus::kIsFocusable,
          AutofillStatus::kWasAutofillTriggeredAnywhereOnForm,
          AutofillStatus::kShouldBeAutofilledBeforeSecurityPolicy,
          AutofillStatus::kHadValueBeforeFilling,
          AutofillStatus::kHadTypedOrFilledValueAtSubmission,
          AutofillStatus::kWasAutofilledAfterSecurityPolicy};
      field_log_events_count = i == 0 ? 3 : 2;
    } else {
      autofill_status_vector = {
          AutofillStatus::kIsFocusable,
          AutofillStatus::kWasAutofillTriggeredAnywhereOnForm,
          AutofillStatus::kShouldBeAutofilledBeforeSecurityPolicy,
          AutofillStatus::kHadTypedOrFilledValueAtSubmission,
          AutofillStatus::kFillingPreventedByIframeSecurityPolicy,
          AutofillStatus::kWasAutofilledAfterSecurityPolicy};
      field_log_events_count = i == 1 ? 3 : 2;
    }
    expected_events.push_back({
        {UFIT::kFormSessionIdentifierName,
         FormGlobalIdToHash64Bit(form_.global_id())},
        {UFIT::kFieldSessionIdentifierName,
         FieldGlobalIdToHash64Bit(form_.fields()[i].global_id())},
        {UFIT::kFieldSignatureName,
         Collapse(CalculateFieldSignatureForField(form_.fields()[i])).value()},
        {UFIT::kAutofillSkippedStatusName, skipped_status_vector.data()[0]},
        {UFIT::kFormControlType2Name,
         base::to_underlying(FormControlType::kInputText)},
        {UFIT::kAutocompleteStateName,
         base::to_underlying(AutofillMetrics::AutocompleteState::kNone)},
        {UFIT::kAutofillStatusVectorName, autofill_status_vector.data()[0]},
        {UFIT::kOverallTypeName, field_types[i]},
        {UFIT::kSectionIdName, 1},
        {UFIT::kTypeChangedByRationalizationName, false},
        {UFIT::kRankInFieldSignatureGroupName, 1},
        {UFIT::kHeuristicTypeName, field_types[i]},
        {UFIT::kFieldLogEventCountName, field_log_events_count + 2},
    });
  }
  EXPECT_THAT(GetUkmEvents(test_ukm_recorder(), UFIT::kEntryName),
              UkmEventsAre(expected_events));
}

}  // namespace
}  // namespace autofill::autofill_metrics
