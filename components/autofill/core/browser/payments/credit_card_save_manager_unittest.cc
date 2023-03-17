// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/payments/test_virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/test_credit_card_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;

#if !BUILDFLAG(IS_IOS)
// time_t representation of 9th Sep, 2001 01:46:40 GMT
constexpr base::Time kArbitraryTime = base::Time::FromTimeT(1000000000);
constexpr base::Time kMuchLaterTime = base::Time::FromTimeT(1234567890);
#endif

// Used to configure form for |CreateTestCreditCardFormData|.
struct CreditCardFormOptions {
  CreditCardFormOptions& with_is_https(bool b) {
    is_https = b;
    return *this;
  }

  CreditCardFormOptions& with_split_names(bool b) {
    split_names = b;
    return *this;
  }

  CreditCardFormOptions& with_is_from_non_focusable_form(bool b) {
    is_from_non_focusable_form = b;
    return *this;
  }

  CreditCardFormOptions& with_is_google_host(bool b) {
    is_google_host = b;
    return *this;
  }
  // True if the scheme of a form is https.
  bool is_https = true;
  // True if the form is using both first name and last name field.
  bool split_names = false;
  // True if the form is a non-focusable form, such as a form that is hidden
  // after information has been entered into it.
  bool is_from_non_focusable_form = false;
  // True if the form is from Google-hosted website, such as payments.google.com
  // or YouTube.
  bool is_google_host = false;
};

}  // anonymous namespace

class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  ~MockPersonalDataManager() override = default;
  MOCK_METHOD(void, OnUserAcceptedUpstreamOffer, (), (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  explicit MockAutofillClient(
      std::unique_ptr<TestPersonalDataManager> pdm = nullptr) {
    set_personal_data_manager(
        pdm ? std::move(pdm) : std::make_unique<TestPersonalDataManager>());
  }
  ~MockAutofillClient() override = default;
  MOCK_METHOD(VirtualCardEnrollmentManager*,
              GetVirtualCardEnrollmentManager,
              (),
              (override));
};

class MockVirtualCardEnrollmentManager
    : public TestVirtualCardEnrollmentManager {
 public:
  MockVirtualCardEnrollmentManager(
      TestPersonalDataManager* personal_data_manager,
      payments::TestPaymentsClient* payments_client,
      TestAutofillClient* autofill_client)
      : TestVirtualCardEnrollmentManager(personal_data_manager,
                                         payments_client,
                                         autofill_client) {}
  MOCK_METHOD(
      void,
      InitVirtualCardEnroll,
      (const CreditCard& credit_card,
       VirtualCardEnrollmentSource virtual_card_enrollment_source,
       absl::optional<
           payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails>
           get_details_for_enrollment_response_details,
       const raw_ptr<PrefService> user_prefs,
       VirtualCardEnrollmentManager::RiskAssessmentFunction
           risk_assessment_function,
       VirtualCardEnrollmentManager::VirtualCardEnrollmentFieldsLoadedCallback
           virtual_card_enrollment_fields_loaded_callback),
      (override));
};

class CreditCardSaveManagerTest : public testing::Test {
 public:
  CreditCardSaveManagerTest() {
    scoped_feature_list_async_parse_form_.InitWithFeatureState(
        features::kAutofillParseAsync, true);
  }

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    personal_data().set_auto_accept_address_imports_for_testing(true);
    personal_data().Init(/*profile_database=*/database_,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr,
                         /*is_off_the_record=*/false);
    personal_data().OnSyncServiceInitialized(&sync_service_);
    autofill_driver_ = std::make_unique<TestAutofillDriver>();
    payments_client_ = new payments::TestPaymentsClient(
        autofill_client_.GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data());
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client_));
    virtual_card_enrollment_manager_ =
        std::make_unique<MockVirtualCardEnrollmentManager>(
            autofill_client_.GetPersonalDataManager(), payments_client_,
            &autofill_client_);
    ON_CALL(autofill_client_, GetVirtualCardEnrollmentManager())
        .WillByDefault(testing::Return(virtual_card_enrollment_manager_.get()));
    credit_card_save_manager_ =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data());
    credit_card_save_manager_->SetCreditCardUploadEnabled(true);
    autofill::TestFormDataImporter* test_form_data_importer =
        new TestFormDataImporter(
            &autofill_client_, payments_client_,
            std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager_),
            /*iban_save_manager=*/nullptr, &personal_data(), "en-US");
    autofill_client_.set_test_form_data_importer(
        std::unique_ptr<TestFormDataImporter>(test_form_data_importer));
    autofill_client_.GetStrikeDatabase();
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);
    browser_autofill_manager_->SetExpectedObservedSubmission(true);
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    browser_autofill_manager_.reset();
    autofill_driver_.reset();

    personal_data().SetPrefService(nullptr);
    personal_data().ClearCreditCards();
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    browser_autofill_manager_->OnFormsSeen(/*updated_forms=*/forms,
                                           /*removed_forms=*/{});
  }

  void FormSubmitted(const FormData& form) {
    browser_autofill_manager_->OnFormSubmitted(
        form, false, mojom::SubmissionSource::FORM_SUBMISSION);
  }

  void UserHasAcceptedUpload(
      AutofillClient::UserProvidedCardDetails user_provided_card_details) {
    credit_card_save_manager_->OnUserDidDecideOnUploadSave(
        AutofillClient::SaveCardOfferUserDecision::kAccepted,
        user_provided_card_details);
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms. The |form| can be
  // configured using the provided |options|.
  void CreateTestCreditCardFormData(FormData* form,
                                    CreditCardFormOptions options) {
    form->name = u"MyForm";
    std::u16string scheme = options.is_https ? u"https://" : u"http://";
    std::u16string host =
        options.is_google_host ? u"pay.google.com" : u"myform.com";
    std::u16string root_host =
        options.is_google_host ? u"pay.google.com" : u"myform.root.com";
    std::u16string form_path = u"/form.html";
    std::u16string submit_path = u"/submit.html";
    form->url = GURL(scheme + host + form_path);
    form->action = GURL(scheme + host + submit_path);
    form->main_frame_origin =
        url::Origin::Create(GURL(scheme + root_host + form_path));

    FormFieldData field;
    if (options.split_names) {
      test::CreateTestFormField("First Name on Card", "firstnameoncard", "",
                                "text", "cc-given-name", &field);
      form->fields.push_back(field);
      test::CreateTestFormField("Last Name on Card", "lastnameoncard", "",
                                "text", "cc-family-name", &field);
      form->fields.push_back(field);
    } else {
      test::CreateTestFormField("Name on Card", "nameoncard", "", "text",
                                &field);
      form->fields.push_back(field);
    }
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", "",
                              &field);
    field.is_focusable = !options.is_from_non_focusable_form;
    form->fields.push_back(field);
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
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
        field.value = u"123 Maple";
      else if (base::EqualsASCII(field.name, "city"))
        field.value = u"Dallas";
      else if (base::EqualsASCII(field.name, "state"))
        field.value = u"Texas";
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
    CreateTestCreditCardFormData(
        &form, CreditCardFormOptions().with_is_https(is_https));
    std::vector<FormData> forms(1, form);
    FormsSeen(forms);

    // Edit the data, and submit.
    form.fields[1].value = u"4111111111111111";
    form.fields[2].value = ASCIIToUTF16(test::NextMonth());
    form.fields[3].value = ASCIIToUTF16(test::NextYear());
    FormSubmitted(form);
    EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  }

  void ExpectUniqueFillableFormParsedUkm() {
    ukm::TestUkmRecorder* test_ukm_recorder =
        autofill_client_.GetTestUkmRecorder();
    auto entries = test_ukm_recorder->GetEntriesByName(
        UkmDeveloperEngagementType::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder->ExpectEntryMetric(
          entry, UkmDeveloperEngagementType::kDeveloperEngagementName,
          1 << AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS);
    }
  }

  void ExpectUniqueCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      autofill_metrics::CardUploadDecision metric) {
    histogram_tester.ExpectUniqueSample("Autofill.CardUploadDecisionMetric",
                                        ToHistogramSample(metric), 1);
  }

  void ExpectCardUploadDecision(const base::HistogramTester& histogram_tester,
                                autofill_metrics::CardUploadDecision metric) {
    histogram_tester.ExpectBucketCount("Autofill.CardUploadDecisionMetric",
                                       ToHistogramSample(metric), 1);
  }

  void ExpectNoCardUploadDecision(const base::HistogramTester& histogram_tester,
                                  autofill_metrics::CardUploadDecision metric) {
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
    ukm::TestUkmRecorder* test_ukm_recorder =
        autofill_client_.GetTestUkmRecorder();
    auto entries = test_ukm_recorder->GetEntriesByName(entry_name);
    EXPECT_EQ(expected_num_matching_entries, entries.size());
    for (const auto* const entry : entries) {
      test_ukm_recorder->ExpectEntryMetric(entry, metric_name,
                                           expected_metric_value);
    }
  }

 protected:
  MockPersonalDataManager& personal_data() {
    return static_cast<MockPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  MockAutofillClient autofill_client_{
      std::make_unique<MockPersonalDataManager>()};
  std::unique_ptr<MockVirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
  scoped_refptr<AutofillWebDataService> database_;
  syncer::TestSyncService sync_service_;
  // TODO(crbug.com/1291003): Refactor to use the real CreditCardSaveManager.
  // Ends up getting owned (and destroyed) by TestFormDataImporter:
  raw_ptr<TestCreditCardSaveManager> credit_card_save_manager_;
  // Ends up getting owned (and destroyed) by TestAutofillClient:
  raw_ptr<payments::TestPaymentsClient> payments_client_;
  // Ends up getting owned (and destroyed) by TestAutofillClient:
  raw_ptr<TestStrikeDatabase> strike_database_;

 private:
  int ToHistogramSample(autofill_metrics::CardUploadDecision metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }

  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
};

// Tests that credit card data are saved for forms on https
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
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
  CreateTestCreditCardFormData(&form,
                               CreditCardFormOptions().with_is_https(false));

  // Set "autocomplete=off" for cardnumber field.
  form.fields[1].should_autocomplete = false;

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  form.fields[1].value = u"4111111111111111";
  form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  form.fields[3].value = ASCIIToUTF16(test::NextYear());
  FormSubmitted(form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

// Tests that credit card data are not saved when CC number does not pass the
// Luhn test.
TEST_F(CreditCardSaveManagerTest, InvalidCreditCardNumberIsNotSaved) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, CreditCardFormOptions());
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  std::string card("4408041234567890");
  ASSERT_FALSE(autofill::IsValidCreditCardNumber(ASCIIToUTF16(card)));
  form.fields[1].value = ASCIIToUTF16(card);
  form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  form.fields[3].value = ASCIIToUTF16(test::NextYear());
  FormSubmitted(form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

TEST_F(CreditCardSaveManagerTest, CreditCardDisabledDoesNotSave) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // The credit card should neither be saved locally or uploaded.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_OnlyCountryInAddresses) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->client_behavior_signals_in_request().empty());

  // Verify that even though the full address profile was saved, only the
  // country was included in the upload details request to payments.
  EXPECT_EQ(1U, personal_data().GetProfiles().size());
  AutofillProfile only_country;
  only_country.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  EXPECT_EQ(1U, payments_client_->addresses_in_upload_details().size());
  // AutofillProfile::Compare will ignore the difference in guid between our
  // actual profile being sent and the expected one constructed here.
  EXPECT_EQ(0, payments_client_->addresses_in_upload_details()[0].Compare(
                   only_country));

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(personal_data().GetCreditCards().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED);

  // Simulate that the user has accepted the upload from the prompt.
  UserHasAcceptedUpload({});
  // We should find that full addresses are included in the UploadCard request,
  // even though only countries were included in GetUploadDetails.
  EXPECT_THAT(
      payments_client_->addresses_in_upload_card(),
      testing::UnorderedElementsAreArray({*personal_data().GetProfiles()[0]}));
}
#endif

// Tests that local save is not called when expiration date is missing.
TEST_F(CreditCardSaveManagerTest, LocalCreditCard_ExpirationDateMissing) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a expiration date, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";
  FormSubmitted(credit_card_form);

  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

// Tests metrics for supporting unfocused card form.
// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_OFFERED_FROM_NON_FOCUSABLE_FIELD);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_OFFERED_FROM_NON_FOCUSABLE_FIELD);
}
#endif

// Tests local card save will still work as usual when supporting unfocused card
// form feature is already on.
TEST_F(CreditCardSaveManagerTest, LocalCreditCard_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

// Tests that |has_non_focusable_field| is correctly sent to AutofillClient when
// offering local save.
TEST_F(CreditCardSaveManagerTest,
       Local_UploadDisabled_SaveCreditCardOptions_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().has_non_focusable_field);
}

// Tests that |has_non_focusable_field| is correctly sent to AutofillClient when
// upload failed because of unsupported bin and falling back to local save.
TEST_F(CreditCardSaveManagerTest,
       Local_UnsupportedCard_SaveCreditCardOptions_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);
  // Set supported bin range so that the used card is unsupported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(34, 34), std::make_pair(300, 305)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().has_non_focusable_field);
}

// Tests that |has_non_focusable_field| is correctly sent to AutofillClient when
// GetDetailsForSaveCard failed and falling back to local save.
TEST_F(
    CreditCardSaveManagerTest,
    Local_GetDetailsForSaveCardFails_SaveCreditCardOptions_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().has_non_focusable_field);
}

// Tests that |has_non_focusable_field| is correctly sent to AutofillClient when
// offering upload save.
TEST_F(CreditCardSaveManagerTest,
       Upload_SaveCreditCardOptions_WithNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions()
                                   .with_split_names(true)
                                   .with_is_from_non_focusable_form(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().has_non_focusable_field);
}

// Tests that |has_non_focusable_field| is not sent to AutofillClient when the
// form does not have any non-focusable fields.
TEST_F(CreditCardSaveManagerTest,
       SaveCreditCardOptions_WithoutNonFocusableField) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_FALSE(
      autofill_client_.get_save_credit_card_options().has_non_focusable_field);
}

// Tests that |from_dynamic_change_form| is correctly sent to AutofillClient
// when offering local save.
TEST_F(CreditCardSaveManagerTest,
       Local_UploadDisabled_SaveCreditCardOptions_WithDynamicForms) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  // Use the two same forms for FormsSeen to mock the dynamic change forms.
  FormsSeen({credit_card_form});
  FormsSeen({credit_card_form});

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().from_dynamic_change_form);
}

// Tests that |from_dynamic_change_form| is correctly sent to AutofillClient
// when upload failed because of unsupported bin and falling back to local save.
TEST_F(CreditCardSaveManagerTest,
       Local_UnsupportedCard_SaveCreditCardOptions_WithDynamicForms) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);
  // Set supported bin range so that the used card is unsupported.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(34, 34), std::make_pair(300, 305)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  // Use the two same forms for FormsSeen to mock the dynamic change forms.
  FormsSeen({credit_card_form});
  FormsSeen({credit_card_form});

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().from_dynamic_change_form);
}

// Tests that |from_dynamic_change_form| is correctly sent to AutofillClient
// when GetDetailsForSaveCard failed and falling back to local save.
TEST_F(
    CreditCardSaveManagerTest,
    Local_GetDetailsForSaveCardFails_SaveCreditCardOptions_WithDynamicForms) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  // Use the two same forms for FormsSeen to mock the dynamic change forms.
  FormsSeen({credit_card_form});
  FormsSeen({credit_card_form});

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().from_dynamic_change_form);
}

// Tests that |from_dynamic_change_form| is correctly sent to AutofillClient
// when offering upload save.
TEST_F(CreditCardSaveManagerTest,
       Upload_SaveCreditCardOptions_WithDynamicForms) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  // Use the two same forms for FormsSeen to mock the dynamic change forms.
  FormsSeen({credit_card_form});
  FormsSeen({credit_card_form});

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().from_dynamic_change_form);
}

// Tests that |from_dynamic_change_form| is not sent to AutofillClient when the
// form is not dynamically changing.
TEST_F(CreditCardSaveManagerTest, SaveCreditCardOptions_WithoutDynamicForms) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Set up our credit card form data without any non_focusable form field.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  // Only using one form for FormsSeen will not be treated as dynamic change
  // form.
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_FALSE(
      autofill_client_.get_save_credit_card_options().from_dynamic_change_form);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NotSavedLocally) {
  personal_data().ClearCreditCards();
  personal_data().ClearProfiles();

  credit_card_save_manager_->SetCreditCardUploadEnabled(true);

  payments::PaymentsClient::UploadCardResponseDetails
      upload_card_response_details;
  payments_client_->SetUploadCardResponseDetailsForUploadCard(
      upload_card_response_details);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Don't keep a copy of the card on this device.
  EXPECT_TRUE(personal_data().GetCreditCards().empty());
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_FeatureNotEnabled) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcUnavailable) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // CVC MISSING

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::CVC_VALUE_NOT_FOUND);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcInvalidLength) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"1234";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the invalid CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::INVALID_CVC_VALUE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::INVALID_CVC_VALUE);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MultipleCvcFields) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  credit_card_form.name = u"MyForm";
  credit_card_form.url = GURL("https://myform.com/form.html");
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
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // CVC MISSING
  credit_card_form.fields[5].value = u"123";

  base::HistogramTester histogram_tester;

  // A CVC value appeared in one of the two CVC fields, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoCvcFieldOnForm) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.  Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = u"MyForm";
  credit_card_form.url = GURL("https://myform.com/form.html");
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
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::CVC_FIELD_NOT_FOUND);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_InvalidCvcInNonCvcField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = u"MyForm";
  credit_card_form.url = GURL("https://myform.com/form.html");
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
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"1234";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the invalid CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::CVC_FIELD_NOT_FOUND);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInNonCvcField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = u"MyForm";
  credit_card_form.url = GURL("https://myform.com/form.html");
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
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInAddressField) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = u"MyForm";
  credit_card_form.url = GURL("https://myform.com/form.html");
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
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC value.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::CVC_FIELD_NOT_FOUND);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoRecentlyUsedProfile) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create, fill and submit an address form in order to establish a profile.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set the current time to another value.
  test_clock.SetNow(kMuchLaterTime);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CvcUnavailableAndNoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // CVC MISSING

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing CVC, name, and address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED | autofill_metrics::CVC_VALUE_NOT_FOUND |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
}
#endif

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(CreditCardSaveManagerTest,
       AttemptToOfferCardUploadSave_AutofillEnableNewSaveCardBubbleUiEnabled) {
  // Setting the flag to enable the new bubble for Save Card UI.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableNewSaveCardBubbleUi);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";
  FormSubmitted(credit_card_form);

  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Confirm that client_behavior_signals vector does contain the
  // FasterAndProtected signal.
  std::vector<ClientBehaviorConstants> client_behavior_signals_in_request =
      payments_client_->client_behavior_signals_in_request();
  EXPECT_THAT(
      client_behavior_signals_in_request,
      testing::Contains(ClientBehaviorConstants::kUsingFasterAndProtectedUi));
}

TEST_F(CreditCardSaveManagerTest,
       AttemptToOfferCardUploadSave_AutofillEnableNewSaveCardBubbleUiDisabled) {
  // Setting the flag to disable the new bubble for Save Card UI.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableNewSaveCardBubbleUi);

  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";
  FormSubmitted(credit_card_form);

  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Confirm that client_behavior_signals vector does not contain the
  // FasterAndProtected signal.
  std::vector<ClientBehaviorConstants> client_behavior_signals_in_request =
      payments_client_->client_behavior_signals_in_request();
  EXPECT_THAT(client_behavior_signals_in_request,
              testing::Not(testing::Contains(
                  ClientBehaviorConstants::kUsingFasterAndProtectedUi)));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoNameAvailableAndNoProfileAvailable) {
  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing names/address.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
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

  ManuallyFillAddressForm("Jane", "Doe", "77401-8294", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Jane", "Doe", "77401-1234", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(3 /* num_fillable_forms_parsed */);

  // Edit the data and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the conflicting zip codes.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ZipCodesDoNotDiscardWhitespace) {
  // Create two separate profiles with different zip codes. Must directly add
  // instead of submitting a form, because they're deduped on form submit.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000001");
  profile1.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  profile1.SetInfo(ADDRESS_HOME_ZIP, u"H3B2Y5", "en-US");
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, u"CA", "en-US");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000002");
  profile2.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  profile2.SetInfo(ADDRESS_HOME_ZIP, u"h3b 2y5", "en-US");
  profile2.SetInfo(ADDRESS_HOME_COUNTRY, u"CA", "en-US");
  personal_data().AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen({credit_card_form});

  // Edit the data and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the conflicting zip codes.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesHavePrefixMatch) {
  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Jane", "Doe", "77401-8294", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // One zip is a prefix of the other, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoZipCodeAvailable) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // Autofill's validation requirements for Venezuala ("VE", see
  // src/components/autofill/core/browser/geo/country_data.cc) do not require
  // zip codes. We use Venezuala here because to use the US (or one of many
  // other countries which autofill requires a zip code for) would result in no
  // address being imported at all, and then we never reach the check for
  // missing zip code in the upload code.
  ManuallyFillAddressForm("Jane", "Doe", "" /* zip_code */, "Venezuela",
                          &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing zip code.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED |
                              autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasMiddleInitial) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can be different case.
  ManuallyFillAddressForm("jane", "doe", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // And they can have a middle initial even if the other names don't.
  ManuallyFillAddressForm("Jane W", "Doe", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen({credit_card_form});

  // Edit the data, but use the name with a middle initial *and* period, and
  // submit.
  credit_card_form.fields[0].value = u"Jane W. Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoMiddleInitialInCCForm) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can have different variations of middle initials.
  ManuallyFillAddressForm("jane w.", "doe", "77401", "US", &address_form1);
  FormSubmitted(address_form1);
  ManuallyFillAddressForm("Jane W", "Doe", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen({credit_card_form});

  // Edit the data, but do not use middle initial.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(autofill_metrics::UPLOAD_OFFERED);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen({credit_card_form});

  // Edit the name by adding a middle name.
  credit_card_form.fields[0].value = u"John Quincy Adams";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasAddressMiddleName) {
  // Create, fill and submit address form with middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John Quincy", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen({credit_card_form});

  // Edit the name by removing middle name.
  credit_card_form.fields[0].value = u"John Adams";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NamesCanMismatch) {
  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = u"Different Person";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the mismatching names.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_IgnoreOldProfiles) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // Advance the current time. Since |address_form1| will not be a recently
  // used address profile, we will not include it in the candidate profiles.
  test_clock.SetNow(kMuchLaterTime);

  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Name matches recently used profile, should offer upload.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 autofill_metrics::UPLOAD_OFFERED);
}
#endif

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_RequestCardholderNameIfNameMissingAndNoPaymentsCustomer) {
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_RequestCardholderNameIfNameConflictingAndNoPaymentsCustomer) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but include a conflicting name, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
}

TEST_F(CreditCardSaveManagerTest,
       GoogleHostSite_ShouldNotOfferSaveIfUploadEnabled) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(
      &credit_card_form, CreditCardFormOptions().with_is_google_host(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // The credit card should neither be saved locally or uploaded.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(CreditCardSaveManagerTest,
       GoogleHostSite_ShouldOfferSaveIfUploadDisabled) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(
      &credit_card_form, CreditCardFormOptions().with_is_google_host(true));

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // The credit card should be saved locally.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameExistsAndNoPaymentsCustomer) {
// On iOS the cardholder name fix flow and expiration date fix flow no longer
// apply since the user is forced to always set correct data before submitting.
#if !BUILDFLAG(IS_IOS)
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Because everything went smoothly, verify that there was no histogram entry
  // or DetectedValue for "Cardholder name explicitly requested" logged.
  ExpectNoCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
#endif
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameMissingAndPaymentsCustomer) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
#endif
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestCardholderNameIfNameConflictingAndPaymentsCustomer) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but include a conflicting name, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);
#endif
}

// This test ensures |should_request_name_from_user_| is reset between offers to
// save.
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_ShouldRequestCardholderName_ResetBetweenConsecutiveSaves) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing name.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the |credit_card_save_manager_| is requesting cardholder name.
  EXPECT_TRUE(credit_card_save_manager_->should_request_name_from_user_);

  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Run through the form submit in exactly the same way (but now Chrome knows
  // that the user is a Google Payments customer).
  personal_data().ClearCreditCards();
  personal_data().ClearProfiles();
  FormSubmitted(credit_card_form);

  // Verify the |credit_card_save_manager_| is NOT requesting cardholder name.
  EXPECT_FALSE(credit_card_save_manager_->should_request_name_from_user_);
#endif
}

// This test ensures |should_request_expiration_date_from_user_|
// is reset between offers to save.
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_ShouldRequestExpirationDate_ResetBetweenConsecutiveSaves) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a expiration date, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing expiration date.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the |credit_card_save_manager_| is requesting expiration date.
  EXPECT_TRUE(
      credit_card_save_manager_->should_request_expiration_date_from_user_);

  // Edit the data, include a expiration date, and submit this time.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";
  FormSubmitted(credit_card_form);

  // Verify the |credit_card_save_manager_| is NOT requesting expiration date.
  EXPECT_FALSE(
      credit_card_save_manager_->should_request_expiration_date_from_user_);
#endif
}

// This test ensures |should_request_expiration_date_from_user_|
// is false when Wallet Sync Transport is enabled.
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_WalletSyncTransportEnabled_ShouldNotRequestExpirationDate) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
  // Wallet Sync Transport is enabled.
  personal_data().SetSyncAndSignInState(
      AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a expiration date, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  FormSubmitted(credit_card_form);

  // Save should not be offered because implicit Sync + Expiration date fix flow
  // aborts offering save
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
#endif
}

// This test ensures |should_request_expiration_date_from_user_|
// is true when Wallet Sync Transport is not enabled.
TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_WalletSyncTransportNotEnabled_ShouldRequestExpirationDate) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
  // Wallet Sync Transport is not enabled.
  personal_data().SetSyncAndSignInState(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a expiration date, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  // With the offer-to-save decision deferred to Google Payments, Payments can
  // still decide to allow saving despite the missing expiration date.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify the |credit_card_save_manager_| is requesting expiration date.
  EXPECT_TRUE(
      credit_card_save_manager_->should_request_expiration_date_from_user_);
#endif
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_DoNotRequestExpirationDateIfMissingNameAndExpirationDate) {
  // On iOS the cardholder name fix flow and expiration date fix flow no longer
  // apply since the user is forced to always set correct data before
  // submitting.
#if !BUILDFLAG(IS_IOS)
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
#endif
}

// iOS should always provide a valid expiration date when attempting to
// upload a Saved Card due to the Messages SaveCard modal.
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_AlwaysRequestCardholderNameAndExpirationDateOnIOS) {
#if BUILDFLAG(IS_IOS)
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
  EXPECT_TRUE(payments_client_->detected_values_in_upload_details() &
              CreditCardSaveManager::DetectedValue::USER_PROVIDED_NAME);

#endif
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestExpirationDateViaExpDateFixFlow) {
#if BUILDFLAG(IS_IOS)
  // iOS should always provide a valid expiration date when attempting to
  // upload a Saved Card due to the Messages SaveCard modal. The manager
  // shouldn't handle expired dates.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardInfobarEditSupport)) {
    return;
  }
#endif

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);
  // Verify that the correct histogram entry and DetectedValue for "Expiration
  // date explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardRequestExpirationDateReason",
      autofill_metrics::SaveCardRequestExpirationDateReason::
          kMonthAndYearMissing,
      1);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestExpirationDateIfOnlyMonthMissing) {
#if BUILDFLAG(IS_IOS)
  // iOS should always provide a valid expiration date when attempting to
  // upload a Saved Card due to the Messages SaveCard modal. The manager
  // shouldn't handle expired dates.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardInfobarEditSupport)) {
    return;
  }
#endif

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);
  // Verify that the correct histogram entry and DetectedValue for "Expiration
  // date explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardRequestExpirationDateReason",
      autofill_metrics::SaveCardRequestExpirationDateReason::kMonthMissingOnly,
      1);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestExpirationDateIfOnlyYearMissing) {
#if BUILDFLAG(IS_IOS)
  // iOS should always provide a valid expiration date when attempting to
  // upload a Saved Card due to the Messages SaveCard modal. The manager
  // shouldn't handle expired dates.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardInfobarEditSupport)) {
    return;
  }
#endif

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = u"";
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);
  // Verify that the correct histogram entry and DetectedValue for "Expiration
  // date explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardRequestExpirationDateReason",
      autofill_metrics::SaveCardRequestExpirationDateReason::kYearMissingOnly,
      1);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestExpirationDateIfExpirationDateInputIsExpired) {
#if BUILDFLAG(IS_IOS)
  // iOS should always provide a valid expiration date when attempting to
  // upload a Saved Card due to the Messages SaveCard modal. The manager
  // shouldn't handle expired dates.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardInfobarEditSupport)) {
    return;
  }
#endif

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"09";
  credit_card_form.fields[3].value = u"2000";
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);
  // Verify that the correct histogram entry and DetectedValue for "Expiration
  // date explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardRequestExpirationDateReason",
      autofill_metrics::SaveCardRequestExpirationDateReason::
          kExpirationDatePresentButExpired,
      1);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_RequestExpirationDateIfExpirationDateInputIsTwoDigitAndExpired) {
#if BUILDFLAG(IS_IOS)
  // iOS should always provide a valid expiration date when attempting to
  // upload a Saved Card due to the Messages SaveCard modal. The manager
  // shouldn't handle expired dates.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCardInfobarEditSupport)) {
    return;
  }
#endif

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("John", "Smith", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data with 2 digit year and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = u"01";
  credit_card_form.fields[3].value = u"10";
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);
  // Verify that the correct histogram entry and DetectedValue for "Expiration
  // date explicitly requested" was logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCardRequestExpirationDateReason",
      autofill_metrics::SaveCardRequestExpirationDateReason::
          kExpirationDatePresentButExpired,
      1);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(
      payments_client_->detected_values_in_upload_details() &
      CreditCardSaveManager::DetectedValue::USER_PROVIDED_EXPIRATION_DATE);
}

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_UploadDetailsFails) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
}
#endif

TEST_F(CreditCardSaveManagerTest, DuplicateMaskedCreditCard_NoUpload) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Add a masked credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Jane Doe", "1111",
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data().AddServerCreditCard(credit_card);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Local save prompt should not be shown as there is already masked
  // card with same |TypeAndLastFourDigits|.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

TEST_F(CreditCardSaveManagerTest, NothingIfNothingFound) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  int detected_values = payments_client_->detected_values_in_upload_details();
  EXPECT_FALSE(detected_values & CreditCardSaveManager::DetectedValue::CVC);
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME);
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::ADDRESS_NAME);
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::POSTAL_CODE);
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::COUNTRY_CODE);
  EXPECT_FALSE(
      detected_values &
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT);
}

TEST_F(CreditCardSaveManagerTest, DetectCvc) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value = CreditCardSaveManager::DetectedValue::CVC;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectCardholderName) {
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectAddressName) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADDRESS_NAME;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectCardholderAndAddressNameIfMatching) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

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

TEST_F(CreditCardSaveManagerTest, DetectNoUniqueNameIfNamesConflict) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Miles Prower";  // Conflict!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  int detected_values = payments_client_->detected_values_in_upload_details();
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME);
  EXPECT_FALSE(detected_values &
               CreditCardSaveManager::DetectedValue::ADDRESS_NAME);
}

TEST_F(CreditCardSaveManagerTest, DetectPostalCode) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::POSTAL_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectNoUniquePostalCodeIfZipsConflict) {
  // Set up two new address profiles with conflicting postal codes.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  personal_data().AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(ADDRESS_HOME_ZIP, u"95051", "en-US");
  personal_data().AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and check what detected_values for an upload save would be.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(payments_client_->detected_values_in_upload_details() &
               CreditCardSaveManager::DetectedValue::POSTAL_CODE);
}

TEST_F(CreditCardSaveManagerTest, DetectAddressLine) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADDRESS_LINE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectLocality) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value = CreditCardSaveManager::DetectedValue::LOCALITY;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectAdministrativeArea) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectCountryCode) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::COUNTRY_CODE;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectHasGooglePaymentAccount) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

  // Submit the form and ensure the detected_values for an upload save contained
  // the expected bit.
  FormSubmitted(credit_card_form);
  int expected_detected_value =
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;
  EXPECT_EQ(payments_client_->detected_values_in_upload_details() &
                expected_detected_value,
            expected_detected_value);
}

TEST_F(CreditCardSaveManagerTest, DetectEverythingAtOnce) {
  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

TEST_F(CreditCardSaveManagerTest, DetectSubsetOfPossibleFields) {
  // Set up a new address profile, taking out address line and state.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Miles Prower";  // Conflict!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
TEST_F(CreditCardSaveManagerTest, DetectAddressComponentsAcrossProfiles) {
  // Set up four new address profiles, each with a different address component.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  personal_data().AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  personal_data().AddProfile(profile2);
  AutofillProfile profile3;
  profile3.set_guid("00000000-0000-0000-0000-000000000202");
  profile3.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  personal_data().AddProfile(profile3);
  AutofillProfile profile4;
  profile4.set_guid("00000000-0000-0000-0000-000000000203");
  profile4.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile4);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name set
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC set

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

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_LogAdditionalErrorsWithUploadDetailsFailure) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC!

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  int upload_decision =
      autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE |
      autofill_metrics::CVC_VALUE_NOT_FOUND |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName, upload_decision,
               1 /* expected_num_matching_entries */);
}
#endif

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_ShouldOfferLocalSaveIfEverythingDetectedAndPaymentsDeclines) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"John Smith", "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"John Smith";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Because Payments rejects the offer to upload save but CVC + name + address
  // were all found, the local save prompt should be shown instead of the upload
  // prompt.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

TEST_F(
    CreditCardSaveManagerTest,
    UploadCreditCard_ShouldNotOfferLocalSaveIfSomethingNotDetectedAndPaymentsDeclines) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC!

  base::HistogramTester histogram_tester;

  // Because Payments rejects the offer to upload save but not all of CVC + name
  // + address were detected, the local save prompt should not be shown either.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNoCvc) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC!

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectMetric(
      UkmCardUploadDecisionType::kUploadDecisionName,
      UkmCardUploadDecisionType::kEntryName,
      autofill_metrics::UPLOAD_OFFERED | autofill_metrics::CVC_VALUE_NOT_FOUND,
      1 /* expected_num_matching_entries */);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
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
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  int upload_decision =
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName, upload_decision,
               1 /* expected_num_matching_entries */);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfConflictingNames) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Miles Prower";  // Conflict!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  int upload_decision =
      autofill_metrics::UPLOAD_OFFERED |
      autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName, upload_decision,
               1 /* expected_num_matching_entries */);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNoZip) {
  // Set up a new address profile without a postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               autofill_metrics::UPLOAD_OFFERED |
                   autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE,
               1 /* expected_num_matching_entries */);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfConflictingZips) {
  // Set up two new address profiles with conflicting postal codes.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000200");
  profile1.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile1.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile1.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile1.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile1);
  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000201");
  profile2.SetInfo(NAME_FULL, u"Jane Doe", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"234 Other Place", "en-US");
  profile2.SetInfo(ADDRESS_HOME_CITY, u"Fake City", "en-US");
  profile2.SetInfo(ADDRESS_HOME_STATE, u"Stateland", "en-US");
  profile2.SetInfo(ADDRESS_HOME_ZIP, u"12345", "en-US");
  profile2.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(
      histogram_tester, autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               autofill_metrics::UPLOAD_OFFERED |
                   autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS,
               1 /* expected_num_matching_entries */);
}
#endif

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_PaymentsDecidesOfferToSaveIfNothingFound) {
  // Set up a new address profile without a name or postal code.
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000200");
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Testing St.", "en-US");
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", "en-US");
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", "en-US");
  personal_data().AddProfile(profile);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"";  // No name!
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"";  // No CVC!

  base::HistogramTester histogram_tester;

  // Payments should be asked whether upload save can be offered.
  // (Unit tests assume they reply yes and save is successful.)
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, autofill_metrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME);
  ExpectCardUploadDecision(histogram_tester,
                           autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME);
  // Verify that the correct UKM was logged.
  int upload_decision =
      autofill_metrics::UPLOAD_OFFERED | autofill_metrics::CVC_VALUE_NOT_FOUND |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME |
      autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE |
      autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName, upload_decision,
               1 /* expected_num_matching_entries */);
}
#endif

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_UploadOfLocalCard) {
  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Jane Doe", "4111111111111111",
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data().AddCreditCard(local_card);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_UploadOfNewCard) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_DenyingUploadOfLocalCardShouldNotOfferLocalSave) {
  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Add a local credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Jane Doe", "4111111111111111",
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data().AddCreditCard(local_card);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_DoNotAddAnyFlagStatesToRequestIfExperimentsOff) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Confirm that upload happened and that no experiment flag state was sent in
  // the request.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
  EXPECT_TRUE(payments_client_->client_behavior_signals_in_request().empty());
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ShouldAddBillableServiceNumberInRequest) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Confirm that the preflight request contained
  // kUploadCardBillableServiceNumber in the request.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(payments::kUploadCardBillableServiceNumber,
            payments_client_->billable_service_number_in_request());
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ShouldAddBillingCustomerNumberInRequest) {
  // Set the billing_customer_number to designate existence of a Payments
  // account.
  personal_data().SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Confirm that the preflight request contained billing customer number in the
  // request.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(123456L, payments_client_->billing_customer_number_in_request());
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ShouldAddUploadCardSourceInRequest) {
  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Confirm that the preflight request contained the correct UploadCardSource.
  FormSubmitted(credit_card_form);
  EXPECT_EQ(payments::PaymentsClient::UploadCardSource::UPSTREAM_CHECKOUT_FLOW,
            payments_client_->upload_card_source_in_request());
}

// Tests that a card with some strikes (but not max strikes) should still show
// the save bubble/infobar.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_NotEnoughStrikesStillShowsOfferToSave) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add a single strike for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(1, credit_card_save_strike_database.GetStrikes("1111"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add a single strike for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(1, credit_card_save_strike_database.GetStrikes("1111"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Tests that a card with max strikes does not offer save on mobile at all.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_MaxStrikesDisallowsSave) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Max out strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(3, credit_card_save_strike_database.GetStrikes("1111"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

// TODO(crbug.com/1113034): Create an equivalent test for iOS, or skip
// permanently if the test doesn't apply to iOS flow.
#if !BUILDFLAG(IS_IOS)
// Tests that a card with max strikes does not offer save on mobile at all.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MaxStrikesDisallowsSave) {
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Max out strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(3, credit_card_save_strike_database.GetStrikes("1111"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  // No form of credit card save should be shown.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE);
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.CreditCardSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::SERVER, 1);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      autofill_metrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE);
}
#endif

#else  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests that a card with max strikes should still offer to save on Desktop via
// the omnibox icon, but that the offer-to-save bubble itself is not shown.
TEST_F(CreditCardSaveManagerTest,
       LocallySaveCreditCard_MaxStrikesStillAllowsSave) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Max out strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(3, credit_card_save_strike_database.GetStrikes("1111"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Max out strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(3, credit_card_save_strike_database.GetStrikes("1111"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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

// Tests that 2 LocalCardMigrationStrikes are removed when cards are saved
// locally.
TEST_F(CreditCardSaveManagerTest,
       LocalCreditCard_LocalCardMigrationStrikesRemovedOnLocalSave) {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);

  // Start with 3 strikes in |local_card_migration_strike_database|.
  local_card_migration_strike_database.AddStrikes(3);
  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(), 3);

  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // 2 strikes should be removed when card was saved locally.
  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(), 1);
}

// Tests that no LocalCardMigrationStrikes get removed due to cards being
// uploaded.
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoLocalSaveMigrationStrikesRemovedOnUpload) {
  LocalCardMigrationStrikeDatabase local_card_migration_strike_database =
      LocalCardMigrationStrikeDatabase(strike_database_);

  // Start with 3 strikes in |local_card_migration_strike_database|.
  local_card_migration_strike_database.AddStrikes(3);
  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(), 3);

  credit_card_save_manager_->SetCreditCardUploadEnabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data with credit card first and last name
  // fields.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form,
                               CreditCardFormOptions().with_split_names(true));
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane";
  credit_card_form.fields[1].value = u"Doe";
  credit_card_form.fields[2].value = u"4111111111111111";
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[4].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[5].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Strike count shouldn't change.
  EXPECT_EQ(local_card_migration_strike_database.GetStrikes(), 3);
}
#endif

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, LocallySaveCreditCard_ClearStrikesOnAdd) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add two strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(2, credit_card_save_strike_database.GetStrikes("1111"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card reset the strike count for that card.
  EXPECT_EQ(0, credit_card_save_strike_database.GetStrikes("1111"));
}

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ClearStrikesOnAdd) {
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add two strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(2, credit_card_save_strike_database.GetStrikes("1111"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card reset the strike count for that card.
  EXPECT_EQ(0, credit_card_save_strike_database.GetStrikes("1111"));
}

// Tests that adding a card clears all strikes for that card.
TEST_F(CreditCardSaveManagerTest, LocallySaveCreditCard_NumStrikesLoggedOnAdd) {
  credit_card_save_manager_->SetCreditCardUploadEnabled(false);

  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add two strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(2, credit_card_save_strike_database.GetStrikes("1111"));

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(1 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

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
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);

  // Add two strikes for the card to be added.
  credit_card_save_strike_database.AddStrike("1111");
  credit_card_save_strike_database.AddStrike("1111");
  EXPECT_EQ(2, credit_card_save_strike_database.GetStrikes("1111"));

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;

  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());

  // Verify that adding the card logged the number of strikes it had previously.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.StrikesPresentWhenServerCardSaved",
      /*sample=*/2, /*count=*/1);
}

// Tests that one strike is added when upload failed and
// bubble is shown.
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NumStrikesLoggedOnUploadNotSuccess) {
  payments::PaymentsClient::UploadCardResponseDetails
      upload_card_response_details;
  payments_client_->SetUploadCardResponseDetailsForUploadCard(
      upload_card_response_details);
  TestCreditCardSaveStrikeDatabase credit_card_save_strike_database =
      TestCreditCardSaveStrikeDatabase(strike_database_);
  EXPECT_EQ(0, credit_card_save_strike_database.GetStrikes("1111"));

  // If upload failed and the bubble was shown, strike count should increase
  // by 1.
  credit_card_save_manager_->set_show_save_prompt(true);
  credit_card_save_manager_->set_upload_request_card_number(
      u"4111111111111111");
  credit_card_save_manager_->OnDidUploadCard(
      AutofillClient::PaymentsRpcResult::kTryAgainFailure,
      upload_card_response_details);
  EXPECT_EQ(1, credit_card_save_strike_database.GetStrikes("1111"));
}

// Make sure that the PersonalDataManager gets notified when the user accepts
// an upload offer.
TEST_F(CreditCardSaveManagerTest, OnUserDidAcceptUpload_NotifiesPDM) {
  EXPECT_CALL(personal_data(), OnUserAcceptedUpstreamOffer);

  // Simulate that the user has accepted the upload from the prompt.
  UserHasAcceptedUpload({});
}

// Tests that if a card doesn't fall in any of the supported bin ranges, local
// save is offered rather than upload save.
TEST_F(CreditCardSaveManagerTest, UploadSaveNotOfferedForUnsupportedCard) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(4111, 4113), std::make_pair(34, 34),
      std::make_pair(300, 305)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"5454545454545454";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Since card isn't in any of the supported ranges, local save should be
  // offered and upload save should not.
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_FALSE(credit_card_save_manager_->CreditCardWasUploaded());
}

// Tests that if a card doesn't fall in any of the supported bin ranges, but is
// already saved, then local save is not offered.
TEST_F(CreditCardSaveManagerTest, LocalSaveNotOfferedForSavedUnsupportedCard) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(4111, 4113), std::make_pair(34, 34),
      std::make_pair(300, 305)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);
  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Add a local credit card whose number matches what we will
  // enter below.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Jane Doe", "5454545454545454",
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  local_card.set_record_type(CreditCard::LOCAL_CARD);
  personal_data().AddCreditCard(local_card);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"5454545454545454";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Since card is already saved, local save should not be offered.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
}

// Tests that if a card falls in one of the supported bin ranges, upload save
// is offered.
TEST_F(CreditCardSaveManagerTest, UploadSaveOfferedForSupportedCard) {
  // Set supported BIN ranges.
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(4111, 4113)};
  payments_client_->SetSupportedBINRanges(supported_card_bin_ranges);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = u"4111111111111111";
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  // Since card is in one of the supported ranges(4111-4113), upload save should
  // be offered.
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_client_.ConfirmSaveCardLocallyWasCalled());
  EXPECT_TRUE(credit_card_save_manager_->CreditCardWasUploaded());
}

// Tests that if payment client returns an invalid legal message upload should
// not be offered.
TEST_F(CreditCardSaveManagerTest, InvalidLegalMessageInOnDidGetUploadDetails) {
  payments_client_->SetUseInvalidLegalMessageInGetUploadDetails(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(
      histogram_tester,
      autofill_metrics::UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE);
  EXPECT_FALSE(
      autofill_client_.get_save_credit_card_options().has_multiple_legal_lines);
}

// Tests that has_multiple_legal_lines is set correctly in
// SaveCreditCardOptions.
TEST_F(CreditCardSaveManagerTest, LegalMessageInOnDidGetUploadDetails) {
  payments_client_->SetUseLegalMessageWithMultipleLinesInGetUploadDetails(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Jane", "Doe", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, CreditCardFormOptions());
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = u"Jane Doe";
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16(test::NextMonth());
  credit_card_form.fields[3].value = ASCIIToUTF16(test::NextYear());
  credit_card_form.fields[4].value = u"123";

  base::HistogramTester histogram_tester;
  FormSubmitted(credit_card_form);

  // Verify has_multiple_legal_lines is set correctly.
  EXPECT_TRUE(
      autofill_client_.get_save_credit_card_options().has_multiple_legal_lines);
}

// Tests that the fields in the card are set correctly and virtual card
// enrollment is offered when a card becomes eligible after upload.
TEST_F(CreditCardSaveManagerTest, OnDidUploadCard_VirtualCardEnrollment) {
  for (CreditCard::VirtualCardEnrollmentState enrollment_state :
       {CreditCard::VirtualCardEnrollmentState::UNENROLLED,
        CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE,
        CreditCard::VirtualCardEnrollmentState::ENROLLED}) {
    for (bool is_update_virtual_card_enrollment_enabled : {true, false}) {
      base::test::ScopedFeatureList feature_list;
      if (is_update_virtual_card_enrollment_enabled) {
        feature_list.InitAndEnableFeature(
            features::kAutofillEnableUpdateVirtualCardEnrollment);
      } else {
        feature_list.InitAndDisableFeature(
            features::kAutofillEnableUpdateVirtualCardEnrollment);
      }
      payments::PaymentsClient::UploadCardResponseDetails
          upload_card_response_details;
      upload_card_response_details.card_art_url =
          GURL("https://www.example.com/");
      upload_card_response_details.instrument_id = 9223372036854775807;
      upload_card_response_details.virtual_card_enrollment_state =
          enrollment_state;

      CreditCard arg_credit_card;
      VirtualCardEnrollmentSource arg_virtual_card_enrollment_source;
      if (is_update_virtual_card_enrollment_enabled &&
          enrollment_state ==
              CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE) {
        EXPECT_CALL(autofill_client_, GetVirtualCardEnrollmentManager).Times(1);
        EXPECT_CALL(*virtual_card_enrollment_manager_,
                    InitVirtualCardEnroll(_, _, _, _, _, _))
            .WillOnce(DoAll(SaveArg<0>(&arg_credit_card),
                            SaveArg<1>(&arg_virtual_card_enrollment_source)));
      }

      credit_card_save_manager_->set_upload_request_card(test::GetCreditCard());
      credit_card_save_manager_->OnDidUploadCard(
          AutofillClient::PaymentsRpcResult::kSuccess,
          upload_card_response_details);

      // The condition inside of this if-statement is true if virtual card
      // enrollment should be offered.
      if (is_update_virtual_card_enrollment_enabled &&
          enrollment_state ==
              CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE) {
        EXPECT_EQ(arg_credit_card.card_art_url(),
                  upload_card_response_details.card_art_url);
        EXPECT_EQ(arg_credit_card.instrument_id(),
                  upload_card_response_details.instrument_id);
        EXPECT_EQ(arg_credit_card.virtual_card_enrollment_state(),
                  upload_card_response_details.virtual_card_enrollment_state);
        EXPECT_EQ(arg_virtual_card_enrollment_source,
                  VirtualCardEnrollmentSource::kUpstream);
      } else {
        EXPECT_TRUE(arg_credit_card.card_art_url().is_empty());
        EXPECT_EQ(arg_credit_card.instrument_id(), 0);
        EXPECT_EQ(arg_credit_card.virtual_card_enrollment_state(),
                  CreditCard::VirtualCardEnrollmentState::UNSPECIFIED);
      }
    }
  }
}

TEST_F(
    CreditCardSaveManagerTest,
    OnDidUploadCard_VirtualCardEnrollment_GetDetailsForEnrollmentResponseDetailsReturned) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableUpdateVirtualCardEnrollment);
  payments::PaymentsClient::UploadCardResponseDetails
      upload_card_response_details;
  upload_card_response_details.card_art_url = GURL("https://example.com/");
  upload_card_response_details.instrument_id = 9223372036854775807;
  upload_card_response_details.virtual_card_enrollment_state =
      CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE;

  payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails
      get_details_for_enrollment_response_details;
  get_details_for_enrollment_response_details.vcn_context_token =
      "test_context_token";
  get_details_for_enrollment_response_details.google_legal_message = {
      TestLegalMessageLine("test_google_legal_message")};
  get_details_for_enrollment_response_details.issuer_legal_message = {
      TestLegalMessageLine("test_issuer_legal_message")};
  upload_card_response_details.get_details_for_enrollment_response_details =
      get_details_for_enrollment_response_details;
  credit_card_save_manager_->set_upload_request_card(test::GetCreditCard());

  CreditCard arg_credit_card;
  VirtualCardEnrollmentSource arg_virtual_card_enrollment_source;
  absl::optional<
      payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails>
      arg_get_details_for_enrollment_response_details;
  EXPECT_CALL(autofill_client_, GetVirtualCardEnrollmentManager).Times(1);
  EXPECT_CALL(*virtual_card_enrollment_manager_,
              InitVirtualCardEnroll(_, _, _, _, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&arg_credit_card),
                SaveArg<1>(&arg_virtual_card_enrollment_source),
                SaveArg<2>(&arg_get_details_for_enrollment_response_details)));

  credit_card_save_manager_->OnDidUploadCard(
      AutofillClient::PaymentsRpcResult::kSuccess,
      upload_card_response_details);

  EXPECT_EQ(arg_credit_card.card_art_url(),
            upload_card_response_details.card_art_url);
  EXPECT_EQ(arg_credit_card.instrument_id(),
            upload_card_response_details.instrument_id);
  EXPECT_EQ(arg_credit_card.virtual_card_enrollment_state(),
            upload_card_response_details.virtual_card_enrollment_state);
  EXPECT_EQ(arg_virtual_card_enrollment_source,
            VirtualCardEnrollmentSource::kUpstream);
  EXPECT_EQ(
      arg_get_details_for_enrollment_response_details.value().vcn_context_token,
      get_details_for_enrollment_response_details.vcn_context_token);
  EXPECT_TRUE(arg_get_details_for_enrollment_response_details.value()
                  .google_legal_message[0]
                  .text() == get_details_for_enrollment_response_details
                                 .google_legal_message[0]
                                 .text());
  EXPECT_TRUE(arg_get_details_for_enrollment_response_details.value()
                  .issuer_legal_message[0]
                  .text() == get_details_for_enrollment_response_details
                                 .issuer_legal_message[0]
                                 .text());
}

}  // namespace autofill
