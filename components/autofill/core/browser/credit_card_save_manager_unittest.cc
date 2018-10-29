// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager.h"
#include "components/autofill/core/browser/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_strike_database.h"
#include "components/autofill/core/browser/test_sync_service.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

const char kEloCardNumber[] = "5067111111111112";
const char kJcbCardNumber[] = "3528111111111110";

std::string NextYear() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return std::to_string(now.year + 1);
}

std::string NextMonth() {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  return std::to_string(now.month % 12 + 1);
}

}  // anonymous namespace

class CreditCardSaveManagerTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    personal_data_.Init(/*profile_database=*/autofill_client_.GetDatabase(),
                        /*account_database=*/nullptr,
                        /*pref_service=*/autofill_client_.GetPrefs(),
                        /*identity_manager=*/nullptr,
                        /*client_profile_validator=*/nullptr,
                        /*history_service=*/nullptr,
                        /*is_off_the_record=*/false);
    personal_data_.SetSyncServiceForTest(&sync_service_);
    autofill_driver_.reset(new TestAutofillDriver());
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    autofill_driver_->SetURLRequestContext(request_context_.get());
    payments_client_ = new payments::TestPaymentsClient(
        autofill_driver_->GetURLLoaderFactory(), autofill_client_.GetPrefs(),
        autofill_client_.GetIdentityManager(), &personal_data_);
    credit_card_save_manager_ =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data_);
    credit_card_save_manager_->SetCreditCardUploadEnabled(true);
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(), &autofill_client_, &personal_data_,
        std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager_),
        payments_client_));
    autofill_manager_->SetExpectedObservedSubmission(true);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    personal_data_.SetPrefService(nullptr);
    personal_data_.ClearCreditCards();

    request_context_ = nullptr;
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms, base::TimeTicks());
  }

  void FormSubmitted(const FormData& form) {
    autofill_manager_->OnFormSubmitted(
        form, false, SubmissionSource::FORM_SUBMISSION, base::TimeTicks::Now());
  }

  void UserHasAcceptedUpload(const base::string16& cardholder_name) {
    credit_card_save_manager_->OnUserDidAcceptUpload(cardholder_name);
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type,
                                    bool split_names = false) {
    form->name = ASCIIToUTF16("MyForm");
    if (is_https) {
      form->origin = GURL("https://myform.com/form.html");
      form->action = GURL("https://myform.com/submit.html");
      form->main_frame_origin =
          url::Origin::Create(GURL("https://myform_root.com/form.html"));
    } else {
      form->origin = GURL("http://myform.com/form.html");
      form->action = GURL("http://myform.com/submit.html");
      form->main_frame_origin =
          url::Origin::Create(GURL("http://myform_root.com/form.html"));
    }

    FormFieldData field;
    if (split_names) {
      test::CreateTestFormField("First Name on Card", "firstnameoncard", "",
                                "text", &field);
      field.autocomplete_attribute = "cc-given-name";
      form->fields.push_back(field);
      test::CreateTestFormField("Last Name on Card", "lastnameoncard", "",
                                "text", &field);
      field.autocomplete_attribute = "cc-family-name";
      form->fields.push_back(field);
      field.autocomplete_attribute = "";
    } else {
      test::CreateTestFormField("Name on Card", "nameoncard", "", "text",
                                &field);
      form->fields.push_back(field);
    }
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form->fields.push_back(field);
    if (use_month_type) {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                                &field);
      form->fields.push_back(field);
    } else {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                                &field);
      form->fields.push_back(field);
      test::CreateTestFormField("", "ccyear", "", "text", &field);
      form->fields.push_back(field);
    }
    test::CreateTestFormField("CVC", "cvc", "", "text", &field);
    form->fields.push_back(field);
  }

  // Fills the fields in |form| with test data.
  void ManuallyFillAddressForm(const char* first_name,
                               const char* last_name,
                               const char* zip_code,
                               const char* country,
                               FormData* form) {
    for (FormFieldData& field : form->fields) {
      if (base::EqualsASCII(field.name, "firstname"))
        field.value = ASCIIToUTF16(first_name);
      else if (base::EqualsASCII(field.name, "lastname"))
        field.value = ASCIIToUTF16(last_name);
      else if (base::EqualsASCII(field.name, "addr1"))
        field.value = ASCIIToUTF16("123 Maple");
      else if (base::EqualsASCII(field.name, "city"))
        field.value = ASCIIToUTF16("Dallas");
      else if (base::EqualsASCII(field.name, "state"))
        field.value = ASCIIToUTF16("Texas");
      else if (base::EqualsASCII(field.name, "zipcode"))
        field.value = ASCIIToUTF16(zip_code);
      else if (base::EqualsASCII(field.name, "country"))
        field.value = ASCIIToUTF16(country);
    }
  }

  // Tests if credit card data gets saved.
  void TestSaveCreditCards(bool is_https) {
    // Set up our form data.
    FormData form;
    CreateTestCreditCardFormData(&form, is_https, false);
    std::vector<FormData> forms(1, form);
    FormsSeen(forms);

    // Edit the data, and submit.
    form.fields[1].value = ASCIIToUTF16("4111111111111111");
    form.fields[2].value = ASCIIToUTF16(NextMonth());
    form.fields[3].value = ASCIIToUTF16(NextYear());
    FormSubmitted(form);
    EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  }

  void ExpectUniqueFillableFormParsedUkm() {
    auto entries = test_ukm_recorder_.GetEntriesByName(
        UkmDeveloperEngagementType::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder_.ExpectEntryMetric(
          entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
          1 << AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS);
    }
  }

  void ExpectUniqueCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::CardUploadDecisionMetric metric) {
    histogram_tester.ExpectUniqueSample("Autofill.CardUploadDecisionMetric",
                                        ToHistogramSample(metric), 1);
  }

  void ExpectCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::CardUploadDecisionMetric metric) {
    histogram_tester.ExpectBucketCount("Autofill.CardUploadDecisionMetric",
                                       ToHistogramSample(metric), 1);
  }

  void ExpectNoCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::CardUploadDecisionMetric metric) {
    histogram_tester.ExpectBucketCount("Autofill.CardUploadDecisionMetric",
                                       ToHistogramSample(metric), 0);
  }

  void ExpectCardUploadDecisionUkm(int expected_metric_value) {
    ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
                 UkmCardUploadDecisionType::kEntryName, expected_metric_value,
                 1 /* expected_num_matching_entries */);
  }

  void ExpectFillableFormParsedUkm(int num_fillable_forms_parsed) {
    ExpectMetric(UkmDeveloperEngagementType::kDeveloperEngagementName,
                 UkmDeveloperEngagementType::kEntryName,
                 1 << AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS,
                 num_fillable_forms_parsed);
  }

  void ExpectMetric(const char* metric_name,
                    const char* entry_name,
                    int expected_metric_value,
                    size_t expected_num_matching_entries) {
    auto entries = test_ukm_recorder_.GetEntriesByName(entry_name);
    EXPECT_EQ(expected_num_matching_entries, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder_.ExpectEntryMetric(entry, metric_name,
                                           expected_metric_value);
    }
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestPersonalDataManager personal_data_;
  TestSyncService sync_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Ends up getting owned (and destroyed) by TestFormDataImporter:
  TestCreditCardSaveManager* credit_card_save_manager_;
  // Ends up getting owned (and destroyed) by TestAutofillManager:
  payments::TestPaymentsClient* payments_client_;
  // Ends up getting owned (and destroyed) by TestAutofillClient:
  TestStrikeDatabase* strike_database_;

 private:
  int ToHistogramSample(AutofillMetrics::CardUploadDecisionMetric metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }
};

// Tests that credit card data are saved for forms on https
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTPS \
  DISABLED_ImportFormDataCreditCardHTTPS
#else
#define MAYBE_ImportFormDataCreditCardHTTPS ImportFormDataCreditCardHTTPS
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_ImportFormDataCreditCardHTTPS) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestSaveCreditCards(true);
}

// Tests that credit card data are saved for forms on http
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTP DISABLED_ImportFormDataCreditCardHTTP
#else
#define MAYBE_ImportFormDataCreditCardHTTP ImportFormDataCreditCardHTTP
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_ImportFormDataCreditCardHTTP) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestSaveCreditCards(false);
}

// Tests that credit card data are saved when autocomplete=off for CC field.
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_CreditCardSavedWhenAutocompleteOff \
  DISABLED_CreditCardSavedWhenAutocompleteOff
#else
#define MAYBE_CreditCardSavedWhenAutocompleteOff \
  CreditCardSavedWhenAutocompleteOff
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_CreditCardSavedWhenAutocompleteOff) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);

  // Set "autocomplete=off" for cardnumber field.
  form.fields[1].should_autocomplete = false;

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  form.fields[1].value = ASCIIToUTF16("4111111111111111");
  form.fields[2].value = ASCIIToUTF16(NextMonth());
  form.fields[3].value = ASCIIToUTF16(NextYear());
  FormSubmitted(form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

// Tests that credit card data are not saved when CC number does not pass the
// Luhn test.
TEST_F(CreditCardSaveManagerTest, InvalidCreditCardNumberIsNotSaved) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  std::string card("4408041234567890");
  ASSERT_FALSE(autofill::IsValidCreditCardNumber(ASCIIToUTF16(card)));
  form.fields[1].value = ASCIIToUTF16(card);
  form.fields[2].value = ASCIIToUTF16(NextMonth());
  form.fields[3].value = ASCIIToUTF16(NextYear());
  FormSubmitted(form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

TEST_F(CreditCardSaveManagerTest, CreditCardDisabledDoesNotSave) {
  autofill_manager_->SetCreditCardEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The credit card should neither be saved locally or uploaded.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_FullAddresses) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSendOnlyCountryInGetUploadDetails);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->active_experiments_in_request().empty());

  // Verify that one profile was saved, and it was included in the upload
  // details request to payments.
  EXPECT_EQ(1U, personal_data_.GetProfiles().size());
  EXPECT_THAT(
      payments_client_->addresses_in_upload_details(),
      testing::UnorderedElementsAreArray({*personal_data_.GetProfiles()[0]}));

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", true, 1);
  // Verify that UMA for "DaysSincePreviousUse" was not logged because we
  // modified the profile.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile", 0);

  // Simulate that the user has accepted the upload from the prompt.
  UserHasAcceptedUpload(/*cardholder_name=*/base::ASCIIToUTF16(""));
  // We should find that full addresses are included in the UploadCard request.
  EXPECT_THAT(
      payments_client_->addresses_in_upload_card(),
      testing::UnorderedElementsAreArray({*personal_data_.GetProfiles()[0]}));
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_OnlyCountryInAddresses) {
  // When this feature is enabled, the addresses being sent in the upload
  // details request will only contain the country.
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSendOnlyCountryInGetUploadDetails);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->active_experiments_in_request().empty());

  // Verify that even though the full address profile was saved, only the
  // country was included in the upload details request to payments.
  EXPECT_EQ(1U, personal_data_.GetProfiles().size());
  AutofillProfile only_country;
  only_country.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  EXPECT_EQ(1U, payments_client_->addresses_in_upload_details().size());
  // AutofillProfile::Compare will ignore the difference in guid between our
  // actual profile being sent and the expected one constructed here.
  EXPECT_EQ(0, payments_client_->addresses_in_upload_details()[0].Compare(
                   only_country));

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", true, 1);
  // Verify that UMA for "DaysSincePreviousUse" was not logged because we
  // modified the profile.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile", 0);

  // Simulate that the user has accepted the upload from the prompt.
  UserHasAcceptedUpload(/*cardholder_name=*/base::ASCIIToUTF16(""));
  // We should find that full addresses are included in the UploadCard request,
  // even though only countries were included in GetUploadDetails.
  EXPECT_THAT(
      payments_client_->addresses_in_upload_card(),
      testing::UnorderedElementsAreArray({*personal_data_.GetProfiles()[0]}));
}

// Tests metrics for SaveCardWithFirstAndLastNameComplete for local cards.
TEST_F(CreditCardSaveManagerTest, LocalCreditCard_FirstAndLastName) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, /*is_https=*/true,
                               /*use_month_type=*/false, /*split_names=*/true);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo");
  credit_card_form.fields[1].value = ASCIIToUTF16("Master");
  credit_card_form.fields[2].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[3].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the histogram entry for SaveCardWithFirstAndLastNameOffered.
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameComplete.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Server", 0);
}

// Tests metrics for SaveCardWithFirstAndLastNameComplete for local cards.
TEST_F(CreditCardSaveManagerTest, LocalCreditCard_LastAndFirstName) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                            &field);
  field.autocomplete_attribute = "cc-family-name";
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("First Name on Card", "firstnameoncard", "", "text",
                            &field);
  field.autocomplete_attribute = "cc-given-name";
  credit_card_form.fields.push_back(field);
  field.autocomplete_attribute = "";
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  credit_card_form.fields.push_back(field);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("Flo");
  credit_card_form.fields[2].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[3].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the histogram entry for SaveCardWithFirstAndLastNameOffered.
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameComplete.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Server", 0);
}

// Tests that a credit card inferred from a form with a credit card first and
// last name can be uploaded.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_FirstAndLastName) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, /*is_https=*/true,
                               /*use_month_type=*/false, /*split_names=*/true);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo");
  credit_card_form.fields[1].value = ASCIIToUTF16("Master");
  credit_card_form.fields[2].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[3].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->active_experiments_in_request().empty());

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());
  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", true, 1);
  // Verify that UMA for "DaysSincePreviousUse" was not logged because we
  // modified the profile.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Local", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameComplete.Server", 1);
}

// Tests that a credit card inferred from a form with a credit card first and
// last name can be uploaded when the last name comes before first name on the
// form.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_LastAndFirstName) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                            &field);
  field.autocomplete_attribute = "cc-family-name";
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("First Name on Card", "firstnameoncard", "", "text",
                            &field);
  field.autocomplete_attribute = "cc-given-name";
  credit_card_form.fields.push_back(field);
  field.autocomplete_attribute = "";
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  credit_card_form.fields.push_back(field);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("Flo");
  credit_card_form.fields[2].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[3].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->active_experiments_in_request().empty());

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());
  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", true, 1);
  // Verify that UMA for "DaysSincePreviousUse" was not logged because we
  // modified the profile.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Local", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameComplete.Server", 1);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCardAndSaveCopy) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillNoLocalSaveOnUploadSuccess);

  const char* const server_id = "InstrumentData:1234";
  payments_client_->SetServerIdForCardUpload(server_id);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  FormSubmitted(credit_card_form);

  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(personal_data_.GetLocalCreditCards().empty());
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // See |OfferStoreUnmaskedCards|
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());
#else
  ASSERT_EQ(1U, personal_data_.GetCreditCards().size());
  const CreditCard* const saved_card = personal_data_.GetCreditCards()[0];
  EXPECT_EQ(CreditCard::OK, saved_card->GetServerStatus());
  EXPECT_EQ(base::ASCIIToUTF16("1111"), saved_card->LastFourDigits());
  EXPECT_EQ(kVisaCard, saved_card->network());
  EXPECT_EQ(std::stoi(NextMonth()), saved_card->expiration_month());
  EXPECT_EQ(std::stoi(NextYear()), saved_card->expiration_year());
  EXPECT_EQ(server_id, saved_card->server_id());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, saved_card->record_type());
  EXPECT_EQ(base::ASCIIToUTF16(card_number), saved_card->number());
#endif
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_DisableLocalSave) {
  personal_data_.ClearCreditCards();
  personal_data_.ClearProfiles();

  credit_card_save_manager_->SetCreditCardUploadEnabled(true);
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillNoLocalSaveOnUploadSuccess);

  const char* const server_id = "InstrumentData:1234";
  payments_client_->SetServerIdForCardUpload(server_id);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Don't keep a copy of the card on this device.
  EXPECT_TRUE(personal_data_.GetCreditCards().empty());
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_FeatureNotEnabled) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcUnavailable) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::CVC_VALUE_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcInvalidLength) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("1234");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the invalid CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::INVALID_CVC_VALUE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::INVALID_CVC_VALUE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MultipleCvcFields) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("http://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC (hidden)", "cvc1", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc2", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // A CVC value appeared in one of the two CVC fields, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoCvcFieldOnForm) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.  Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("http://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_InvalidCvcInNonCvcField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("http://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Random Field", "random", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter an invalid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("1234");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the invalid CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInNonCvcField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("http://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Random Field", "random", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter a valid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInAddressField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");
  credit_card_form.main_frame_origin =
      url::Origin::Create(GURL("http://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter a valid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Bob Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoRecentlyUsedProfile) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create, fill and submit an address form in order to establish a profile.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set the current time to another value.
  test_clock.SetNow(kMuchLaterTime);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", false, 1);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CvcUnavailableAndNoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC, name, and address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED | AutofillMetrics::CVC_VALUE_NOT_FOUND |
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoNameAvailable) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoNameAvailableAndNoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing names/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE |
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesConflict) {
  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1, "1");
  test::CreateTestAddressFormData(&address_form2, "2");

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  ManuallyFillAddressForm("Flo", "Master", "77401-8294", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "77401-1234", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(3 /* num_fillable_forms_parsed */);

  // Edit the data and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the conflicting zip codes.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ZipCodesDoNotDiscardWhitespace) {
  // Create two separate profiles with different zip codes. Must directly add
  // instead of submitting a form, because they're deduped on form submit.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000001");
  profile1.SetInfo(NAME_FULL, ASCIIToUTF16("Flo Master"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("H3B2Y5"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"), "en-US");
  personal_data_.AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000002");
  profile2.SetInfo(NAME_FULL, ASCIIToUTF16("Flo Master"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("h3b 2y5"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"), "en-US");
  personal_data_.AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the conflicting zip codes.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesHavePrefixMatch) {
  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "77401-8294", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // One zip is a prefix of the other, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoZipCodeAvailable) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // Autofill's validation requirements for Venezuala ("VE", see
  // src/components/autofill/core/browser/country_data.cc) do not require zip
  // codes. We use Venezuala here because to use the US (or one of many other
  // countries which autofill requires a zip code for) would result in no
  // address being imported at all, and then we never reach the check for
  // missing zip code in the upload code.
  ManuallyFillAddressForm("Flo", "Master", "" /* zip_code */, "Venezuela",
                          &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing zip code.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED |
                              AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasMiddleInitial) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can be different case.
  ManuallyFillAddressForm("flo", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // And they can have a middle initial even if the other names don't.
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but use the name with a middle initial *and* period, and
  // submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo W. Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoMiddleInitialInCCForm) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can have different variations of middle initials.
  ManuallyFillAddressForm("flo w.", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but do not use middle initial.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CCFormHasCardholderMiddleName) {
  // Create, fill and submit address form without middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by adding a middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Quincy Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasAddressMiddleName) {
  // Create, fill and submit address form with middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John Quincy", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by removing middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NamesCanMismatch) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Master", "Blaster", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Bob Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_OFFERED |
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_IgnoreOldProfiles) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // Advance the current time. Since |address_form1| will not be a recently
  // used address profile, we will not include it in the candidate profiles.
  test_clock.SetNow(kMuchLaterTime);

  ManuallyFillAddressForm("Master", "Blaster", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Master Blaster");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Name matches recently used profile, should offer upload.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

// Requesting cardholder name currently not available on Android.
#if !defined(OS_ANDROID)
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_RequestCardholderNameIfNameMissingAndNoPaymentsCustomer) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry and DetectedValue for "Cardholder
  // name explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_RequestCardholderNameIfNameConflictingAndNoPaymentsCustomer) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but include a conflicting name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Jane Doe");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry and DetectedValue for "Cardholder
  // name explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameExistsAndNoPaymentsCustomer) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Because everything went smoothly, verify that there was no histogram entry
  // or DetectedValue for "Cardholder name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameMissingAndPaymentsCustomer) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set the billing_customer_number Priority Preference to designate existence
  // of a Payments account.
  autofill_client_.GetPrefs()->SetDouble(prefs::kAutofillBillingCustomerNumber,
                                         12345);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that there was no histogram entry or DetectedValue for "Cardholder
  // name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameConflictingAndPaymentsCustomer) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Set the billing_customer_number Priority Preference to designate existence
  // of a Payments account.
  autofill_client_.GetPrefs()->SetDouble(prefs::kAutofillBillingCustomerNumber,
                                         12345);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but include a conflicting name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Jane Doe");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that there was no histogram entry or DetectedValue for "Cardholder
  // name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameMissingAndNoPaymentsCustomerExpOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that there was no histogram entry or DetectedValue for "Cardholder
  // name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameConflictingAndNoPaymentsCustomerExpOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but include a conflicting name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Jane Doe");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that there was no histogram entry or DetectedValue for "Cardholder
  // name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

// This test ensures |should_request_name_from_user_| is reset between offers to
// save.
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_ShouldRequestCardholderName_ResetBetweenConsecutiveSaves) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the |credit_card_save_manager_| is requesting cardholder name.
  EXPECT_TRUE(credit_card_save_manager_->should_request_name_from_user_);

  // Simulate a Chrome/Payments sync where billing_customer_number was newly
  // set.
  autofill_client_.GetPrefs()->SetDouble(prefs::kAutofillBillingCustomerNumber,
                                         12345);
  // Run through the form submit in exactly the same way (but now Chrome knows
  // that the user is a Google Payments customer).
  personal_data_.ClearCreditCards();
  personal_data_.ClearProfiles();
  FormSubmitted(credit_card_form);

  // Verify the |credit_card_save_manager_| is NOT requesting cardholder name.
  EXPECT_FALSE(credit_card_save_manager_->should_request_name_from_user_);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestCardholderNameIfTestingExperimentOn) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamAlwaysRequestCardholderName);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Even though everything went smoothly, because the "always request
  // cardholder name" experiment was enabled, verify that the correct histogram
  // entry and DetectedValue for "Cardholder name explicitly requested" was
  // logged.
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}
#endif

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_LogPreviousUseDate) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);
  const std::vector<AutofillProfile*>& profiles =
      personal_data_.GetProfilesToSuggest();
  ASSERT_EQ(1U, profiles.size());

  // Advance the current time and simulate use of the address profile.
  test_clock.SetNow(kMuchLaterTime);
  profiles[0]->RecordAndLogUse();

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the credit card form and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that UMA for "DaysSincePreviousUse" is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile",
      (kMuchLaterTime - kArbitraryTime).InDays(),
      /*expected_count=*/1);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_UploadDetailsFails) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
}

TEST_F(CreditCardSaveManagerTest, DuplicateMaskedCreditCard_NoUpload) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Add a masked credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111",
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Local save prompt should not be shown as there is alredy masked
  // card with same |TypeAndLastFourDigits|.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

// This class is parametrized to allow running all the inheriting tests with and
// without a specific feature enabled. See INSTANTIATE_TEST_CASE_P.
class CreditCardSaveManagerFeatureParameterizedTest
    : public CreditCardSaveManagerTest,
      public ::testing::WithParamInterface<
          /*enable_send_only_country_in_get_upload_details=*/bool> {
 public:
  CreditCardSaveManagerFeatureParameterizedTest() {}
  ~CreditCardSaveManagerFeatureParameterizedTest() override {}

  void SetUp() override {
    CreditCardSaveManagerTest::SetUp();
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillSendOnlyCountryInGetUploadDetails,
        /*enable_send_only_country_in_get_upload_details=*/GetParam());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditCardSaveManagerFeatureParameterizedTest);
};

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, NothingIfNothingFound) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(payments_client_->detected_values_in_upload_details(), 0);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectCvc) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value = CreditCardSaveManager::DetectedValue::CVC;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectCardholderName) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Smith");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectAddressName) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADDRESS_NAME;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectCardholderAndAddressNameIfMatching) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Smith");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bits.
  FormSubmitted(credit_card_form);
  int expected_detected_values =
      CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME |
      CreditCardSaveManager::DetectedValue::ADDRESS_NAME;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_values,
            expected_detected_values);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectNoUniqueNameIfNamesConflict) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Miles Prower");  // Conflict!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(payments_client_->detected_values_in_upload_details(), 0);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectPostalCode) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::POSTAL_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectNoUniquePostalCodeIfZipsConflict) {
  // Set up two new address profiles with conflicting postal codes.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  personal_data_.AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95051"), "en-US");
  personal_data_.AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(payments_client_->detected_values_in_upload_details(), 0);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectAddressLine) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADDRESS_LINE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectLocality) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value = CreditCardSaveManager::DetectedValue::LOCALITY;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectAdministrativeArea) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectCountryCode) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::COUNTRY_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectHasGooglePaymentAccount) {
  // Set the billing_customer_number Priority Preference to designate existence
  // of a Payments account.
  autofill_client_.GetPrefs()->SetDouble(prefs::kAutofillBillingCustomerNumber,
                                         12345);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest, DetectEverythingAtOnce) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Smith");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bits.
  FormSubmitted(credit_card_form);
  int expected_detected_values =
      CreditCardSaveManager::DetectedValue::CVC |
      CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME |
      CreditCardSaveManager::DetectedValue::ADDRESS_NAME |
      CreditCardSaveManager::DetectedValue::ADDRESS_LINE |
      CreditCardSaveManager::DetectedValue::LOCALITY |
      CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA |
      CreditCardSaveManager::DetectedValue::POSTAL_CODE |
      CreditCardSaveManager::DetectedValue::COUNTRY_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_values,
            expected_detected_values);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectSubsetOfPossibleFields) {
  // Set up a new address profile, taking out address line and state.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Miles Prower");  // Conflict!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bits.
  FormSubmitted(credit_card_form);
  int expected_detected_values =
      CreditCardSaveManager::DetectedValue::CVC |
      CreditCardSaveManager::DetectedValue::LOCALITY |
      CreditCardSaveManager::DetectedValue::POSTAL_CODE |
      CreditCardSaveManager::DetectedValue::COUNTRY_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_values,
            expected_detected_values);
}

// This test checks that ADDRESS_LINE, LOCALITY, ADMINISTRATIVE_AREA, and
// COUNTRY_CODE don't care about possible conflicts or consistency and are
// populated if even one address profile contains it.
TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       DetectAddressComponentsAcrossProfiles) {
  // Set up four new address profiles, each with a different address component.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."),
                   "en-US");
  personal_data_.AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  personal_data_.AddProfile(profile2);
  AutofillProfile profile3;
  profile3.set_guid("00000000-0000-0000-0000-000000000202");
  profile3.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  personal_data_.AddProfile(profile3);
  AutofillProfile profile4;
  profile4.set_guid("00000000-0000-0000-0000-000000000203");
  profile4.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile4);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name set
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bits.
  FormSubmitted(credit_card_form);
  int expected_detected_values =
      CreditCardSaveManager::DetectedValue::ADDRESS_LINE |
      CreditCardSaveManager::DetectedValue::LOCALITY |
      CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA |
      CreditCardSaveManager::DetectedValue::COUNTRY_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_values,
            expected_detected_values);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_LogAdditionalErrorsWithUploadDetailsFailure) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC!

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE |
                   AutofillMetrics::CVC_VALUE_NOT_FOUND,
               1 /* expected_num_matching_entries */);
}

TEST_P(
    CreditCardSaveManagerFeatureParameterizedTest,
    UploadCreditCard_ShouldOfferLocalSaveIfEverythingDetectedAndPaymentsDeclines) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Smith");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Because Payments rejects the offer to upload save but CVC + name + address
  // were all found, the local save prompt should be shown instead of the upload
  // prompt.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

TEST_P(
    CreditCardSaveManagerFeatureParameterizedTest,
    UploadCreditCard_ShouldOfferLocalSaveIfEverythingDetectedAndPaymentsDeclines_WithFirstAndLastName) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("John Smith"), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, /*is_https=*/true,
                               /*use_month_type=*/false, /*split_names=*/true);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("John");
  credit_card_form.fields[1].value = ASCIIToUTF16("Smith");
  credit_card_form.fields[2].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[3].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Because Payments rejects the offer to upload save but CVC + name + address
  // were all found, the local save prompt should be shown instead of the upload
  // prompt.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameOffered.Server", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCardWithFirstAndLastNameComplete.Server", 0);
}

TEST_P(
    CreditCardSaveManagerFeatureParameterizedTest,
    UploadCreditCard_ShouldNotOfferLocalSaveIfSomethingNotDetectedAndPaymentsDeclines) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC!

  base::HistogramTester histogram_tester;

  // Because Payments rejects the offer to upload save but not all of CVC + name
  // + address were detected, the local save prompt should not be shown either.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNoCvc) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC!

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectMetric(
      UkmCardUploadDecisionType::kUploadDecisionName,
      UkmCardUploadDecisionType::kEntryName,
      AutofillMetrics::UPLOAD_OFFERED | AutofillMetrics::CVC_VALUE_NOT_FOUND,
      1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNoName) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_OFFERED |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME,
               1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfConflictingNames) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Miles Prower");  // Conflict!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_OFFERED |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES,
               1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNoZip) {
  // Set up a new address profile without a postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("Flo Master"), "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_OFFERED |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE,
               1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfConflictingZips) {
  // Set up two new address profiles with conflicting postal codes.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(NAME_FULL, ASCIIToUTF16("Flo Master"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."),
                   "en-US");
  profile1.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(NAME_FULL, ASCIIToUTF16("Flo Master"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("234 Other Place"),
                   "en-US");
  profile2.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Fake City"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("Stateland"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("12345"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_OFFERED |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS,
               1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNothingFound) {
  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Testing St."), "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"), "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"), "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"), "en-US");
  personal_data_.AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("");  // No name!
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // No CVC!

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::UPLOAD_OFFERED |
                   AutofillMetrics::CVC_VALUE_NOT_FOUND |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE,
               1 /* expected_num_matching_entries */);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_UploadOfLocalCard) {
  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Flo Master", "4111111111111111",
                          NextMonth().c_str(), NextYear().c_str(), "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data_.AddCreditCard(local_card);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that metrics noted it was an existing local card for which credit
  // card upload was offered and accepted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadOfferedCardOrigin",
      AutofillMetrics::OFFERING_UPLOAD_OF_LOCAL_CARD, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadAcceptedCardOrigin",
      AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_LOCAL_CARD, 1);
}

TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_UploadOfNewCard) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that metrics noted it was a brand new card for which credit card
  // upload was offered and accepted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadOfferedCardOrigin",
      AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.UploadAcceptedCardOrigin",
      AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_NEW_CARD, 1);
}

// This test ensures that if offer-to-upload is denied by Google Payments, local
// save is not offered if the card is already a local card.
TEST_P(CreditCardSaveManagerFeatureParameterizedTest,
       UploadCreditCard_DenyingUploadOfLocalCardShouldNotOfferLocalSave) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Flo Master", "4111111111111111",
                          NextMonth().c_str(), NextYear().c_str(), "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data_.AddCreditCard(local_card);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither local or upload save should be offered in this case.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that metrics noted it was an existing local card for which credit
  // card upload was offered and accepted.
  histogram_tester.ExpectTotalCount("Autofill.UploadOfferedCardOrigin", 0);
  histogram_tester.ExpectTotalCount("Autofill.UploadAcceptedCardOrigin", 0);
}

// Every test will appear with suffix /0 (param false) and /1 (param true), e.g.
//  CreditCardSaveManagerFeatureParameterizedTest.NothingIfNothingFound/0:
//    Feature disabled
//  CreditCardSaveManagerFeatureParameterizedTest.NothingIfNothingFound/1:
//    Feature enabled.
INSTANTIATE_TEST_CASE_P(,  // Empty instatiation name.
                        CreditCardSaveManagerFeatureParameterizedTest,
                        ::testing::Values(false, true));

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_DoNotAddAnyFlagStatesToRequestIfExperimentsOff) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Confirm that upload happened and that no experiment flag state was sent in
  // the request.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->active_experiments_in_request().empty());
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_EloDisallowed) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamDisallowElo);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(kEloCardNumber);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With Elo disallowed, local save should be offered and upload save should
  // not.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUploadDisallowedForNetwork",
      AutofillMetrics::DISALLOWED_ELO, 1);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_EloAllowed) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamDisallowElo);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(kEloCardNumber);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the feature flag off, the Elo card should be allowed to be uploaded as
  // normal.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCardUploadDisallowedForNetwork", 0);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_JcbDisallowed) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamDisallowJcb);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(kJcbCardNumber);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With JCB disallowed, local save should be offered and upload save should
  // not.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUploadDisallowedForNetwork",
      AutofillMetrics::DISALLOWED_JCB, 1);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_JcbAllowed) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamDisallowJcb);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(kJcbCardNumber);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the feature flag off, the JCB card should be allowed to be uploaded as
  // normal.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.CreditCardUploadDisallowedForNetwork", 0);
}

// We can't tell what network a card is until *after* FormDataImporter imports
// it, making it possible to deny upload save for a pre-existing local card.
// This test ensures that we do not offer local save (again) for the card that
// FormDataImporter imported.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_DisallowedLocalCard) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamDisallowElo);

  // Add a local credit card that will match what we will enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Flo Master", kEloCardNumber,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data_.AddCreditCard(local_card);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(kEloCardNumber);
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The card is disallowed, but because it is already a local card, local save
  // should not be offered again.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUploadDisallowedForNetwork",
      AutofillMetrics::DISALLOWED_ELO, 1);
}

// Tests that a card with max strikes should still show the save bubble/infobar
// if the strike database flag is disabled.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_MaxStrikesButStrikeDatabaseDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was still shown because the strike
  // database flag was disabled.
  EXPECT_TRUE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that no histogram entries were logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenLocalCardSaved", 0);
}

// Tests that a card with max strikes should still show the save bubble/infobar
// if the strike database flag is disabled.
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_MaxStrikesButStrikeDatabaseDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was still shown because the strike
  // database flag was disabled.
  EXPECT_TRUE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that no histogram entries were logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenServerCardSaved", 0);
}

// Tests that a card with some strikes (but not max strikes) should still show
// the save bubble/infobar.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_NotEnoughStrikesStillShowsOfferToSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Add a single strike for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/1);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was still shown because the card did
  // not have too many strikes.
  EXPECT_TRUE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes", 0);
}

// Tests that a card with some strikes (but not max strikes) should still show
// the save bubble/infobar.
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NotEnoughStrikesStillShowsOfferToSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Add a single strike for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/1);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was still shown because the card did
  // not have too many strikes.
  EXPECT_TRUE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes", 0);
}

#if defined(OS_ANDROID) || defined(OS_IOS)
// Tests that a card with max strikes does not offer save on mobile at all.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_MaxStrikesDisallowsSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // No form of credit card save should be shown.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

// Tests that a card with max strikes does not offer save on mobile at all.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MaxStrikesDisallowsSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // No form of credit card save should be shown.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE);
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE);
}

#else  // !defined(OS_ANDROID) && !defined(OS_IOS)
// Tests that a card with max strikes should still offer to save on Desktop via
// the omnibox icon, but that the offer-to-save bubble itself is not shown.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_MaxStrikesStillAllowsSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was not shown because the card had too
  // many strikes.
  EXPECT_FALSE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

// Tests that a card with max strikes should still offer to save on Desktop via
// the omnibox icon, but that the offer-to-save bubble itself is not shown.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MaxStrikesStillAllowsSave) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Max out strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/3);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the offer-to-save bubble was not shown because the card had too
  // many strikes.
  EXPECT_FALSE(
      autofill_client_.get_offer_to_save_credit_card_bubble_was_shown());
  // Verify that the correct histogram entry was logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);
}
#endif

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, LocallySaveCreditCard_ClearStrikesOnAdd) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Add a couple of strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/2);
  EXPECT_EQ(2, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card reset the strike count for that card.
  EXPECT_EQ(0, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));
}

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ClearStrikesOnAdd) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Add a couple of strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/2);
  EXPECT_EQ(2, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card reset the strike count for that card.
  EXPECT_EQ(0, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));
}

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, LocallySaveCreditCard_NumStrikesLoggedOnAdd) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Add a couple of strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/2);
  EXPECT_EQ(2, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card logged the number of strikes it had previously.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.StrikesPresentWhenLocalCardSaved",
      /*sample=*/2, /*count=*/1);
}

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NumStrikesLoggedOnAdd) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillSaveCreditCardUsesStrikeSystem);

  // Add a couple of strikes for the card to be added.
  strike_database_->AddEntryWithNumStrikes(
      strike_database_->GetKeyForCreditCardSave("1111"), /*num_strikes=*/2);
  EXPECT_EQ(2, strike_database_->GetStrikesForTesting(
                   strike_database_->GetKeyForCreditCardSave("1111")));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16(NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(NextYear());
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card logged the number of strikes it had previously.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.StrikesPresentWhenServerCardSaved",
      /*sample=*/2, /*count=*/1);
}

}  // namespace autofill
