// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_download_manager.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_form_structure.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using testing::_;
using testing::AnyOf;
using testing::AtLeast;
using testing::Contains;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Not;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace autofill {

using features::kAutofillEnforceMinRequiredFieldsForHeuristics;
using features::kAutofillEnforceMinRequiredFieldsForQuery;
using features::kAutofillEnforceMinRequiredFieldsForUpload;
using features::kAutofillRestrictUnownedFieldsToFormlessCheckout;
using mojom::ButtonTitleType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

const int kDefaultPageID = 137;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    ON_CALL(*this, GetChannel())
        .WillByDefault(Return(version_info::Channel::UNKNOWN));
  }

  ~MockAutofillClient() override {}

  MOCK_METHOD0(ShouldShowSigninPromo, bool());
  MOCK_CONST_METHOD0(GetChannel, version_info::Channel());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class MockAutofillDownloadManager : public TestAutofillDownloadManager {
 public:
  MockAutofillDownloadManager(AutofillDriver* driver,
                              AutofillDownloadManager::Observer* observer)
      : TestAutofillDownloadManager(driver, observer) {}

  MOCK_METHOD6(StartUploadRequest,
               bool(const FormStructure&,
                    bool,
                    const ServerFieldTypeSet&,
                    const std::string&,
                    bool,
                    PrefService*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

void ExpectFilledField(const char* expected_label,
                       const char* expected_name,
                       const char* expected_value,
                       const char* expected_form_control_type,
                       const FormFieldData& field) {
  SCOPED_TRACE(expected_label);
  EXPECT_EQ(UTF8ToUTF16(expected_label), field.label);
  EXPECT_EQ(UTF8ToUTF16(expected_name), field.name);
  EXPECT_EQ(UTF8ToUTF16(expected_value), field.value);
  EXPECT_EQ(expected_form_control_type, field.form_control_type);
}

// Verifies that the |filled_form| has been filled with the given data.
// Verifies address fields if |has_address_fields| is true, and verifies
// credit card fields if |has_credit_card_fields| is true. Verifies both if both
// are true. |use_month_type| is used for credit card input month type.
void ExpectFilledForm(int page_id,
                      const FormData& filled_form,
                      int expected_page_id,
                      const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* phone,
                      const char* email,
                      const char* name_on_card,
                      const char* card_number,
                      const char* expiration_month,
                      const char* expiration_year,
                      bool has_address_fields,
                      bool has_credit_card_fields,
                      bool use_month_type) {
  // The number of fields in the address and credit card forms created above.
  const size_t kAddressFormSize = 11;
  const size_t kCreditCardFormSize = use_month_type ? 4 : 5;

  EXPECT_EQ(expected_page_id, page_id);
  EXPECT_EQ(ASCIIToUTF16("MyForm"), filled_form.name);
  if (has_credit_card_fields) {
    EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);
  } else {
    EXPECT_EQ(GURL("http://myform.com/form.html"), filled_form.url);
    EXPECT_EQ(GURL("http://myform.com/submit.html"), filled_form.action);
  }

  size_t form_size = 0;
  if (has_address_fields)
    form_size += kAddressFormSize;
  if (has_credit_card_fields)
    form_size += kCreditCardFormSize;
  ASSERT_EQ(form_size, filled_form.fields.size());

  if (has_address_fields) {
    ExpectFilledField("First Name", "firstname", first, "text",
                      filled_form.fields[0]);
    ExpectFilledField("Middle Name", "middlename", middle, "text",
                      filled_form.fields[1]);
    ExpectFilledField("Last Name", "lastname", last, "text",
                      filled_form.fields[2]);
    ExpectFilledField("Address Line 1", "addr1", address1, "text",
                      filled_form.fields[3]);
    ExpectFilledField("Address Line 2", "addr2", address2, "text",
                      filled_form.fields[4]);
    ExpectFilledField("City", "city", city, "text", filled_form.fields[5]);
    ExpectFilledField("State", "state", state, "text", filled_form.fields[6]);
    ExpectFilledField("Postal Code", "zipcode", postal_code, "text",
                      filled_form.fields[7]);
    ExpectFilledField("Country", "country", country, "text",
                      filled_form.fields[8]);
    ExpectFilledField("Phone Number", "phonenumber", phone, "tel",
                      filled_form.fields[9]);
    ExpectFilledField("Email", "email", email, "email", filled_form.fields[10]);
  }

  if (has_credit_card_fields) {
    size_t offset = has_address_fields ? kAddressFormSize : 0;
    ExpectFilledField("Name on Card", "nameoncard", name_on_card, "text",
                      filled_form.fields[offset + 0]);
    ExpectFilledField("Card Number", "cardnumber", card_number, "text",
                      filled_form.fields[offset + 1]);
    if (use_month_type) {
      std::string exp_year = expiration_year;
      std::string exp_month = expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty())
        date = exp_year + "-" + exp_month;

      ExpectFilledField("Expiration Date", "ccmonth", date.c_str(), "month",
                        filled_form.fields[offset + 2]);
    } else {
      ExpectFilledField("Expiration Date", "ccmonth", expiration_month, "text",
                        filled_form.fields[offset + 2]);
      ExpectFilledField("", "ccyear", expiration_year, "text",
                        filled_form.fields[offset + 3]);
    }
  }
}

void ExpectFilledAddressFormElvis(int page_id,
                                  const FormData& filled_form,
                                  int expected_page_id,
                                  bool has_credit_card_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "Elvis", "Aaron",
                   "Presley", "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901",
                   "theking@gmail.com", "", "", "", "", true,
                   has_credit_card_fields, false);
}

void ExpectFilledCreditCardFormElvis(int page_id,
                                     const FormData& filled_form,
                                     int expected_page_id,
                                     bool has_address_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "", "", "", "", "",
                   "", "", "", "", "", "", "Elvis Presley", "4234567890123456",
                   "04", "2999", has_address_fields, true, false);
}

void ExpectFilledCreditCardYearMonthWithYearMonth(int page_id,
                                                  const FormData& filled_form,
                                                  int expected_page_id,
                                                  bool has_address_fields,
                                                  const char* year,
                                                  const char* month) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "", "", "", "", "",
                   "", "", "", "", "", "", "Miku Hatsune", "4234567890654321",
                   month, year, has_address_fields, true, true);
}

void CheckThatOnlyFieldByIndexHasThisPossibleType(
    const FormStructure& form_structure,
    size_t field_index,
    ServerFieldType type,
    FieldPropertiesMask mask) {
  EXPECT_TRUE(field_index < form_structure.field_count());

  for (size_t i = 0; i < form_structure.field_count(); i++) {
    if (i == field_index) {
      EXPECT_THAT(form_structure.field(i)->possible_types(), ElementsAre(type));
      EXPECT_EQ(mask, form_structure.field(i)->properties_mask);
    } else {
      EXPECT_THAT(form_structure.field(i)->possible_types(),
                  Not(Contains(type)));
    }
  }
}

void CheckThatNoFieldHasThisPossibleType(const FormStructure& form_structure,
                                         ServerFieldType type) {
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    EXPECT_THAT(form_structure.field(i)->possible_types(), Not(Contains(type)));
  }
}

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() {}

  // Mock methods to enable testability.
  MOCK_METHOD3(SendFormDataToRenderer,
               void(int query_id,
                    RendererFormDataAction action,
                    const FormData& data));

  MOCK_METHOD1(SendAutofillTypePredictionsToRenderer,
               void(const std::vector<FormStructure*>& forms));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

}  // namespace

class AutofillManagerTest : public testing::Test {
 public:
  AutofillManagerTest() {}

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.Init(/*profile_database=*/database_,
                        /*account_database=*/nullptr,
                        /*pref_service=*/autofill_client_.GetPrefs(),
                        /*identity_manager=*/nullptr,
                        /*client_profile_validator=*/nullptr,
                        /*history_service=*/nullptr,
                        /*is_off_the_record=*/false);
    personal_data_.SetPrefService(autofill_client_.GetPrefs());

    autocomplete_history_manager_ =
        std::make_unique<MockAutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(
        /*profile_database=*/database_,
        /*is_off_the_record=*/false);

    autofill_driver_ =
        std::make_unique<testing::NiceMock<MockAutofillDriver>>();
    payments::TestPaymentsClient* payments_client =
        new payments::TestPaymentsClient(
            autofill_driver_->GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data_);
    autofill_client_.set_test_payments_client(
        std::unique_ptr<payments::TestPaymentsClient>(payments_client));
    TestCreditCardSaveManager* credit_card_save_manager =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client, &personal_data_);
    credit_card_save_manager->SetCreditCardUploadEnabled(true);
    TestFormDataImporter* test_form_data_importer = new TestFormDataImporter(
        &autofill_client_, payments_client,
        std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager),
        &personal_data_, "en-US");
    autofill_client_.set_test_form_data_importer(
        std::unique_ptr<autofill::TestFormDataImporter>(
            test_form_data_importer));
    autofill_manager_ = std::make_unique<TestAutofillManager>(
        autofill_driver_.get(), &autofill_client_, &personal_data_,
        autocomplete_history_manager_.get());
    download_manager_ = new MockAutofillDownloadManager(
        autofill_driver_.get(), autofill_manager_.get());
    // AutofillManager takes ownership of |download_manager_|.
    autofill_manager_->set_download_manager(download_manager_);
    external_delegate_ = std::make_unique<TestAutofillExternalDelegate>(
        autofill_manager_.get(), autofill_driver_.get(),
        /*call_parent_methods=*/false);
    autofill_manager_->SetExternalDelegate(external_delegate_.get());

    // Initialize the TestPersonalDataManager with some default data.
    CreateTestAutofillProfiles();
    CreateTestCreditCards();
  }

  void CreateTestServerCreditCards() {
    personal_data_.ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data_.AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data_.AddServerCreditCard(full_server_card);
  }

  void CreateTestServerAndLocalCreditCards() {
    personal_data_.ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data_.AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data_.AddServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    personal_data_.AddCreditCard(local_card);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    personal_data_.SetPrefService(nullptr);
    personal_data_.ClearCreditCards();
  }

  void GetAutofillSuggestions(int query_id,
                              const FormData& form,
                              const FormFieldData& field) {
    autofill_manager_->OnQueryFormFieldAutofill(
        query_id, form, field, gfx::RectF(),
        /*autoselect_first_suggestion=*/false);
  }

  void GetAutofillSuggestions(const FormData& form,
                              const FormFieldData& field) {
    GetAutofillSuggestions(kDefaultPageID, form, field);
  }

  void AutocompleteSuggestionsReturned(
      const std::vector<base::string16>& results,
      int query_id = kDefaultPageID) {
    std::vector<Suggestion> suggestions;
    std::transform(results.begin(), results.end(),
                   std::back_inserter(suggestions),
                   [](auto result) { return Suggestion(result); });

    autofill_manager_->OnSuggestionsReturned(
        query_id, /*autoselect_first_suggestion=*/false, suggestions);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms, base::TimeTicks());
  }

  void FormSubmitted(const FormData& form) {
    autofill_manager_->OnFormSubmitted(form, false,
                                       SubmissionSource::FORM_SUBMISSION);
  }

  void FillAutofillFormData(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    autofill_manager_->FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_FILL,
                                         query_id, form, field, unique_id);
  }

  // Calls |autofill_manager_->OnFillAutofillFormData()| with the specified
  // input parameters after setting up the expectation that the mock driver's
  // |SendFormDataToRenderer()| method will be called and saving the parameters
  // of that call into the |response_query_id| and |response_data| output
  // parameters.
  void FillAutofillFormDataAndSaveResults(int input_query_id,
                                          const FormData& input_form,
                                          const FormFieldData& input_field,
                                          int unique_id,
                                          int* response_query_id,
                                          FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _))
        .WillOnce((DoAll(testing::SaveArg<0>(response_query_id),
                         testing::SaveArg<2>(response_data))));
    FillAutofillFormData(input_query_id, input_form, input_field, unique_id);
  }

  int MakeFrontendID(const std::string& cc_sid,
                     const std::string& profile_sid) const {
    return autofill_manager_->MakeFrontendID(cc_sid, profile_sid);
  }

  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field) {
    return autofill_manager_->WillFillCreditCardNumber(form, field);
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->name = ASCIIToUTF16("MyForm");
    if (is_https) {
      form->url = GURL("https://myform.com/form.html");
      form->action = GURL("https://myform.com/submit.html");
    } else {
      form->url = GURL("http://myform.com/form.html");
      form->action = GURL("http://myform.com/submit.html");
    }

    FormFieldData field;
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
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

  void PrepareForRealPanResponse(FormData* form, CreditCard* card) {
    // This line silences the warning from PaymentsClient about matching sync
    // and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    CreateTestCreditCardFormData(form, true, false);
    FormsSeen(std::vector<FormData>(1, *form));
    *card = CreditCard(CreditCard::MASKED_SERVER_CARD, "a123");
    test::SetCreditCardInfo(card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card->SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _))
        .Times(AtLeast(1));
    autofill_manager_->FillOrPreviewCreditCardForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, kDefaultPageID, *form,
        form->fields[0], card);
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) {
    payments::FullCardRequest* full_card_request =
        autofill_manager_->credit_card_access_manager_->cvc_authenticator_
            ->full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = base::ASCIIToUTF16("123");
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsClient::UnmaskResponseDetails response;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        autofill_manager_->credit_card_access_manager_
            ->GetOrCreateCVCAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

  void DisableCreditCardAutofill() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillCreditCardAblationExperiment);
  }

  // Wrappers around the TestAutofillExternalDelegate::GetSuggestions call that
  // take a hardcoded number of expected results so callsites are cleaner.
  void CheckSuggestions(int expected_page_id, const Suggestion& suggestion0) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    external_delegate_->CheckSuggestions(expected_page_id, 1,
                                         &suggestion_vector[0]);
  }
  void CheckSuggestions(int expected_page_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    external_delegate_->CheckSuggestions(expected_page_id, 2,
                                         &suggestion_vector[0]);
  }
  void CheckSuggestions(int expected_page_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1,
                        const Suggestion& suggestion2) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    suggestion_vector.push_back(suggestion2);
    external_delegate_->CheckSuggestions(expected_page_id, 3,
                                         &suggestion_vector[0]);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockAutofillClient autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  std::unique_ptr<TestAutofillExternalDelegate> external_delegate_;
  scoped_refptr<AutofillWebDataService> database_;
  MockAutofillDownloadManager* download_manager_;
  TestPersonalDataManager personal_data_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  int ToHistogramSample(AutofillMetrics::CardUploadDecisionMetric metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }

  void CreateTestAutofillProfiles() {
    AutofillProfile profile1;
    test::SetProfileInfo(&profile1, "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile1.set_guid("00000000-0000-0000-0000-000000000001");
    personal_data_.AddProfile(profile1);

    AutofillProfile profile2;
    test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile2.set_guid("00000000-0000-0000-0000-000000000002");
    personal_data_.AddProfile(profile2);

    AutofillProfile profile3;
    test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "",
                         "");
    profile3.set_guid("00000000-0000-0000-0000-000000000003");
    personal_data_.AddProfile(profile3);
  }

  void CreateTestCreditCards() {
    CreditCard credit_card1;
    test::SetCreditCardInfo(&credit_card1, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card1.set_guid("00000000-0000-0000-0000-000000000004");
    credit_card1.set_use_count(10);
    credit_card1.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(5));
    personal_data_.AddCreditCard(credit_card1);

    CreditCard credit_card2;
    test::SetCreditCardInfo(&credit_card2, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card2.set_guid("00000000-0000-0000-0000-000000000005");
    credit_card2.set_use_count(5);
    credit_card2.set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(4));
    personal_data_.AddCreditCard(credit_card2);

    CreditCard credit_card3;
    test::SetCreditCardInfo(&credit_card3, "", "", "", "", "");
    credit_card3.set_guid("00000000-0000-0000-0000-000000000006");
    personal_data_.AddCreditCard(credit_card3);
  }
};

class SuggestionMatchingTest
    : public AutofillManagerTest,
      public testing::WithParamInterface<std::tuple<bool, std::string>> {
 protected:
  void SetUp() override {
    AutofillManagerTest::SetUp();
    InitializeFeatures();
  }

#if defined(OS_ANDROID) || defined(OS_IOS)
  void InitializeFeatures();
#else
  void InitializeFeatures();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  std::string MakeLabel(const std::vector<std::string>& parts);
  std::string MakeMobileLabel(const std::vector<std::string>& parts);

  enum class EnabledFeature { kNone, kDesktop, kMobileShowAll, kMobileShowOne };
  EnabledFeature enabled_feature_;
  base::test::ScopedFeatureList features_;
};

#if defined(OS_ANDROID) || defined(OS_IOS)
void SuggestionMatchingTest::InitializeFeatures() {
  if (std::get<0>(GetParam())) {
    std::string variant = std::get<1>(GetParam());

    if (variant ==
        features::kAutofillUseMobileLabelDisambiguationParameterShowAll) {
      enabled_feature_ = EnabledFeature::kMobileShowAll;
    } else if (variant ==
               features::
                   kAutofillUseMobileLabelDisambiguationParameterShowOne) {
      enabled_feature_ = EnabledFeature::kMobileShowOne;
    } else {
      NOTREACHED();
    }

    std::map<std::string, std::string> parameters;
    parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
        variant;
    features_.InitAndEnableFeatureWithParameters(
        features::kAutofillUseMobileLabelDisambiguation, parameters);
  } else {
    enabled_feature_ = EnabledFeature::kNone;
  }
}
#else
void SuggestionMatchingTest::InitializeFeatures() {
  enabled_feature_ = std::get<0>(GetParam()) ? EnabledFeature::kDesktop
                                             : EnabledFeature::kNone;
  features_.InitWithFeatureState(
      features::kAutofillUseImprovedLabelDisambiguation,
      std::get<0>(GetParam()));
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

std::string SuggestionMatchingTest::MakeLabel(
    const std::vector<std::string>& parts) {
  return base::JoinString(
      parts, l10n_util::GetStringUTF8(IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR));
}

std::string SuggestionMatchingTest::MakeMobileLabel(
    const std::vector<std::string>& parts) {
  return base::JoinString(
      parts, l10n_util::GetStringUTF8(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR));
}

class CreditCardSuggestionMatchingTest
    : public AutofillManagerTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    AutofillManagerTest::SetUp();
    features_.InitWithFeatureState(features::kAutofillKeyboardAccessory,
                                   GetParam());
  }

  base::test::ScopedFeatureList features_;
};

// Test that calling OnFormsSeen with an empty set of forms (such as when
// reloading a page or when the renderer processes a set of forms but detects
// no changes) does not load the forms again.
TEST_F(AutofillManagerTest, OnFormsSeen_Empty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);

  base::HistogramTester histogram_tester;
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);

  // No more forms, metric is not logged.
  forms.clear();
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
}

// Test that calling OnFormsSeen consecutively with a different set of forms
// will query for each separately.
TEST_F(AutofillManagerTest, OnFormsSeen_DifferentFormStructures) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);

  base::HistogramTester histogram_tester;
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
  download_manager_->VerifyLastQueriedForms(forms);

  // Different form structure.
  FormData form2;
  form2.name = ASCIIToUTF16("MyForm");
  form2.url = GURL("https://myform.com/form.html");
  form2.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form2.fields.push_back(field);

  forms.clear();
  forms.push_back(form2);
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 2);
  download_manager_->VerifyLastQueriedForms(forms);
}

// Test that when forms are seen, the renderer is updated with the predicted
// field types
TEST_F(AutofillManagerTest, OnFormsSeen_SendAutofillTypePredictionsToRenderer) {
  // Set up a queryable form.
  FormData form1;
  test::CreateTestAddressFormData(&form1);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.name = ASCIIToUTF16("NonQueryable");
  form2.url = form1.url;
  form2.action = GURL("https://myform.com/submit.html");
  form2.fields.push_back(field);

  // Package the forms for observation.
  std::vector<FormData> forms{form1, form2};

  // Setup expectations.
  EXPECT_CALL(*autofill_driver_, SendAutofillTypePredictionsToRenderer(_))
      .Times(2);
  FormsSeen(forms);
}

// Test that no autofill suggestions are returned for a field with an
// unrecognized autocomplete attribute.
TEST_F(AutofillManagerTest, GetProfileSuggestions_UnrecognizedAttribute) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that autocomplete manager is not called for suggestions either.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  // Suggestions should be returned for the first two fields.
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 2);
  GetAutofillSuggestions(form, form.fields[1]);
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 2);

  // No suggestions should not be provided for the third field because of its
  // unrecognized autocomplete attribute.
  GetAutofillSuggestions(form, form.fields[2]);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

// Test that when small forms are disabled (min required fields enforced) no
// suggestions are returned when there are less than three fields and none of
// them have an autocomplete attribute.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_NoAutocomplete) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillEnforceMinRequiredFieldsForHeuristics);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that autocomplete manager is called for both fields.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(2);

  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());

  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that when small forms are disabled (min required fields enforced)
// for a form with two fields with one that has an autocomplete attribute,
// suggestions are only made for the one that has the attribute.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_WithOneAutocomplete) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillEnforceMinRequiredFieldsForHeuristics);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Check that suggestions are made for the field that has the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(kDefaultPageID, Suggestion("Charles", "", "", 1),
                   Suggestion("Elvis", "", "", 2));

  // Check that there are no suggestions for the field without the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that suggestions are returned by default when there are less than
// three fields and none of them have an autocomplete attribute.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_NoMinFieldsEnforced_NoAutocomplete) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      kAutofillEnforceMinRequiredFieldsForHeuristics);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that autocomplete manager is called for both fields.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Charles", "Charles Hardin Holley", "", 1),
                   Suggestion("Elvis", "Elvis Aaron Presley", "", 2));

  GetAutofillSuggestions(form, form.fields[1]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Holley", "Charles Hardin Holley", "", 1),
                   Suggestion("Presley", "Elvis Aaron Presley", "", 2));
}

// Test that for form with two fields with one that has an autocomplete
// attribute, suggestions are made for both if small form support is enabled
// (no mininum number of fields enforced).
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_NoMinFieldsEnforced_WithOneAutocomplete) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      kAutofillEnforceMinRequiredFieldsForHeuristics);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Charles", "Charles Hardin Holley", "", 1),
                   Suggestion("Elvis", "Elvis Aaron Presley", "", 2));

  GetAutofillSuggestions(form, form.fields[1]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Holley", "Charles Hardin Holley", "", 1),
                   Suggestion("Presley", "Elvis Aaron Presley", "", 2));
}

// Test that for a form with two fields with autocomplete attributes,
// suggestions are made for both fields. This is true even if a minimum number
// of fields is enforced.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_SmallFormWithTwoAutocomplete) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillEnforceMinRequiredFieldsForHeuristics);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Charles", "Charles Hardin Holley", "", 1),
                   Suggestion("Elvis", "Elvis Aaron Presley", "", 2));

  GetAutofillSuggestions(form, form.fields[1]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Holley", "Charles Hardin Holley", "", 1),
                   Suggestion("Presley", "Elvis Aaron Presley", "", 2));
}

// Test that the call is properly forwarded to AutocompleteHistoryManager.
TEST_F(AutofillManagerTest, OnAutocompleteEntrySelected) {
  base::string16 test_value = ASCIIToUTF16("TestValue");
  EXPECT_CALL(*autocomplete_history_manager_.get(),
              OnAutocompleteEntrySelected(test_value))
      .Times(1);

  autofill_manager_->OnAutocompleteEntrySelected(test_value);
}

// Test that we return all address profile suggestions when all form fields
// are empty.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Charles", label1, "", 1),
                   Suggestion("Elvis", label2, "", 2));
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Elvis", label, "", 1));
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that we merge identical values.
TEST_P(SuggestionMatchingTest,
       GetProfileSuggestions_AlreadyAutofilledMergeValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  // Two profiles have the same last name, and the third shares the same first
  // letter for last name.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000103");
  profile1.SetInfo(NAME_FIRST, ASCIIToUTF16("Robin"), "en-US");
  profile1.SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                   "en-US");
  personal_data_.AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, ASCIIToUTF16("Carl"), "en-US");
  profile2.SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                   "en-US");
  personal_data_.AddProfile(profile2);

  AutofillProfile profile3;
  profile3.set_guid("00000000-0000-0000-0000-000000000126");
  profile3.SetInfo(NAME_FIRST, ASCIIToUTF16("Aaron"), "en-US");
  profile3.SetInfo(NAME_LAST, ASCIIToUTF16("Googler"), "en-US");
  profile3.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1600 Amphitheater pkwy"),
                   "en-US");
  personal_data_.AddProfile(profile3);

  FormFieldData field;
  test::CreateTestFormField("Last Name", "lastname", "G", "text", &field);
  GetAutofillSuggestions(form, field);

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      CheckSuggestions(kDefaultPageID,
                       Suggestion("Googler", "1600 Amphitheater pkwy", "", 1),
                       Suggestion("Grimes", "1234 Smith Blvd.", "", 2));
      break;
    case EnabledFeature::kNone:
      // Test that we sent the right values to the external delegate. No labels
      // with duplicate values "Grimes" merged.
      CheckSuggestions(
          kDefaultPageID,
          Suggestion("Googler", "1600 Amphitheater pkwy", "", 1),
          Suggestion("Grimes", "1234 Smith Blvd., Carl Grimes", "", 2),
          Suggestion("Grimes", "1234 Smith Blvd., Robin Grimes", "", 3));
  }
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that they have no label.
TEST_P(SuggestionMatchingTest,
       GetProfileSuggestions_AlreadyAutofilledNoLabels) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Elvis", label, "", 1));
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(AutofillManagerTest, GetProfileSuggestions_UnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Username", "username", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Quest", "quest", "", "quest", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Color", "color", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we cull duplicate profile suggestions.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_WithDuplicates) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Add a duplicate profile.
  AutofillProfile duplicate_profile = *(personal_data_.GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000001"));
  personal_data_.AddProfile(duplicate_profile);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Charles", label1, "", 1),
                   Suggestion("Elvis", label2, "", 2));
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(AutofillManagerTest, GetProfileSuggestions_AutofillDisabledByUser) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Disable Autofill.
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(AutofillManagerTest, OnSuggestionsReturned_CallsExternalDelegate) {
  std::vector<Suggestion> suggestions = {
      Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2)};

  {
    autofill_manager_->OnSuggestionsReturned(
        kDefaultPageID, /*autoselect_first_suggestion=*/false, suggestions);

    EXPECT_FALSE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(kDefaultPageID, suggestions[0], suggestions[1]);
  }
  {
    autofill_manager_->OnSuggestionsReturned(
        kDefaultPageID, /*autoselect_first_suggestion=*/true, suggestions);

    EXPECT_TRUE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(kDefaultPageID, suggestions[0], suggestions[1]);
  }
}

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the credit card suggestions to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has whitespace in it.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_Whitespace) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = ASCIIToUTF16("       ");
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it, which should be removed.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_StopCharsOnly) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = ASCIIToUTF16("____-____-____-____");
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has some invisible unicode characters in it.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_InvisibleUnicodeOnly) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = base::string16({0x200E, 0x200F});
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it and some input.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_StopCharsWithInput) {
  // Add a credit card with particular numbers that we will attempt to recall.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Smith",
                          "5255667890123123",  // Mastercard
                          "08", "2017", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];

  field.value = ASCIIToUTF16("5255-66__-____-____");
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string master_card_label = std::string("08/17");
#else
  const std::string master_card_label = std::string("Expires on 08/17");
#endif

  // Test that we sent the right value to the external delegate.
  CheckSuggestions(kDefaultPageID,
                   Suggestion(std::string("Mastercard  ") +
                                  test::ObfuscatedCardDigitsAsUTF8("3123"),
                              master_card_label, kMasterCard,
                              autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return only matching credit card profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "78", "text", &field);
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
#else
  const std::string visa_label = std::string("Expires on 04/99");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that we return credit card profile suggestions when the selected form
// field is not the credit card number field.
TEST_P(CreditCardSuggestionMatchingTest, GetCreditCardSuggestions_NonCCNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  const std::string obfuscated_last_four_digits1 =
      test::ObfuscatedCardDigitsAsUTF8("3456");
  const std::string obfuscated_last_four_digits2 =
      test::ObfuscatedCardDigitsAsUTF8("8765");

#if defined(OS_ANDROID)
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits1
          : std::string("Visa  ") + obfuscated_last_four_digits1;
  const std::string master_card_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits2
          : std::string("Mastercard  ") + obfuscated_last_four_digits2;

#elif defined(OS_IOS)
  const std::string visa_label = obfuscated_last_four_digits1;
  const std::string master_card_label = obfuscated_last_four_digits2;

#else
  const std::string visa_label = base::JoinString(
      {"Visa  ", obfuscated_last_four_digits1, ", expires on 04/99"}, "");
  const std::string master_card_label = base::JoinString(
      {"Mastercard  ", obfuscated_last_four_digits2, ", expires on 10/98"}, "");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Elvis Presley", visa_label, kVisaCard,
                              autofill_manager_->GetPackedCreditCardID(4)),
                   Suggestion("Buddy Holly", master_card_label, kMasterCard,
                              autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we will eventually return the credit card signin promo when there
// are no credit card suggestions and the promo is active. See the tests in
// AutofillExternalDelegateTest that test whether the promo is added.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_OnlySigninPromo) {
  personal_data_.ClearCreditCards();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[1];

  ON_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillByDefault(Return(true));
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(2);
  EXPECT_TRUE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Autocomplete suggestions are not queried.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);

  // Test that we sent no values to the external delegate. It will add the promo
  // before passing along the results.
  external_delegate_->CheckNoSuggestions(kDefaultPageID);

  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the page is secure, but the form action URL is valid but
// not secure.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_FormActionNotHTTPS) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, /* is_https= */ true, false);
  // However we set the action (target URL) to be HTTP after all.
  form.action = GURL("http://myform.com/submit.html");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID,
                   Suggestion(l10n_util::GetStringUTF8(
                                  IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                              "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return credit card suggestions for secure pages that have an
// empty form action target URL.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_EmptyFormAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card suggestions for secure pages that have a
// form action set to "javascript:something".
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_JavascriptFormAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Have the form action be a javascript function (which is a valid URL).
  form.action = GURL("javascript:alert('Hello');");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_RepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "5231567890123456",  // Mastercard
                          "05", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  credit_card.set_use_date(AutofillClock::Now() -
                           base::TimeDelta::FromDays(15));
  personal_data_.AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label1 = std::string("10/98");
  const std::string master_card_label2 = std::string("05/99");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label1 = std::string("Expires on 10/98");
  const std::string master_card_label2 = std::string("Expires on 05/99");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label1, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("3456"),
                 master_card_label2, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_P(SuggestionMatchingTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Charles", label1, "", 1),
                   Suggestion("Elvis", label2, "", 2));

  const int kPageID2 = 2;
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(kPageID2, form, field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the credit card suggestions to the external delegate.
  CheckSuggestions(
      kPageID2,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that for non-https forms with both address and credit card fields, we
// only return address suggestions. Instead of credit card suggestions, we
// should return a warning explaining that credit card profile suggestions are
// unavailable when the form is not https.
TEST_F(AutofillManagerTest, GetAddressAndCreditCardSuggestionsNonHttps) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kPageID2,
                   Suggestion(l10n_util::GetStringUTF8(
                                  IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                              "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

TEST_F(AutofillManagerTest,
       ShouldShowAddressSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kAutofillCreditCardAblationExperiment);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[0];

  GetAutofillSuggestions(form, field);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(AutofillManagerTest,
       ShouldNotShowCreditCardsSuggestionsIfCreditCardAutofillDisabled) {
  DisableCreditCardAutofill();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Check that credit card suggestions will not be available.
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

TEST_F(AutofillManagerTest,
       ShouldLogFormSubmitEventIfCreditCardAutofillDisabled) {
  DisableCreditCardAutofill();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  base::HistogramTester histogram_tester;
  FormSubmitted(form);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
                                     1);
}

// Test that we return normal Autofill suggestions when trying to autofill
// already filled forms.
TEST_P(SuggestionMatchingTest, GetFieldSuggestionsWhenFormIsAutofilled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Mark one of the fields as filled.
  form.fields[2].is_autofilled = true;
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Charles", label1, "", 1),
                   Suggestion("Elvis", label2, "", 2));
}

// Test that nothing breaks when there are autocomplete suggestions but no
// autofill suggestions.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForAutocompleteOnly) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Some Field", "somefield", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<base::string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("one"));
  suggestions.push_back(ASCIIToUTF16("two"));
  AutocompleteSuggestionsReturned(suggestions);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("one", "", "", 0),
                   Suggestion("two", "", "", 0));
}

// Test that we do not return duplicate values drawn from multiple profiles when
// filling an already filled field.
TEST_P(SuggestionMatchingTest, GetFieldSuggestionsWithDuplicateValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "", "", "", "", "", "", "", "", "",
                       "", "");
  profile.set_guid("00000000-0000-0000-0000-000000000101");
  personal_data_.AddProfile(profile);

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = ASCIIToUTF16("Elvis");
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("Elvis", label, "", 1));
}

TEST_P(SuggestionMatchingTest, GetProfileSuggestions_FancyPhone) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetInfo(NAME_FULL, ASCIIToUTF16("Natty Bumppo"), "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1800PRAIRIE"));
  personal_data_.AddProfile(profile);

  const FormFieldData& field = form.fields[9];
  GetAutofillSuggestions(form, field);

  std::string value1;
  std::string value2;
  std::string value3;
  std::string label1;
  std::string label2;
  std::string label3;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "Natty Bumppo";
      label2 = MakeLabel(
          {"Charles Holley", "123 Apple St., unit 6", "buddy@gmail.com"});
      label3 = MakeLabel({"Elvis Presley", "3734 Elvis Presley Blvd., Apt. 10",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "Natty";
      label2 = MakeMobileLabel(
          {"Charles", "123 Apple St., unit 6", "buddy@gmail.com"});
      label3 = MakeMobileLabel(
          {"Elvis", "3734 Elvis Presley Blvd., Apt. 10", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "";
      label2 = "123 Apple St., unit 6";
      label3 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      value1 = "18007724743";  // 1800PRAIRIE
      value2 = "23456789012";
      value3 = "12345678901";
      label1 = "Natty Bumppo";
      label2 = "123 Apple St.";
      label3 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion(value1, label1, "", 1),
                   Suggestion(value2, label2, "", 2),
                   Suggestion(value3, label3, "", 3));
}

TEST_F(AutofillManagerTest, GetProfileSuggestions_ForPhonePrefixOrSuffix) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 5, "tel-extension"}};

  FormFieldData field;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text",
                              &field);
    field.max_length = test_field.max_length;
    field.autocomplete_attribute = std::string();
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  personal_data_.ClearProfiles();
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000104");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1800FLOWERS"));
  personal_data_.AddProfile(profile);

  const FormFieldData& phone_prefix = form.fields[2];
  GetAutofillSuggestions(form, phone_prefix);

  // Test that we sent the right prefix values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("356", "1800FLOWERS", "", 1));

  const FormFieldData& phone_suffix = form.fields[3];
  GetAutofillSuggestions(form, phone_suffix);

  // Test that we sent the right suffix values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("9377", "1800FLOWERS", "", 1));
}

// Test that we correctly fill an address form.
TEST_F(AutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data_.GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  EXPECT_EQ(2U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());
}

TEST_F(AutofillManagerTest, WillFillCreditCardNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData* number_field = nullptr;
  FormFieldData* name_field = nullptr;
  FormFieldData* month_field = nullptr;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    if (form.fields[i].name == ASCIIToUTF16("cardnumber"))
      number_field = &form.fields[i];
    else if (form.fields[i].name == ASCIIToUTF16("nameoncard"))
      name_field = &form.fields[i];
    else if (form.fields[i].name == ASCIIToUTF16("ccmonth"))
      month_field = &form.fields[i];
  }

  // Empty form - whole form is Autofilled.
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));

  // If the user has entered a value, it won't be overridden.
  number_field->value = ASCIIToUTF16("gibberish");
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));

  // But if that value is removed, it will be Autofilled.
  number_field->value.clear();
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));

  // When the number is already autofilled, we won't fill it.
  number_field->is_autofilled = true;
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));

  // If another field is filled, we would still fill other non-filled fields in
  // the section.
  number_field->is_autofilled = false;
  name_field->is_autofilled = true;
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
}

// Test that we correctly log FIELD_WAS_AUTOFILLED event in UserHappiness.
TEST_F(AutofillManagerTest, FillCreditCardForm_LogFieldWasAutofill) {
  // Set up our form data.
  FormData form;
  // Construct a form with a 4 fields: cardholder name, card number,
  // expiration date and cvc.
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  base::HistogramTester histogram_tester;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  // Cardholder name, card number, expiration data were autofilled but cvc was
  // not be autofilled.
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::FIELD_WAS_AUTOFILLED, 3);
}

// Test that we correctly fill a credit card form.
TEST_F(AutofillManagerTest, FillCreditCardForm_Simple) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that whitespace is stripped from the credit card number.
TEST_F(AutofillManagerTest, FillCreditCardForm_StripCardNumberWhitespace) {
  // Same as the SetUp(), but generate Elvis card with whitespace in credit
  // card number.  |credit_card| will be owned by the TestPersonalDataManager.
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000008");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000008";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that separator characters are stripped from the credit card number.
TEST_F(AutofillManagerTest, FillCreditCardForm_StripCardNumberSeparators) {
  // Same as the SetUp(), but generate Elvis card with separator characters in
  // credit card number.  |credit_card| will be owned by the
  // TestPersonalDataManager.
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234-5678-9012-3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that we correctly fill a credit card form with month input type.
// Test 1 of 4: Empty month, empty year
TEST_F(AutofillManagerTest, FillCreditCardForm_NoYearNoMonth) {
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 2 of 4: Non-empty month, empty year
TEST_F(AutofillManagerTest, FillCreditCardForm_NoYearMonth) {
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// Test 3 of 4: Empty month, non-empty year
TEST_F(AutofillManagerTest, FillCreditCardForm_YearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 4 of 4: Non-empty month, non-empty year
TEST_F(AutofillManagerTest, FillCreditCardForm_YearMonth) {
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "04");
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_F(AutofillManagerTest, FillCreditCardForm_SplitName) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[2]);
}

// Test that we correctly fill a combined address and credit card form.
TEST_F(AutofillManagerTest, FillAddressAndCreditCardForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  {
    SCOPED_TRACE("Address");
    FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                       MakeFrontendID(std::string(), guid),
                                       &response_page_id, &response_data);
    ExpectFilledAddressFormElvis(response_page_id, response_data,
                                 kDefaultPageID, true);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  {
    FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                       MakeFrontendID(guid2, std::string()),
                                       &response_page_id, &response_data);
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }
}

// Test that a field with an unrecognized autocomplete attribute is not filled.
TEST_F(AutofillManagerTest, FillAddressForm_UnrecognizedAttribute) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  address_form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.autocomplete_attribute = "";
  address_form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // The fist and middle names should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);

  // The last name should not be filled.
  ExpectFilledField("Last name", "lastname", "", "text",
                    response_data.fields[2]);
}

// Test that non credit card related fields with the autocomplete attribute set
// to off are not filled on desktop when the feature to autofill all addresses
// is disabled.
TEST_F(AutofillManagerTest, FillAddressForm_AutocompleteOffRespected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAlwaysFillAddresses);

  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // The fist name should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);

  // The middle name should not be filled on desktop.
  if (IsDesktopPlatform()) {
    ExpectFilledField("Middle name", "middle", "", "text",
                      response_data.fields[1]);
  } else {
    ExpectFilledField("Middle name", "middle", "Aaron", "text",
                      response_data.fields[1]);
  }

  // The last name should be filled.
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);

  // The address line 1 should not be filled on desktop.
  if (IsDesktopPlatform()) {
    ExpectFilledField("Address Line 1", "addr1", "", "text",
                      response_data.fields[3]);
  } else {
    ExpectFilledField("Address Line 1", "addr1", "3734 Elvis Presley Blvd.",
                      "text", response_data.fields[3]);
  }
}

// Test that non credit card related fields with the autocomplete attribute set
// to off are filled on all platforms when the feature to autofill all addresses
// is enabled (default).
TEST_F(AutofillManagerTest, FillAddressForm_AutocompleteOffNotRespected) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // All fields should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);
  ExpectFilledField("Address Line 1", "addr1", "3734 Elvis Presley Blvd.",
                    "text", response_data.fields[3]);
}

// Test that if a company is of a format of a birthyear and the relevant feature
// is enabled, we would not fill it.
TEST_F(AutofillManagerTest, FillAddressForm_CompanyBirthyear) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRejectCompanyBirthyear);

  // Set up our form data.
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Company", "company", "", "text", &field);
  address_form.fields.push_back(field);

  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000123";
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "1987", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile.set_guid(guid);
  personal_data_.AddProfile(profile);

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, *address_form.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // All the fields should be filled except the company.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);
  ExpectFilledField("Company", "company", "", "text", response_data.fields[3]);
}

// Test that a field with a value equal to it's placeholder attribute is filled.
TEST_F(AutofillManagerTest, FillAddressForm_PlaceholderEqualsValue) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set the same placeholder and value for each field.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.placeholder = ASCIIToUTF16("First Name");
  field.value = ASCIIToUTF16("First Name");
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.placeholder = ASCIIToUTF16("Middle Name");
  field.value = ASCIIToUTF16("Middle Name");
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.placeholder = ASCIIToUTF16("Last Name");
  field.value = ASCIIToUTF16("Last Name");
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // All the fields should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);
}

// Test that a credit card field with an unrecognized autocomplete attribute
// gets filled.
TEST_F(AutofillManagerTest, FillCreditCardForm_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  // Set a valid autocomplete attribute on the card name.
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute on the card number.
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute on the expiration month.
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // The credit card name and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                    response_data.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[1]);

  // The expiration month should be filled.
  ExpectFilledField("Expiration Date", "ccmonth", "04/2999", "text",
                    response_data.fields[2]);
}

// Test that credit card fields are filled even if they have the autocomplete
// attribute set to off.
TEST_F(AutofillManagerTest, FillCreditCardForm_AutocompleteOff) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);

  // Set the autocomplete=off on all fields.
  for (FormFieldData field : form.fields)
    field.should_autocomplete = false;

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // All fields should be filled.
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_F(AutofillManagerTest, FillCreditCardForm_ExpiredCard) {
  personal_data_.ClearCreditCards();
  CreditCard expired_card;
  test::SetCreditCardInfo(&expired_card, "Homer Simpson",
                          "4234567890654321",  // Visa
                          "05", "2000", "1");
  expired_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data_.AddCreditCard(expired_card);

  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  // Create a credit card form.
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  std::vector<const char*> kCreditCardTypes = {"Visa", "Mastercard", "AmEx",
                                               "discover"};
  test::CreateTestSelectField("Card Type", "cardtype", "", kCreditCardTypes,
                              kCreditCardTypes, 4, &field);
  field.autocomplete_attribute = "cc-type";
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "cc-number";
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "cc-exp-month";
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  field.autocomplete_attribute = "cc-exp-year";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // The credit card name, type and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Homer Simpson", "text",
                    response_data.fields[0]);
  ExpectFilledField("Card Type", "cardtype", "Visa", "select-one",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890654321", "text",
                    response_data.fields[2]);

  // The expiration month and year should not be filled.
  ExpectFilledField("Expiration Month", "ccmonth", "", "text",
                    response_data.fields[3]);
  ExpectFilledField("Expiration Year", "ccyear", "", "text",
                    response_data.fields[4]);
}

// Test that non-focusable field is ignored while inferring boundaries between
// sections, but not filled.
TEST_F(AutofillManagerTest, FillFormWithNonFocusableFields) {
  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email_", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // All the visible fields should be filled as all the fields belong to the
  // same logical section.
  ASSERT_EQ(6U, response_data.fields.size());
  ExpectFilledField("First Name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("", "lastname", "Presley", "text", response_data.fields[1]);
  ExpectFilledField("", "email", "theking@gmail.com", "text",
                    response_data.fields[2]);
  ExpectFilledField("Phone Number", "phonenumber", "12345678901", "tel",
                    response_data.fields[3]);
  ExpectFilledField("", "email_", "", "text", response_data.fields[4]);
  ExpectFilledField("Country", "country", "United States", "text",
                    response_data.fields[5]);
}

// Test that we correctly fill a form that has multiple logical sections, e.g.
// both a billing and a shipping address.
TEST_F(AutofillManagerTest, FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  const size_t kAddressFormSize = form.fields.size();
  test::CreateTestAddressFormData(&form);
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + ASCIIToUTF16("_");
  }
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the first section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(base::string16(), response_data.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    response_data.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(response_page_id, response_data,
                                 kDefaultPageID, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  ASSERT_LT(9U, kAddressFormSize);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID2, form, form.fields[kAddressFormSize + 9],
      MakeFrontendID(std::string(), guid2), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(response_data.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      EXPECT_EQ(base::string16(), response_data.fields[i].value);
    }

    // The second address section should be filled with Elvis's data.
    FormData secondSection = response_data;
    secondSection.fields.erase(secondSection.fields.begin(),
                               secondSection.fields.begin() + kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      // Restore the expected field names.
      base::string16 name = secondSection.fields[i].name;
      base::string16 original_name = name.substr(0, name.size() - 1);
      secondSection.fields[i].name = original_name;
    }
    ExpectFilledAddressFormElvis(response_page_id, secondSection, kPageID2,
                                 false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(AutofillManagerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("", "country", "", "text", &field);
  field.autocomplete_attribute = "section-billing country";
  form.fields.push_back(field);

  test::CreateTestFormField("", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "address", "", "text", &field);
  field.autocomplete_attribute = "section-billing address-line1";
  form.fields.push_back(field);

  test::CreateTestFormField("", "city", "", "text", &field);
  field.autocomplete_attribute = "section-billing locality";
  form.fields.push_back(field);

  test::CreateTestFormField("", "state", "", "text", &field);
  field.autocomplete_attribute = "section-billing region";
  form.fields.push_back(field);

  test::CreateTestFormField("", "zip", "", "text", &field);
  field.autocomplete_attribute = "section-billing postal-code";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccname", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccnumber", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-number";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccexp", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-exp";
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", &field);
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the unnamed section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[1],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(kDefaultPageID, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "Elvis", "text",
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "Presley", "text",
                      response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "theking@gmail.com", "text",
                      response_data.fields[10]);
  }

  // Fill the address portion of the billing section.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid2),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(kPageID2, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "US", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "3734 Elvis Presley Blvd.", "text",
                      response_data.fields[3]);
    ExpectFilledField("", "city", "Memphis", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "Tennessee", "text",
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "38116", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }

  // Fill the credit card portion of the billing section.
  const int kPageID3 = 3;
  const char guid3[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      MakeFrontendID(guid3, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(kPageID3, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "Elvis Presley", "text",
                      response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "4234567890123456", "text",
                      response_data.fields[8]);
    ExpectFilledField("", "ccexp", "04/2999", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(AutofillManagerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Confirm email", "email2", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // The second email address should be filled.
  EXPECT_EQ(ASCIIToUTF16("theking@gmail.com"),
            response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(AutofillManagerTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark the address fields as autofilled.
  for (auto iter = form.fields.begin(); iter != form.fields.end(); ++iter) {
    iter->is_autofilled = true;
  }

  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_page_id, response_data, kDefaultPageID, "Elvis",
                     "", "", "", "", "", "", "", "", "", "", "", "", "", "",
                     true, true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                     MakeFrontendID(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (auto iter = form.fields.begin(); iter != form.fields.end(); ++iter) {
    iter->is_autofilled = true;
  }

  const int kPageID3 = 3;
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      MakeFrontendID(guid2, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(response_page_id, response_data, kPageID3, "", "", "", "",
                     "", "", "", "", "", "", "", "", "", "", "2999", true, true,
                     false);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_F(AutofillManagerTest, FillPartlyAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark couple of the address fields as autofilled.
  form.fields[3].is_autofilled = true;
  form.fields[4].is_autofilled = true;
  form.fields[5].is_autofilled = true;
  form.fields[6].is_autofilled = true;
  form.fields[10].is_autofilled = true;

  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_page_id, response_data, kDefaultPageID, "Elvis",
                     "Aaron", "Presley", "", "", "", "", "38116",
                     "United States", "12345678901", "", "", "", "", "", true,
                     true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                     MakeFrontendID(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(AutofillManagerTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.name = ASCIIToUTF16("MyMaxlengthPhoneForm");
  form_with_us_number_max_length.url =
      GURL("http://myform.com/phone_form.html");
  form_with_us_number_max_length.action =
      GURL("http://myform.com/phone_submit.html");
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.name = ASCIIToUTF16("MyAutocompletetypePhoneForm");

  struct {
    const char* label;
    const char* name;
    size_t max_length;
    const char* autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 3, "tel-extension"}};

  FormFieldData field;
  const size_t default_max_length = field.max_length;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text",
                              &field);
    field.max_length = test_field.max_length;
    field.autocomplete_attribute = std::string();
    form_with_us_number_max_length.fields.push_back(field);

    field.max_length = default_max_length;
    field.autocomplete_attribute = test_field.autocomplete_attribute;
    form_with_autocompletetype.fields.push_back(field);
  }

  std::vector<FormData> forms;
  forms.push_back(form_with_us_number_max_length);
  forms.push_back(form_with_autocompletetype);
  FormsSeen(forms);

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data1);
  EXPECT_EQ(1, response_page_id);

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data1.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data1.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), response_data1.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), response_data1.fields[3].value);
  EXPECT_EQ(base::string16(), response_data1.fields[4].value);

  page_id = 2;
  response_page_id = 0;
  FormData response_data2;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data2);
  EXPECT_EQ(2, response_page_id);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data2.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data2.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), response_data2.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), response_data2.fields[3].value);
  EXPECT_EQ(base::string16(), response_data2.fields[4].value);

  // We should not be able to fill international numbers correctly in a form
  // containing fields with US max_length. However, the field should fill with
  // the number of digits equal to the max length specified, starting from the
  // right.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("GB"));
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("447700954321"));
  page_id = 3;
  response_page_id = 0;
  FormData response_data3;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data3);
  EXPECT_EQ(3, response_page_id);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(ASCIIToUTF16("4"), response_data3.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("700"), response_data3.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("321"), response_data3.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4321"), response_data3.fields[3].value);
  EXPECT_EQ(base::string16(), response_data3.fields[4].value);

  page_id = 4;
  response_page_id = 0;
  FormData response_data4;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data4);
  EXPECT_EQ(4, response_page_id);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), response_data4.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), response_data4.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[3].value);
  EXPECT_EQ(base::string16(), response_data4.fields[4].value);
}

TEST_F(AutofillManagerTest, FillFirstPhoneNumber_ComponentizedNumbers) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.url =
      GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      ASCIIToUTF16("multiple_componentized_number_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("country code", "country_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping country code", "shipping_country_code",
                            "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping area code", "shipping_area_code", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_componentized_phone_fields);

  FormData form_data_copy(form_with_multiple_componentized_phone_fields);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("1"), response_data.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("5554567"), response_data.fields[3].value);
  EXPECT_EQ(base::string16(), response_data.fields[4].value);
  EXPECT_EQ(base::string16(), response_data.fields[5].value);
  EXPECT_EQ(base::string16(), response_data.fields[6].value);
  EXPECT_EQ(base::string16(), response_data.fields[7].value);
}

TEST_F(AutofillManagerTest, FillFirstPhoneNumber_WholeNumbers) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name =
      ASCIIToUTF16("multiple_whole_number_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_whole_number_fields);

  FormData form_data_copy(form_with_multiple_whole_number_fields);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[1].value);
  EXPECT_EQ(base::string16(), response_data.fields[2].value);
  EXPECT_EQ(base::string16(), response_data.fields[3].value);
}

TEST_F(AutofillManagerTest, FillFirstPhoneNumber_FillPartsOnceOnly) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.url =
      GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      ASCIIToUTF16("multiple_componentized_number_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("country code", "country_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  field.autocomplete_attribute = "tel-national";
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  field.autocomplete_attribute = "";
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping country code", "shipping_country_code",
                            "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping area code", "shipping_area_code", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_componentized_phone_fields);

  FormData form_data_copy(form_with_multiple_componentized_phone_fields);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify only the first complete set of phone number fields are filled,
  // and phone components are not filled more than once.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("1"), response_data.fields[1].value);
  EXPECT_EQ(base::string16(), response_data.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[3].value);
  EXPECT_EQ(base::string16(), response_data.fields[4].value);
  EXPECT_EQ(base::string16(), response_data.fields[5].value);
  EXPECT_EQ(base::string16(), response_data.fields[6].value);
  EXPECT_EQ(base::string16(), response_data.fields[7].value);
}

// Verify when extension is misclassified, and there is a complete
// phone field, we do not fill anything to extension field.
TEST_F(AutofillManagerTest,
       FillFirstPhoneNumber_NotFillMisclassifiedExtention) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_misclassified_extension;
  form_with_misclassified_extension.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_misclassified_extension.name =
      ASCIIToUTF16("complete_phone_form_with_extension");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  field.autocomplete_attribute = "name";
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("address", "address", "", "text", &field);
  field.autocomplete_attribute = "addresses";
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  field.autocomplete_attribute = "tel-area-code";
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  field.autocomplete_attribute = "tel-local";
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  field.autocomplete_attribute = "tel-local";
  form_with_misclassified_extension.fields.push_back(field);

  std::vector<FormData> forms;
  forms.push_back(form_with_misclassified_extension);

  FormData form_data_copy(form_with_misclassified_extension);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_misclassified_extension,
      *form_with_misclassified_extension.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify the misclassified extension field is not filled.
  ASSERT_EQ(5U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(base::string16(), response_data.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("5554567"), response_data.fields[3].value);
  EXPECT_EQ(base::string16(), response_data.fields[4].value);
}

// Verify when no complete number can be found, we do best-effort filling.
TEST_F(AutofillManagerTest, FillFirstPhoneNumber_BestEfforFilling) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_no_complete_number;
  form_with_no_complete_number.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_no_complete_number.name = ASCIIToUTF16("no_complete_phone_form");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  field.autocomplete_attribute = "name";
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("address", "address", "", "text", &field);
  field.autocomplete_attribute = "address";  // not standard, but covered.
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  field.autocomplete_attribute = "tel-area-code";
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  field.autocomplete_attribute = "extension";
  form_with_no_complete_number.fields.push_back(field);

  std::vector<FormData> forms;
  forms.push_back(form_with_no_complete_number);

  FormData form_data_copy(form_with_no_complete_number);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_no_complete_number,
      *form_with_no_complete_number.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify when there is no complete phone number fields, we do best effort
  // filling.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St., unit 6"),
            response_data.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data.fields[2].value);
  EXPECT_EQ(base::string16(), response_data.fields[3].value);
}

// When the focus is on second phone field explicitly, we will fill the
// entire form, both first phone field and second phone field included.
TEST_F(AutofillManagerTest, FillFirstPhoneNumber_FocusOnSecondPhoneNumber) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name =
      ASCIIToUTF16("multiple_whole_number_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_whole_number_fields);

  FormData form_data_copy(form_with_multiple_whole_number_fields);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  auto it = form_with_multiple_whole_number_fields.fields.begin();
  // Move it to point to "shipping number".
  std::advance(it, 3);
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_whole_number_fields, *it,
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify when the second phone number field is being focused, we fill
  // that field *AND* the first phone number field.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[1].value);
  EXPECT_EQ(base::string16(), response_data.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[3].value);
}

TEST_F(AutofillManagerTest, FillFirstPhoneNumber_HiddenFieldShouldNotCount) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name =
      ASCIIToUTF16("multiple_whole_number_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  field.is_focusable = false;
  form_with_multiple_whole_number_fields.fields.push_back(field);
  field.is_focusable = true;
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_whole_number_fields);

  FormData form_data_copy(form_with_multiple_whole_number_fields);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify hidden/non-focusable phone field is set to only_fill_when_focused.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(base::string16(), response_data.fields[1].value);
  EXPECT_EQ(base::string16(), response_data.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[3].value);
}

// The hidden and the presentational fields should be filled, only if their
// control type is 'select-one'. This exception is made to support synthetic
// fields.
TEST_F(AutofillManagerTest, FormWithHiddenOrPresentationalSelects) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  {
    const std::vector<const char*> values{"CA", "US", "BR"};
    const std::vector<const char*> contents{"Canada", "United States",
                                            "Banana Republic"};
    test::CreateTestSelectField("Country", "country", "", values, contents,
                                values.size(), &field);
    field.is_focusable = false;
    form.fields.push_back(field);
  }
  {
    const std::vector<const char*> values{"NY", "CA", "TN"};
    const std::vector<const char*> contents{"New York", "California",
                                            "Tennessee"};
    test::CreateTestSelectField("State", "state", "", values, contents,
                                values.size(), &field);
    field.role = FormFieldData::RoleAttribute::kPresentation;
    form.fields.push_back(field);
  }

  test::CreateTestFormField("City", "city", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Street Address", "address", "", "text", &field);
  field.role = FormFieldData::RoleAttribute::kPresentation;
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  base::HistogramTester histogram_tester;

  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  histogram_tester.ExpectTotalCount(
      "Autofill.HiddenOrPresentationalSelectFieldsFilled", 2);

  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Country", "country", "US", "select-one",
                    response_data.fields[2]);
  ExpectFilledField("State", "state", "TN", "select-one",
                    response_data.fields[3]);
  ExpectFilledField("City", "city", "", "text", response_data.fields[4]);
  ExpectFilledField("Street Address", "address", "", "text",
                    response_data.fields[5]);
}

TEST_F(AutofillManagerTest,
       FillFirstPhoneNumber_MultipleSectionFilledCorrectly) {
  AutofillProfile* work_profile =
      personal_data_.GetProfileWithGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());

  FormData form_with_multiple_sections;
  form_with_multiple_sections.url = GURL("http://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_sections.name = ASCIIToUTF16("multiple_section_fields");
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("other number", "other_phone_number", "", "text",
                            &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Shipping Address", "shipping_address", "", "text",
                            &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("other shipping number",
                            "other_shipping_phone_number", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);

  std::vector<FormData> forms;
  forms.push_back(form_with_multiple_sections);

  FormData form_data_copy(form_with_multiple_sections);
  std::vector<FormData> forms_copy;
  forms_copy.push_back(form_data_copy);

  FormsSeen(forms);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data;
  // Fill first sections.
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_multiple_sections,
      *form_with_multiple_sections.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify first section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St."), response_data.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[2].value);
  EXPECT_EQ(base::string16(), response_data.fields[3].value);
  EXPECT_EQ(base::string16(), response_data.fields[4].value);
  EXPECT_EQ(base::string16(), response_data.fields[5].value);
  EXPECT_EQ(base::string16(), response_data.fields[6].value);
  EXPECT_EQ(base::string16(), response_data.fields[7].value);
  EXPECT_EQ(base::string16(), response_data.fields[8].value);

  // Fill second section.
  auto it = form_with_multiple_sections.fields.begin();
  std::advance(it, 6);  // Pointing to second section.

  FillAutofillFormDataAndSaveResults(page_id, form_with_multiple_sections, *it,
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify second section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(base::string16(), response_data.fields[0].value);
  EXPECT_EQ(base::string16(), response_data.fields[1].value);
  EXPECT_EQ(base::string16(), response_data.fields[2].value);
  EXPECT_EQ(base::string16(), response_data.fields[3].value);
  EXPECT_EQ(base::string16(), response_data.fields[4].value);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley"),
            response_data.fields[5].value);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St."), response_data.fields[6].value);
  EXPECT_EQ(ASCIIToUTF16("6505554567"), response_data.fields[7].value);
  EXPECT_EQ(base::string16(), response_data.fields[8].value);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(AutofillManagerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormFieldData field;
  test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(AutofillManagerTest, FormChangesAddField) {
  // The offset of the phone field in the address form.
  const int kPhoneFieldOffset = 9;

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Remove the phone field -- we'll add it back later.
  auto pos = form.fields.begin() + kPhoneFieldOffset;
  FormFieldData field = *pos;
  pos = form.fields.erase(pos);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we restore the field before filling.
  form.fields.insert(pos, field);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we can still fill a form when the visibility of some fields
// changes.
TEST_F(AutofillManagerTest, FormChangesVisibilityOfFields) {
  // Set up our form data.
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;

  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form.name = ASCIIToUTF16("multiple_groups_fields");
  test::CreateTestFormField("First Name", "first_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "last_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Postal Code", "postal_code", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form with the first profile. The hidden fields will not get
  // filled.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  ASSERT_EQ(5U, response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.", "text",
                    response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "", "text",
                    response_data.fields[3]);
  ExpectFilledField("Country", "country", "", "text", response_data.fields[4]);

  // Two other fields will show up. Select the second profile. The fields that
  // were already filled, would be left unchanged, and the rest would be filled
  // with the second profile. (Two different profiles are selected, to make sure
  // the right fields are getting filled.)
  response_data.fields[3].is_focusable = true;
  response_data.fields[4].is_focusable = true;
  FormData later_response_data;
  const char guid2[] = "00000000-0000-0000-0000-000000000002";
  FillAutofillFormDataAndSaveResults(kDefaultPageID, response_data,
                                     response_data.fields[4],
                                     MakeFrontendID(std::string(), guid2),
                                     &response_page_id, &later_response_data);
  ASSERT_EQ(5U, later_response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis", "text",
                    later_response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley", "text",
                    later_response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.", "text",
                    later_response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "79401", "text",
                    later_response_data.fields[3]);
  ExpectFilledField("Country", "country", "United States", "text",
                    later_response_data.fields[4]);
}

// Test that we are able to save form data when forms are submitted.
TEST_F(AutofillManagerTest, FormSubmitted) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Test that we are saving form data when the FormSubmitted event is sent.
TEST_F(AutofillManagerTest, FormSubmittedSaveData) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  autofill_manager_->OnFormSubmitted(response_data, false,
                                     SubmissionSource::FORM_SUBMISSION);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Test that when Autocomplete is enabled and Autofill is disabled, form
// submissions are still received by AutocompleteHistoryManager.
TEST_F(AutofillManagerTest, FormSubmittedAutocompleteEnabled) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnWillSubmitForm(_, true));
  FormSubmitted(form);
}

// Test that the value patterns metric is reported.
TEST_F(AutofillManagerTest, ValuePatternsMetric) {
  struct ValuePatternTestCase {
    const char* value;
    autofill::ValuePatternsMetric pattern;
  } kTestCases[] = {
      {"user@okaxis", autofill::ValuePatternsMetric::kUpiVpa},
      {"IT60X0542811101000000123456", autofill::ValuePatternsMetric::kIban}};
  for (const ValuePatternTestCase test_case : kTestCases) {
    // Set up our form data.
    FormData form;
    FormFieldData field;
    test::CreateTestFormField("Some label", "my-field", test_case.value, "text",
                              &field);
    field.is_focusable = true;  // The metric skips hidden fields.
    form.name = ASCIIToUTF16("my-form");
    form.url = GURL("http://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");
    form.fields.push_back(field);
    std::vector<FormData> forms(1, form);
    FormsSeen(forms);

    base::HistogramTester histogram_tester;
    FormSubmitted(form);
    histogram_tester.ExpectUniqueSample("Autofill.SubmittedValuePatterns",
                                        test_case.pattern, 1);
  }
}

// Test that when Autofill is disabled, Autocomplete suggestions are still
// queried.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_SomeWhenAutofillDisabled) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];

  // Expect Autocomplete manager to be called for suggestions.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field should not autocomplete,
// autocomplete is not queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_AutofillDisabledAndFieldShouldNotAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[0];
  field.should_autocomplete = false;

  // Autocomplete manager is not called for suggestions.

  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that we do not query for Autocomplete suggestions when there are
// Autofill suggestions available.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_NoneWhenAutofillPresent) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];

  // AutocompleteManager is not called for suggestions.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we query for Autocomplete suggestions when there are no Autofill
// suggestions available.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_SomeWhenAutofillEmpty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete manager is called for suggestions because Autofill is empty.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card name
// field,
// autocomplete is queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_CreditCardNameFieldShouldAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The first field is "Name on card", which should autocomplete.
  FormFieldData field = form.fields[0];
  field.should_autocomplete = true;

  // Autocomplete manager is not called for suggestions.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card number
// field, autocomplete is not queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_CreditCardNumberShouldNotAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The second field is "Card Number", which should not autocomplete.
  FormFieldData field = form.fields[1];
  field.should_autocomplete = true;

  // Autocomplete manager is not called for suggestions.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that we do not query for Autocomplete suggestions when there are no
// Autofill suggestions available, and that the field should not autocomplete.
TEST_F(
    AutofillManagerTest,
    AutocompleteSuggestions_NoneWhenAutofillEmptyFieldShouldNotAutocomplete) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  field.should_autocomplete = false;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete manager is not called for suggestions.
  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
}

TEST_F(AutofillManagerTest, AutocompleteOffRespectedForAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, &personal_data_,
                              autocomplete_history_manager_.get()));
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  EXPECT_CALL(*(autocomplete_history_manager_.get()),
              OnGetAutocompleteSuggestions)
      .Times(0);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData* field = &form.fields[0];
  field->should_autocomplete = false;
  GetAutofillSuggestions(form, *field);
}

TEST_F(AutofillManagerTest, DestructorCancelsAutocompleteQueries) {
  EXPECT_CALL(*(autocomplete_history_manager_.get()), CancelPendingQueries)
      .Times(1);
  autofill_manager_.reset();
}

// Make sure that we don't error out when AutocompleteHistoryManager was
// destroyed before AutofillManager.
TEST_F(AutofillManagerTest, Destructor_DeletedAutocomplete_Works) {
  // The assertion here is that no exceptions will be thrown.
  autocomplete_history_manager_.reset();
  autofill_manager_.reset();
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form and apply type predictions.
TEST_F(AutofillManagerTest, OnLoadedServerPredictions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(
      std::unique_ptr<TestFormStructure>(form_structure));

  // Similarly, a second form.
  FormData form2;
  form2.name = ASCIIToUTF16("MyForm");
  form2.url = GURL("http://myform.com/form.html");
  form2.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);

  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form2.fields.push_back(field);

  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form2.fields.push_back(field);

  TestFormStructure* form_structure2 = new TestFormStructure(form2);
  form_structure2->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(
      std::unique_ptr<TestFormStructure>(form_structure2));

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(3);
  for (int i = 0; i < 7; ++i) {
    response.add_field()->set_overall_type_prediction(0);
  }
  response.add_field()->set_overall_type_prediction(3);
  response.add_field()->set_overall_type_prediction(2);
  response.add_field()->set_overall_type_prediction(61);
  response.add_field()->set_overall_type_prediction(5);
  response.add_field()->set_overall_type_prediction(4);
  response.add_field()->set_overall_type_prediction(35);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure->FormSignatureAsStr());
  signatures.push_back(form_structure2->FormSignatureAsStr());

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);
  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);
  // We expect the server type to have been applied to the first field of the
  // first form.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->Type().GetStorableType());

  // We expect the server types to have been applied to the second form.
  EXPECT_EQ(NAME_LAST, form_structure2->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE, form_structure2->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure2->field(2)->Type().GetStorableType());
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form from the API and apply type predictions.
// What we test here:
//  * The API response parser is used.
//  * The query can be processed with a response from the API.
TEST_F(AutofillManagerTest, OnLoadedServerPredictionsFromApi) {
  // Set features.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled
      // We want to query the API rather than the legacy server.
      {features::kAutofillUseApi},
      // Disabled
      {});

  // First form on the page.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField(/*label=*/"City", /*name=*/"city",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"State", /*name=*/"state",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Postal Code", /*name=*/"zipcode",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  // Simulate having seen this form on page load.
  // |form_structure_instance| will be owned by |autofill_manager_|.
  auto form_structure_instance = std::make_unique<TestFormStructure>(form);
  // This pointer is valid as long as autofill manager lives.
  TestFormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(std::move(form_structure_instance));

  // Second form on the page.
  FormData form2;
  form2.name = ASCIIToUTF16("MyForm2");
  form2.url = GURL("http://myform.com/form.html");
  form2.action = GURL("http://myform.com/submit.html");
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form2.fields.push_back(field);
  auto form_structure_instance2 = std::make_unique<TestFormStructure>(form2);
  // This pointer is valid as long as autofill manager lives.
  TestFormStructure* form_structure2 = form_structure_instance2.get();
  form_structure2->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(std::move(form_structure_instance2));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form 1.
  form_suggestion = response.add_form_suggestions();
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      ADDRESS_HOME_CITY);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      ADDRESS_HOME_STATE);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      ADDRESS_HOME_ZIP);
  // Set suggestions for form 2.
  form_suggestion = response.add_form_suggestions();
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      NAME_LAST);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      NAME_MIDDLE);
  form_suggestion->add_field_suggestions()->set_primary_type_prediction(
      ADDRESS_HOME_ZIP);
  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);

  std::vector<std::string> signatures = {form_structure->FormSignatureAsStr(),
                                         form_structure2->FormSignatureAsStr()};

  // Run method under test.
  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(encoded_response_string,
                                               signatures);

  // Verify whether the relevant histograms were updated.
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // We expect the server suggestions to have been applied to the first field of
  // the first form.
  EXPECT_EQ(ADDRESS_HOME_CITY,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure->field(2)->Type().GetStorableType());
  // We expect the server suggestions to have been applied to the second form as
  // well.
  EXPECT_EQ(NAME_LAST, form_structure2->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE, form_structure2->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure2->field(2)->Type().GetStorableType());
}

// Test that OnLoadedServerPredictions does not call ParseQueryResponse if the
// AutofillManager has been reset between the time the query was sent and the
// response received.
TEST_F(AutofillManagerTest, OnLoadedServerPredictions_ResetManager) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(
      std::unique_ptr<TestFormStructure>(form_structure));

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(3);
  for (int i = 0; i < 7; ++i) {
    response.add_field()->set_overall_type_prediction(0);
  }
  response.add_field()->set_overall_type_prediction(3);
  response.add_field()->set_overall_type_prediction(2);
  response.add_field()->set_overall_type_prediction(61);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure->FormSignatureAsStr());

  // Reset the manager (such as during a navigation).
  autofill_manager_->Reset();

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);

  // Verify that FormStructure::ParseQueryResponse was NOT called.
  histogram_tester.ExpectTotalCount("Autofill.ServerQueryResponse", 0);
}

// Test that when server predictions disagree with the heuristic ones, the
// overall types and sections would be set based on the server one.
TEST_F(AutofillManagerTest, DetermineHeuristicsWithOverallPrediction) {
  // Set up our form data.
  FormData form;
  form.url = GURL("https://www.myform.com");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Expiration Year", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Expiration Month", "exp_month", "", "text",
                            &field);
  form.fields.push_back(field);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes();
  autofill_manager_->AddSeenFormStructure(
      std::unique_ptr<TestFormStructure>(form_structure));

  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_FIRST);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NAME_LAST);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_NUMBER);
  response.add_field()->set_overall_type_prediction(CREDIT_CARD_EXP_MONTH);
  response.add_field()->set_overall_type_prediction(
      CREDIT_CARD_EXP_4_DIGIT_YEAR);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure->FormSignatureAsStr());

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);
  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // Since the card holder name appears as the first name + last name (rather
  // than the full name), and since they appears as the first fields of the
  // section, the heuristics detect them as the address first/last name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // We expect to see the server type as the overall type.
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NAME_LAST,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // Although the heuristic types of the first two fields belongs to the address
  // section, the final fields' section should be based on the overall
  // prediction, therefore they should be grouped in one section.
  const auto section = form_structure->field(0)->section;
  EXPECT_EQ(section, form_structure->field(1)->section);
  EXPECT_EQ(section, form_structure->field(2)->section);
  EXPECT_EQ(section, form_structure->field(3)->section);
  EXPECT_EQ(section, form_structure->field(4)->section);
}

// Test that we are able to save form data when forms are submitted and we only
// have server data for the field types.
TEST_F(AutofillManagerTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen(std::vector<FormData>(1, form));

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes();

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->heuristic_type());
  }
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenFormStructure(
      std::unique_ptr<TestFormStructure>(form_structure));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Test that we are able to save form data after the possible types have been
// determined. We do two submissions and verify that only at the second
// submission are the possible types able to be inferred.
TEST_F(AutofillManagerTest, FormSubmittedPossibleTypesTwoSubmissions) {
  // Set up our form data.
  FormData form;
  std::vector<ServerFieldTypeSet> expected_types;
  test::CreateTestAddressFormData(&form, &expected_types);
  FormsSeen(std::vector<FormData>(1, form));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  personal_data_.ClearProfiles();
  // The default credit card is a Elvis card. It must be removed because name
  // fields would be detected. However at least one profile or card is needed to
  // start the upload process, which is why this other card is created.
  personal_data_.ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data_.AddCreditCard(credit_card);
  ASSERT_EQ(0u, personal_data_.GetProfiles().size());

  // Simulate form submission. The first submission should not count the data
  // towards possible types. Therefore we expect all UNKNOWN_TYPE entries.
  ServerFieldTypeSet type_set;
  type_set.insert(UNKNOWN_TYPE);
  std::vector<ServerFieldTypeSet> unknown_types(expected_types.size(),
                                                type_set);
  autofill_manager_->SetExpectedSubmittedFieldTypes(unknown_types);
  FormSubmitted(response_data);
  ASSERT_EQ(1u, personal_data_.GetProfiles().size());

  // The second submission should now have data by which to infer types.
  autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  FormSubmitted(response_data);
  ASSERT_EQ(1u, personal_data_.GetProfiles().size());
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_F(AutofillManagerTest, FormSubmittedWithDifferentFields) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Cache the expected form signature.
  std::string signature = FormStructure(form).FormSignatureAsStr();

  // Change the structure of the form prior to submission.
  // Websites would typically invoke JavaScript either on page load or on form
  // submit to achieve this.
  form.fields.pop_back();
  FormFieldData field = form.fields[3];
  form.fields[3] = form.fields[7];
  form.fields[7] = field;

  // Simulate form submission.
  FormSubmitted(form);
  EXPECT_EQ(signature, autofill_manager_->GetSubmittedFormSignature());
}

// Test that we do not save form data when submitted fields contain default
// values.
TEST_F(AutofillManagerTest, FormSubmittedWithDefaultValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[3].value = ASCIIToUTF16("Enter your address");

  // Convert the state field to a <select> popup, to make sure that we only
  // reject default values for text fields.
  ASSERT_TRUE(form.fields[6].name == ASCIIToUTF16("state"));
  form.fields[6].form_control_type = "select-one";
  form.fields[6].value = ASCIIToUTF16("Tennessee");

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[3],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // Simulate form submission.  We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());

  // Set the address field's value back to the default value.
  response_data.fields[3].value = ASCIIToUTF16("Enter your address");

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

struct ProfileMatchingTypesTestCase {
  const char* input_value;     // The value to input in the field.
  ServerFieldType field_type;  // The expected field type to be determined.
};

class ProfileMatchingTypesTest
    : public AutofillManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<ProfileMatchingTypesTestCase,
                     int,        // AutofillDataModel::ValidityState
                     bool>> {};  // AutofillDataModel::ValidationSource

const ProfileMatchingTypesTestCase kProfileMatchingTypesTestCases[] = {
    // Profile fields matches.
    {"Elvis", NAME_FIRST},
    {"Aaron", NAME_MIDDLE},
    {"A", NAME_MIDDLE_INITIAL},
    {"Presley", NAME_LAST},
    {"Elvis Aaron Presley", NAME_FULL},
    {"theking@gmail.com", EMAIL_ADDRESS},
    {"RCA", COMPANY_NAME},
    {"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1},
    {"Apt. 10", ADDRESS_HOME_LINE2},
    {"Memphis", ADDRESS_HOME_CITY},
    {"Tennessee", ADDRESS_HOME_STATE},
    {"38116", ADDRESS_HOME_ZIP},
    {"USA", ADDRESS_HOME_COUNTRY},
    {"United States", ADDRESS_HOME_COUNTRY},
    {"12345678901", PHONE_HOME_WHOLE_NUMBER},
    {"+1 (234) 567-8901", PHONE_HOME_WHOLE_NUMBER},
    {"(234)567-8901", PHONE_HOME_CITY_AND_NUMBER},
    {"2345678901", PHONE_HOME_CITY_AND_NUMBER},
    {"1", PHONE_HOME_COUNTRY_CODE},
    {"234", PHONE_HOME_CITY_CODE},
    {"5678901", PHONE_HOME_NUMBER},
    {"567", PHONE_HOME_NUMBER},
    {"8901", PHONE_HOME_NUMBER},

    // Test a European profile.
    {"Paris", ADDRESS_HOME_CITY},
    {"Île de France", ADDRESS_HOME_STATE},    // Exact match
    {"Ile de France", ADDRESS_HOME_STATE},    // Missing accent.
    {"-Ile-de-France-", ADDRESS_HOME_STATE},  // Extra punctuation.
    {"île dÉ FrÃÑÇË", ADDRESS_HOME_STATE},    // Other accents & case mismatch.
    {"75008", ADDRESS_HOME_ZIP},
    {"FR", ADDRESS_HOME_COUNTRY},
    {"France", ADDRESS_HOME_COUNTRY},
    {"33249197070", PHONE_HOME_WHOLE_NUMBER},
    {"+33 2 49 19 70 70", PHONE_HOME_WHOLE_NUMBER},
    {"02 49 19 70 70", PHONE_HOME_CITY_AND_NUMBER},
    {"0249197070", PHONE_HOME_CITY_AND_NUMBER},
    {"33", PHONE_HOME_COUNTRY_CODE},
    {"2", PHONE_HOME_CITY_CODE},

    // Credit card fields matches.
    {"John Doe", CREDIT_CARD_NAME_FULL},
    {"John", CREDIT_CARD_NAME_FIRST},
    {"Doe", CREDIT_CARD_NAME_LAST},
    {"4234-5678-9012-3456", CREDIT_CARD_NUMBER},
    {"04", CREDIT_CARD_EXP_MONTH},
    {"April", CREDIT_CARD_EXP_MONTH},
    {"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR},
    {"99", CREDIT_CARD_EXP_2_DIGIT_YEAR},
    {"04/2999", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

    // Make sure whitespace and invalid characters are handled properly.
    {"", EMPTY_TYPE},
    {" ", EMPTY_TYPE},
    {"***", UNKNOWN_TYPE},
    {" Elvis", NAME_FIRST},
    {"Elvis ", NAME_FIRST},

    // Make sure fields that differ by case match.
    {"elvis ", NAME_FIRST},
    {"UnItEd StAtEs", ADDRESS_HOME_COUNTRY},

    // Make sure fields that differ by punctuation match.
    {"3734 Elvis Presley Blvd", ADDRESS_HOME_LINE1},
    {"3734, Elvis    Presley Blvd.", ADDRESS_HOME_LINE1},

    // Make sure that a state's full name and abbreviation match.
    {"TN", ADDRESS_HOME_STATE},     // Saved as "Tennessee" in profile.
    {"Texas", ADDRESS_HOME_STATE},  // Saved as "TX" in profile.

    // Special phone number case. A profile with no country code should
    // only match PHONE_HOME_CITY_AND_NUMBER.
    {"5142821292", PHONE_HOME_CITY_AND_NUMBER},

    // Make sure unsupported variants do not match.
    {"Elvis Aaron", UNKNOWN_TYPE},
    {"Mr. Presley", UNKNOWN_TYPE},
    {"3734 Elvis Presley", UNKNOWN_TYPE},
    {"38116-1023", UNKNOWN_TYPE},
    {"5", UNKNOWN_TYPE},
    {"56", UNKNOWN_TYPE},
    {"901", UNKNOWN_TYPE},
};

// Tests that DeterminePossibleFieldTypesForUpload finds accurate possible
// types and validities.
TEST_P(ProfileMatchingTypesTest, DeterminePossibleFieldTypesForUpload) {
  // Unpack the test paramters
  const auto& test_case = std::get<0>(GetParam());
  auto validity_state =
      static_cast<AutofillDataModel::ValidityState>(std::get<1>(GetParam()));
  const auto& validation_source =
      static_cast<AutofillDataModel::ValidationSource>(std::get<2>(GetParam()));

  SCOPED_TRACE(base::StringPrintf(
      "Test: input_value='%s', field_type=%s, validity_state=%d, "
      "validation_source=%d ",
      test_case.input_value,
      AutofillType(test_case.field_type).ToString().c_str(), validity_state,
      validation_source));

  ASSERT_LE(AutofillDataModel::UNVALIDATED, validity_state);
  ASSERT_LE(validity_state, AutofillDataModel::UNSUPPORTED);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;
  profiles.resize(3);
  test::SetProfileInfo(&profiles[0], "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "+1 (234) 567-8901");
  profiles[0].set_guid("00000000-0000-0000-0000-000000000001");

  test::SetProfileInfo(&profiles[1], "Charles", "", "Holley", "buddy@gmail.com",
                       "Decca", "123 Apple St.", "unit 6", "Lubbock", "TX",
                       "79401", "US", "5142821292");
  profiles[1].set_guid("00000000-0000-0000-0000-000000000002");

  test::SetProfileInfo(&profiles[2], "Charles", "", "Baudelaire",
                       "lesfleursdumal@gmail.com", "", "108 Rue Saint-Lazare",
                       "Apt. 11", "Paris", "Île de France", "75008", "FR",
                       "+33 2 49 19 70 70");
  profiles[2].set_guid("00000000-0000-0000-0000-000000000001");

  // Set the validity state for the matching field type.
  if (GroupTypeOfServerFieldType(test_case.field_type) != CREDIT_CARD) {
    for (auto& profile : profiles) {
      if (test_case.field_type == UNKNOWN_TYPE) {
        // An UNKNOWN type is always UNVALIDATED
        validity_state = AutofillDataModel::UNVALIDATED;
      } else if (profile.IsAnInvalidPhoneNumber(test_case.field_type)) {
        // A phone field is a compound field, and an invalid part makes
        // the phone number invalid.
        validity_state = AutofillDataModel::INVALID;
      }
      profile.SetValidityState(test_case.field_type, validity_state,
                               validation_source);
    }
  }

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("", "1", "", "text", &field);
  field.value = UTF8ToUTF16(test_case.input_value);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  base::HistogramTester histogram_tester;
  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  ASSERT_EQ(1U, form_structure.field_count());

  ServerFieldTypeSet possible_types = form_structure.field(0)->possible_types();
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_EQ(*possible_types.begin(), test_case.field_type);

  // We don't add validity states for credit card fields.
  if (GroupTypeOfServerFieldType(test_case.field_type) != CREDIT_CARD) {
    ServerFieldTypeValidityStatesMap possible_types_validities =
        form_structure.field(0)->possible_types_validities();
    ASSERT_EQ(1U, possible_types_validities.size());
    EXPECT_NE(possible_types_validities.end(),
              possible_types_validities.find(test_case.field_type));
    EXPECT_EQ(possible_types_validities[test_case.field_type][0],
              (validation_source == AutofillDataModel::SERVER)
                  ? validity_state
                  : AutofillDataModel::UNVALIDATED);
  }
}

// Tests that DeterminePossibleFieldTypesForUpload is called when a form is
// submitted.
TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesForUpload_IsTriggered) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  std::vector<base::string16> expected_values;

  // These fields should all match.
  FormFieldData field;
  ServerFieldTypeSet types;

  test::CreateTestFormField("", "1", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("Elvis"));
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "2", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("Aaron"));
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "3", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("A"));
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // Make sure the form is in the cache so that it is processed for Autofill
  // upload.
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Once the form is cached, fill the values.
  EXPECT_EQ(form.fields.size(), expected_values.size());
  for (size_t i = 0; i < expected_values.size(); i++) {
    form.fields[i].value = expected_values[i];
  }

  autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  FormSubmitted(form);
}

// Test that the possible field types with multiple validities are determined
// correctly.
TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesWithMultipleValidities) {
  // Set up the user's profiles.
  std::vector<AutofillProfile> profiles;
  {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "", "Memphis", "Tennessee", "38116", "US",
                         "(234) 567-8901");
    profile.set_guid("00000000-0000-0000-0000-000000000001");
    profile.SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::VALID,
                             AutofillDataModel::SERVER);
    profiles.push_back(profile);
  }
  {
    AutofillProfile profile;
    test::SetProfileInfo(&profile, "Alice", "", "Munro", "munro@gmail.com", "",
                         "1331 W Georgia", "", "Vancouver", "Tennessee",
                         "V4D 4S4", "CA", "(778) 567-8901");
    profile.set_guid("00000000-0000-0000-0000-000000000002");
    profile.SetValidityState(ADDRESS_HOME_STATE, AutofillDataModel::INVALID,
                             AutofillDataModel::SERVER);
    profiles.push_back(profile);
  }

  // Set up the test cases:
  typedef struct {
    std::string input_value;
    ServerFieldType field_type;
    std::vector<AutofillDataModel::ValidityState> expected_validity_states;
  } TestFieldData;

  std::vector<TestFieldData> test_cases[3];
  // Tennessee appears in both of the user's profile as ADDRESS_HOME_STATE. In
  // the first one, it's VALID, and for the other, it's INVALID. Therefore, the
  // possible_field_types would only include the type ADDRESS_HOME_STATE, and
  // the corresponding validity of that type would include both VALID and
  // INVALID.
  test_cases[0].push_back(
      {"Tennessee",
       ADDRESS_HOME_STATE,
       {AutofillDataModel::VALID, AutofillDataModel::INVALID}});
  // Alice appears only in the second profile as a NAME_FIRST, and it's
  // UNVALIDATED.
  test_cases[1].push_back(
      {"Alice", NAME_FIRST, {AutofillDataModel::UNVALIDATED}});
  // An UNKNOWN type is always UNVALIDATED.
  test_cases[2].push_back({"What a beautiful day!",
                           UNKNOWN_TYPE,
                           {AutofillDataModel::UNVALIDATED}});

  for (const std::vector<TestFieldData>& test_fields : test_cases) {
    FormData form;
    form.name = ASCIIToUTF16("MyForm");
    form.url = GURL("http://myform.com/form.html");
    form.action = GURL("http://myform.com/submit.html");

    // Create the form fields specified in the test case.
    FormFieldData field;
    ServerFieldTypeSet possible_types;
    ServerFieldTypeValidityStatesMap possible_types_validities;
    for (const TestFieldData& test_field : test_fields) {
      test::CreateTestFormField("", "1", "", "text", &field);
      field.value = ASCIIToUTF16(test_field.input_value);
      form.fields.push_back(field);
    }

    // Assign the specified predicted type for each field in the test case.
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form_structure.field(i)->set_server_type(test_fields[i].field_type);
    }

    AutofillManager::DeterminePossibleFieldTypesForUpload(
        profiles, {}, base::string16(), "en-us", &form_structure);

    ASSERT_EQ(test_fields.size(), form_structure.field_count());

    for (size_t i = 0; i < test_fields.size(); ++i) {
      possible_types = form_structure.field(i)->possible_types();
      // For both cases we only expect one possible type.
      EXPECT_EQ(1U, possible_types.size());
      // Expect to see the field_type as the possible type.
      EXPECT_NE(possible_types.end(),
                possible_types.find(test_fields[i].field_type));

      // Expect the same for possible_types_validities.
      possible_types_validities =
          form_structure.field(i)->possible_types_validities();
      EXPECT_EQ(1U, possible_types_validities.size());
      EXPECT_NE(possible_types_validities.end(),
                possible_types_validities.find(test_fields[i].field_type));
      // Check for the expected validity states for the possible type.
      EXPECT_EQ(test_fields[i].expected_validity_states.size(),
                possible_types_validities[test_fields[i].field_type].size());
      for (size_t j = 0; j < test_fields[i].expected_validity_states.size();
           ++j)
        EXPECT_EQ(possible_types_validities[test_fields[i].field_type][j],
                  test_fields[i].expected_validity_states[j]);
    }
  }
}

// Tests that DisambiguateUploadTypes makes the correct choices.
TEST_F(AutofillManagerTest, DisambiguateUploadTypes) {
  // Set up the test profile.
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "", "Memphis", "Tennessee", "38116", "US",
                       "(234) 567-8901");
  profile.set_guid("00000000-0000-0000-0000-000000000001");
  profiles.push_back(profile);

  // Set up the test credit card.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley", "4234-5678-9012-3456",
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  typedef struct {
    std::string input_value;
    ServerFieldType predicted_type;
    bool expect_disambiguation;
    ServerFieldType expected_upload_type;
  } TestFieldData;

  std::vector<TestFieldData> test_cases[13];

  // Address disambiguation.
  // An ambiguous address line followed by a field predicted as a line 2 and
  // that is empty should be disambiguated as an ADDRESS_HOME_LINE1.
  test_cases[0].push_back({"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1, true,
                           ADDRESS_HOME_LINE1});
  test_cases[0].push_back({"", ADDRESS_HOME_LINE2, true, EMPTY_TYPE});

  // An ambiguous address line followed by a field predicted as a line 2 but
  // filled with another know profile value should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases[1].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});
  test_cases[1].push_back(
      {"38116", ADDRESS_HOME_LINE2, true, ADDRESS_HOME_ZIP});

  // An ambiguous address line followed by an empty field predicted as
  // something other than a line 2 should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases[2].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});
  test_cases[2].push_back({"", ADDRESS_HOME_ZIP, true, EMPTY_TYPE});

  // An ambiguous address line followed by no other field should be
  // disambiguated as an ADDRESS_HOME_STREET_ADDRESS.
  test_cases[3].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});

  // Phone number disambiguation.
  // A field with possible types PHONE_HOME_CITY_AND_NUMBER and
  // PHONE_HOME_WHOLE_NUMBER should be disambiguated as
  // PHONE_HOME_CITY_AND_NUMBER
  test_cases[4].push_back({"2345678901", PHONE_HOME_WHOLE_NUMBER, true,
                           PHONE_HOME_CITY_AND_NUMBER});

  // Name disambiguation.
  // An ambiguous name field that has no next field and that is preceded by
  // a non credit card field should be disambiguated as a non credit card
  // name.
  test_cases[5].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[5].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[5].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});

  // An ambiguous name field that has no next field and that is preceded by
  // a credit card field should be disambiguated as a credit card name.
  test_cases[6].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});
  test_cases[6].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[6].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});

  // An ambiguous name field that has no previous field and that is
  // followed by a non credit card field should be disambiguated as a non
  // credit card name.
  test_cases[7].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[7].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});
  test_cases[7].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});

  // An ambiguous name field that has no previous field and that is followed
  // by a credit card field should be disambiguated as a credit card name.
  test_cases[8].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[8].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});
  test_cases[8].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});

  // An ambiguous name field that is preceded and followed by non credit
  // card fields should be disambiguated as a non credit card name.
  test_cases[9].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[9].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[9].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});
  test_cases[9].push_back(
      {"Tennessee", ADDRESS_HOME_STATE, true, ADDRESS_HOME_STATE});

  // An ambiguous name field that is preceded and followed by credit card
  // fields should be disambiguated as a credit card name.
  test_cases[10].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});
  test_cases[10].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[10].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});
  test_cases[10].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});

  // An ambiguous name field that is preceded by a non credit card field and
  // followed by a credit card field should not be disambiguated.
  test_cases[11].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[11].push_back(
      {"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST});
  test_cases[11].push_back(
      {"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST});
  test_cases[11].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});

  // An ambiguous name field that is preceded by a credit card field and
  // followed by a non credit card field should not be disambiguated.
  test_cases[12].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});
  test_cases[12].push_back(
      {"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST});
  test_cases[12].push_back(
      {"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST});
  test_cases[12].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});

  for (const std::vector<TestFieldData>& test_fields : test_cases) {
    FormData form;
    form.name = ASCIIToUTF16("MyForm");
    form.url = GURL("http://myform.com/form.html");
    form.action = GURL("http://myform.com/submit.html");

    // Create the form fields specified in the test case.
    FormFieldData field;
    for (const TestFieldData& test_field : test_fields) {
      test::CreateTestFormField("", "1", "", "text", &field);
      field.value = ASCIIToUTF16(test_field.input_value);
      form.fields.push_back(field);
    }

    // Assign the specified predicted type for each field in the test case.
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form_structure.field(i)->set_server_type(test_fields[i].predicted_type);
    }

    AutofillManager::DeterminePossibleFieldTypesForUpload(
        profiles, credit_cards, base::string16(), "en-us", &form_structure);
    ASSERT_EQ(test_fields.size(), form_structure.field_count());

    // Make sure the disambiguation method selects the expected upload type.
    ServerFieldTypeSet possible_types;
    for (size_t i = 0; i < test_fields.size(); ++i) {
      possible_types = form_structure.field(i)->possible_types();
      if (test_fields[i].expect_disambiguation) {
        EXPECT_EQ(1U, possible_types.size());
        EXPECT_NE(possible_types.end(),
                  possible_types.find(test_fields[i].expected_upload_type));
      } else {
        EXPECT_EQ(2U, possible_types.size());
      }
    }
  }
}

// When a field contains fields with UPI/VPA values, a crowdsourcing vote should
// be uploaded.
TEST_F(AutofillManagerTest, CrowdsourceUPIVPA) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  FormData form;
  FormFieldData field;
  test::CreateTestFormField("", "name1", "1234@indianbank", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "name2", "not-upi@gmail.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  EXPECT_THAT(form_structure.field(0)->possible_types(), ElementsAre(UPI_VPA));
  EXPECT_THAT(form_structure.field(1)->possible_types(),
              Not(Contains(UPI_VPA)));
}

// If a server-side credit card is unmasked by entering the CVC, the
// AutofillManager reuses the CVC value to identify a potentially existing CVC
// form field to cast a |CREDIT_CARD_VERIFICATION_CODE|-type vote.
TEST_F(AutofillManagerTest, CrowdsourceCVCFieldByValue) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  const char cvc[] = "1234";
  const char four_digit_but_not_cvc[] = "6676";
  const char credit_card_number[] = "4234-5678-9012-3456";

  FormData form;
  FormFieldData field1;
  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field1);
  form.fields.push_back(field1);

  // This field would not be detected as CVC heuristically if the CVC value
  // wouldn't be known.
  FormFieldData field2;
  test::CreateTestFormField("not_cvc", "not_cvc", four_digit_but_not_cvc,
                            "text", &field2);
  form.fields.push_back(field2);

  // This field has the CVC value used to unlock the card and should be detected
  // as the CVC field.
  FormFieldData field3;
  test::CreateTestFormField("c_v_c", "c_v_c", cvc, "text", &field3);
  form.fields.push_back(field3);

  FormStructure form_structure(form);
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::ASCIIToUTF16(cvc), "en-us",
      &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(
      form_structure, 2, CREDIT_CARD_VERIFICATION_CODE,
      FieldPropertiesFlags::KNOWN_VALUE);
}

// Expiration year field was detected by the server. The other field with a
// 4-digit value should be detected as CVC.
TEST_F(AutofillManagerTest,
       CrowdsourceCVCFieldAfterInvalidExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";
  const char cvc[] = "1234";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Expiration date, but is not the expiration date of the used credit card.
  FormFieldData field1;
  test::CreateTestFormField("exp_year", "exp_year",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("cvc_number", "cvc_number", cvc, "text", &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::NO_FLAGS);
}

// Tests if the CVC field is heuristically detected if it appears after the
// expiration year field as it was predicted by the server.
// The value in the CVC field would be a valid expiration year value.
TEST_F(AutofillManagerTest, CrowdsourceCVCFieldAfterExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char cvc[] = "1234";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Expiration date, that is the expiration date of the used credit card.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            actual_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::NO_FLAGS);
}

// Tests if the CVC field is heuristically detected if it contains a value which
// is not a valid expiration year.
TEST_F(AutofillManagerTest, CrowdsourceCVCFieldBeforeExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Must be CVC since it is an implausible expiration date.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", "2130", "text",
                            &field2);
  form.fields.push_back(field2);

  // A field which is filled with a plausible expiration date which is not the
  // date of the credit card.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({UNKNOWN_TYPE});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 1,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::NO_FLAGS);
}

// Tests if no CVC field is heuristically detected due to the missing of a
// credit card number field.
TEST_F(AutofillManagerTest, CrowdsourceNoCVCFieldDueToMissingCreditCardNumber) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";
  const char cvc[] = "2031";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Server predicted as expiration year.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({UNKNOWN_TYPE});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);
  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

// Test if no CVC is found because the candidate has no valid CVC value.
TEST_F(AutofillManagerTest, CrowdsourceNoCVCDueToInvalidCandidateValue) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char credit_card_exp_year[] = "2030";
  const char cvc[] = "12";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Server predicted as expiration year.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types(
      {CREDIT_CARD_NUMBER, UNKNOWN_TYPE});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  AutofillManager::DeterminePossibleFieldTypesForUpload(
      profiles, credit_cards, base::string16(), "en-us", &form_structure);

  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

TEST_F(AutofillManagerTest, RemoveProfile) {
  // Add and remove an Autofill profile.
  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000102";
  profile.set_guid(guid);
  personal_data_.AddProfile(profile);

  int id = MakeFrontendID(std::string(), guid);

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data_.GetProfileWithGUID(guid));
}

TEST_F(AutofillManagerTest, RemoveCreditCard) {
  // Add and remove an Autofill credit card.
  CreditCard credit_card;
  const char guid[] = "00000000-0000-0000-0000-000000100007";
  credit_card.set_guid(guid);
  personal_data_.AddCreditCard(credit_card);

  int id = MakeFrontendID(guid, std::string());

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data_.GetCreditCardWithGUID(guid));
}

// Test our external delegate is called at the right time.
TEST_F(AutofillManagerTest, TestExternalDelegate) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);  // should call the delegate's OnQuery()

  EXPECT_TRUE(external_delegate_->on_query_seen());
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(AutofillManagerTest, OnTextFieldDidChangeAndUnfocus_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  // Simulate editing a field.
  autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  autofill_manager_->OnFocusNoLongerOnForm();
}

// Test that navigating with a filled form sends an upload with types matching
// the fields.
TEST_F(AutofillManagerTest, OnTextFieldDidChangeAndNavigation_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  // Simulate editing a field.
  autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate a navigation so that the pending form is uploaded.
  autofill_manager_->Reset();
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(AutofillManagerTest, OnDidFillAutofillFormDataAndUnfocus_Upload) {
  // Set up our form data (empty).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;

  // These fields should all match.
  ServerFieldTypeSet types;
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  autofill_manager_->SetExpectedObservedSubmission(false);

  // Form was autofilled with user data.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  autofill_manager_->OnDidFillAutofillFormData(form,
                                               AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  autofill_manager_->OnFocusNoLongerOnForm();
}

// Test that suggestions are returned for credit card fields with an
// unrecognized
// autocomplete attribute.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  // Set a valid autocomplete attribute on the card name.
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute on the card number.
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute on the expiration month.
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Suggestions should be returned for the first two fields
  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Suggestions should still be returned for the third field because it is a
  // credit card field.
  GetAutofillSuggestions(form, form.fields[2]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test to verify suggestions appears for forms having credit card number split
// across fields.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_ForNumberSplitAcrossFields) {
  // Set up our form data with credit card number split across fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData name_field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text",
                            &name_field);
  form.fields.push_back(name_field);

  // Add new 4 |card_number_field|s to the |form|.
  FormFieldData card_number_field;
  card_number_field.max_length = 4;
  test::CreateTestFormField("Card Number", "cardnumber_1", "", "text",
                            &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_2", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_3", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_4", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  FormFieldData exp_field;
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                            &exp_field);
  form.fields.push_back(exp_field);

  test::CreateTestFormField("", "ccyear", "", "text", &exp_field);
  form.fields.push_back(exp_field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Verify whether suggestions are populated correctly for one of the middle
  // credit card number fields when filled partially.
  FormFieldData number_field = form.fields[3];
  number_field.value = ASCIIToUTF16("901");

  // Get the suggestions for already filled credit card |number_field|.
  GetAutofillSuggestions(form, number_field);

#if defined(OS_ANDROID) || defined(OS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard, autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that inputs detected to be CVC inputs are forced to
// !should_autocomplete for AutocompleteHistoryManager::OnWillSubmitForm.
TEST_F(AutofillManagerTest, DontSaveCvcInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  EXPECT_CALL(*(autocomplete_history_manager_.get()), OnWillSubmitForm(_, true))
      .WillOnce(SaveArg<0>(&form_seen_by_ahm));

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  struct {
    const char* label;
    const char* name;
    const char* value;
    ServerFieldType expected_field_type;
  } test_fields[] = {
      {"Card number", "1", "4234-5678-9012-3456", CREDIT_CARD_NUMBER},
      {"Card verification code", "2", "123", CREDIT_CARD_VERIFICATION_CODE},
      {"expiration date", "3", "04/2020", CREDIT_CARD_EXP_4_DIGIT_YEAR},
  };

  for (const auto& test_field : test_fields) {
    FormFieldData field;
    test::CreateTestFormField(test_field.label, test_field.name,
                              test_field.value, "text", &field);
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormSubmitted(form);

  EXPECT_EQ(form.fields.size(), form_seen_by_ahm.fields.size());
  ASSERT_EQ(base::size(test_fields), form_seen_by_ahm.fields.size());
  for (size_t i = 0; i < base::size(test_fields); ++i) {
    EXPECT_EQ(
        form_seen_by_ahm.fields[i].should_autocomplete,
        test_fields[i].expected_field_type != CREDIT_CARD_VERIFICATION_CODE);
  }
}

TEST_F(AutofillManagerTest, DontOfferToSavePaymentsCard) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  // Manually fill out |form| so we can use it in OnFormSubmitted.
  for (size_t i = 0; i < form.fields.size(); ++i) {
    if (form.fields[i].name == ASCIIToUTF16("cardnumber"))
      form.fields[i].value = ASCIIToUTF16("4012888888881881");
    else if (form.fields[i].name == ASCIIToUTF16("nameoncard"))
      form.fields[i].value = ASCIIToUTF16("John H Dillinger");
    else if (form.fields[i].name == ASCIIToUTF16("ccmonth"))
      form.fields[i].value = ASCIIToUTF16("01");
    else if (form.fields[i].name == ASCIIToUTF16("ccyear"))
      form.fields[i].value = ASCIIToUTF16("2017");
  }

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.should_store_pan = false;
  details.cvc = ASCIIToUTF16("123");
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::SUCCESS, "4012888888881881");
  autofill_manager_->OnFormSubmitted(form, false,
                                     SubmissionSource::FORM_SUBMISSION);
}

TEST_F(AutofillManagerTest, FillInUpdatedExpirationDate) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.should_store_pan = false;
  details.cvc = ASCIIToUTF16("123");
  details.exp_month = ASCIIToUTF16("02");
  details.exp_year = ASCIIToUTF16("2018");
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::SUCCESS, "4012888888881881");
}

TEST_F(AutofillManagerTest, ProfileDisabledDoesNotFillFormData) {
  autofill_manager_->SetProfileAutofillEnabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _)).Times(0);

  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       MakeFrontendID(std::string(), guid));
}

TEST_F(AutofillManagerTest, ProfileDisabledDoesNotSuggest) {
  autofill_manager_->SetProfileAutofillEnabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "", "email", &field);
  GetAutofillSuggestions(form, field);
  // Expect no suggestions as autofill and autocomplete are disabled for
  // addresses.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(AutofillManagerTest, CreditCardDisabledDoesNotFillFormData) {
  autofill_manager_->SetCreditCardAutofillEnabled(false);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _)).Times(0);

  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       MakeFrontendID(guid, std::string()));
}

TEST_F(AutofillManagerTest, CreditCardDisabledDoesNotSuggest) {
  autofill_manager_->SetCreditCardAutofillEnabled(false);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo());
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  GetAutofillSuggestions(form, field);
  // Expect no suggestions as autofill and autocomplete are disabled for credit
  // cards.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Verify that typing "gmail" matches "theking@gmail.com" and "buddy@gmail.com"
// when substring matching is enabled.
TEST_P(SuggestionMatchingTest, DisplaySuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "gmail", "email", &field);
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 =
          MakeLabel({"Charles Holley", "123 Apple St., unit 6", "23456789012"});
      label2 = MakeLabel({"Elvis Presley", "3734 Elvis Presley Blvd., Apt. 10",
                          "(234) 567-8901"});
      break;
    case EnabledFeature::kMobileShowAll:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It
      // has an extra digit.
      label1 =
          MakeMobileLabel({"Charles", "123 Apple St., unit 6", "23456789012"});
      label2 = MakeMobileLabel(
          {"Elvis", "3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("buddy@gmail.com", label1, "", 1),
                   Suggestion("theking@gmail.com", label2, "", 2));
}

// Verify that typing "apple" will match "123 Apple St." when substring matching
// is enabled.
TEST_P(SuggestionMatchingTest,
       DisplaySuggestionsWithMatchingTokens_CaseIgnored) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Address Line 2", "addr2", "apple", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
      label = "Charles Holley";
      break;
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
      label = "23456789012";
      break;
    case EnabledFeature::kNone:
      label = "123 Apple St.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(kDefaultPageID,
                   Suggestion("123 Apple St., unit 6", label, "", 1));
}

// Verify that typing "mail" will not match any of the "@gmail.com" email
// addresses when substring matching is enabled.
TEST_F(AutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "mail", "email", &field);
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Verify that typing "dre" matches "Nancy Drew" when substring matching is
// enabled.
TEST_P(CreditCardSuggestionMatchingTest,
       DisplayCreditCardSuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "dre", "text",
                            &field);

  const char guid[] = "00000000-0000-0000-0000-000000000030";
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Nancy Drew",
                          "4444555566667777",  // Visa
                          "01", "2030", "1");
  credit_card.set_guid(guid);
  personal_data_.AddCreditCard(credit_card);

#if defined(OS_ANDROID)
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? test::ObfuscatedCardDigitsAsUTF8("7777")
          : std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("7777");

#elif defined(OS_IOS)
  const std::string visa_label = test::ObfuscatedCardDigitsAsUTF8("7777");

#else
  const std::string visa_label = base::UTF16ToUTF8(
      credit_card.NetworkOrBankNameLastFourDigitsAndDescriptiveExpiration(
          "en-US"));
#endif

  GetAutofillSuggestions(form, field);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Nancy Drew", visa_label, kVisaCard,
                              MakeFrontendID(guid, std::string())));
}

// Verify that typing "lvis" will not match any of the credit card name when
// substring matching is enabled.
TEST_F(AutofillManagerTest, NoCreditCardSuggestionsForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "lvis", "text",
                            &field);
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(AutofillManagerTest, GetPopupType_CreditCardForm) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kCreditCards,
              autofill_manager_->GetPopupType(form, field));
  }
}

TEST_F(AutofillManagerTest, GetPopupType_AddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kAddresses,
              autofill_manager_->GetPopupType(form, field));
  }
}

TEST_F(AutofillManagerTest, GetPopupType_PersonalInformationForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kPersonalInformation,
              autofill_manager_->GetPopupType(form, field));
  }
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit of three and no impressions yet.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_UnmetLimit) {
  // No impressions yet.
  ASSERT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // The mock implementation of ShouldShowSigninPromo() will return true here,
  // creating an impression, and false below, preventing an impression.
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Expect to now have an impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // No additional impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit that has been attained already.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_WithAttainedLimit) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Set the impression count to the same value as the limit.
  autofill_client_.GetPrefs()->SetInteger(
      prefs::kAutofillCreditCardSigninPromoImpressionCount,
      kCreditCardSigninPromoImpressionLimit);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions stay the same.
  EXPECT_EQ(kCreditCardSigninPromoImpressionLimit,
            autofill_client_.GetPrefs()->GetInteger(
                prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form on a non-secure page.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_NonSecureContext) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  autofill_client_.set_form_origin(form.url);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form targeting a non-secure page.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_NonSecureAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for an address
// form.
TEST_F(AutofillManagerTest, ShouldShowCreditCardSigninPromo_AddressField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);

  // Call will now return false, because it is initiated from an address field.
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(0);
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));
}

// Verify that typing "S" into the middle name field will match and order middle
// names "Shawn Smith" followed by "Adam Smith" i.e. prefix matched followed by
// substring matched.
TEST_P(SuggestionMatchingTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000103");
  profile1.SetInfo(NAME_FIRST, ASCIIToUTF16("Robin"), "en-US");
  profile1.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Adam Smith"), "en-US");
  profile1.SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                   "en-US");
  personal_data_.AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, ASCIIToUTF16("Carl"), "en-US");
  profile2.SetInfo(NAME_MIDDLE, ASCIIToUTF16("Shawn Smith"), "en-US");
  profile2.SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                   "en-US");
  personal_data_.AddProfile(profile2);

  FormFieldData field;
  test::CreateTestFormField("Middle Name", "middlename", "S", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label1 = "1234 Smith Blvd.";
      label2 = "1234 Smith Blvd.";
      break;
    case EnabledFeature::kNone:
      label1 = "1234 Smith Blvd., Carl Shawn Smith Grimes";
      label2 = "1234 Smith Blvd., Robin Adam Smith Grimes";
  }
  CheckSuggestions(kDefaultPageID, Suggestion("Shawn Smith", label1, "", 1),
                   Suggestion("Adam Smith", label2, "", 2));
}

TEST_F(AutofillManagerTest, ShouldUploadForm) {
  // Note: The enforcement of a minimum number of required fields for upload
  // is disabled by default. This tests validates both the disabled and enabled
  // scenarios.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.url = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  // Empty Form.
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Add a field to the form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);

  // With min required fields enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // With min required fields disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // Add a second field to the form.
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  // With min required fields enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // With min required fields disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // Has less than 3 fields but has autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";

  // With min required fields enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // With min required fields disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // Has more than 3 fields, no autocomplete attribute.
  form.fields[0].autocomplete_attribute = "";
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure_3(form);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields and at least one autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Is off the record.
  autofill_driver_->SetIsIncognito(true);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Make sure it's reset for the next test case.
  autofill_driver_->SetIsIncognito(false);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has one field which is appears to be a password field.
  form.fields.clear();
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);

  // With min required fields enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // With min required fields disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kAutofillEnforceMinRequiredFieldsForUpload);
    EXPECT_TRUE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
  }

  // Autofill disabled.
  autofill_manager_->SetProfileAutofillEnabled(false);
  autofill_manager_->SetCreditCardAutofillEnabled(false);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(FormStructure(form)));
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off.
TEST_F(AutofillManagerTest,
       DisplaySuggestions_AutocompleteOffNotRespected_AddressField) {
  // Set up an address form.
  FormData mixed_form;
  mixed_form.name = ASCIIToUTF16("MyForm");
  mixed_form.url = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  std::vector<FormData> mixed_forms(1, mixed_form);
  FormsSeen(mixed_forms);

  // Suggestions should be displayed on desktop for this field in all
  // circumstances.
  GetAutofillSuggestions(mixed_form, mixed_form.fields[0]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Suggestions should always be displayed for all the other fields.
  for (size_t i = 1U; i < mixed_form.fields.size(); ++i) {
    GetAutofillSuggestions(mixed_form, mixed_form.fields[i]);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off
// and the feature to autofill all addresses is also off.
TEST_F(AutofillManagerTest,
       DisplaySuggestions_AutocompleteOffRespected_AddressField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAutofillAlwaysFillAddresses);

  // Set up an address form.
  FormData mixed_form;
  mixed_form.name = ASCIIToUTF16("MyForm");
  mixed_form.url = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  std::vector<FormData> mixed_forms(1, mixed_form);
  FormsSeen(mixed_forms);

  // Suggestions should not be displayed on desktop for this field.
  GetAutofillSuggestions(mixed_form, mixed_form.fields[0]);
  if (IsDesktopPlatform()) {
    EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
  } else {
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Suggestions should always be displayed for all the other fields.
  for (size_t i = 1U; i < mixed_form.fields.size(); ++i) {
    GetAutofillSuggestions(mixed_form, mixed_form.fields[i]);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Verify that suggestions are shown on desktop for credit card related fields
// even if the initiating field field has the "autocomplete" attribute set to
// off.
TEST_F(AutofillManagerTest,
       DisplaySuggestions_AutocompleteOff_CreditCardField) {
  // Set up a credit card form.
  FormData mixed_form;
  mixed_form.name = ASCIIToUTF16("MyForm");
  mixed_form.url = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccexpiresmonth", "", "text",
                            &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  mixed_form.fields.push_back(field);
  std::vector<FormData> mixed_forms(1, mixed_form);
  FormsSeen(mixed_forms);

  // Suggestions should always be displayed.
  for (const FormFieldData& field : mixed_form.fields) {
    GetAutofillSuggestions(mixed_form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with server only types is still autofillable if the form
// gets updated in cache.
TEST_F(AutofillManagerTest, DisplaySuggestionsForUpdatedServerTypedForm) {
  // Create a form with unknown heuristic fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Field 1", "field1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 2", "field2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 3", "field3", "", "text", &field);
  form.fields.push_back(field);

  auto form_structure = std::make_unique<TestFormStructure>(form);
  form_structure->DetermineHeuristicTypes();
  // Make sure the form can not be autofilled now.
  ASSERT_EQ(0u, form_structure->autofill_count());
  for (size_t idx = 0; idx < form_structure->field_count(); ++idx) {
    ASSERT_EQ(UNKNOWN_TYPE, form_structure->field(idx)->heuristic_type());
  }

  // Prepare and set known server fields.
  const std::vector<ServerFieldType> heuristic_types(form.fields.size(),
                                                     UNKNOWN_TYPE);
  const std::vector<ServerFieldType> server_types{NAME_FIRST, NAME_MIDDLE,
                                                  NAME_LAST};
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Make sure the form can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    ASSERT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify one of the fields in the original form.
  form.fields[0].css_classes += ASCIIToUTF16("a");

  // Expect the form still can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify form action URL. This can happen on in-page navitaion if the form
  // doesn't have an actual action (attribute is empty).
  form.action = net::AppendQueryParameter(form.action, "arg", "value");

  // Expect the form still can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with <select> field is accepted if <option> value (not
// content) is quite long. Some websites use value to propagate long JSON to
// JS-backed logic.
TEST_F(AutofillManagerTest, FormWithLongOptionValuesIsAcceptable) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  // Prepare <select> field with long <option> values.
  const size_t kOptionValueLength = 10240;
  const std::string long_string(kOptionValueLength, 'a');
  const std::vector<const char*> values(3, long_string.c_str());
  const std::vector<const char*> contents{"A", "B", "C"};
  test::CreateTestSelectField("Country", "country", "", values, contents,
                              values.size(), &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Suggestions should be displayed.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Test that with small form upload enabled but heuristics and query disabled
// we get uploads but not quality metrics.
TEST_F(AutofillManagerTest, SmallForm_Upload_NoHeuristicsOrQuery) {
  // Setup the feature environment.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      // Enabled.
      {kAutofillEnforceMinRequiredFieldsForHeuristics,
       kAutofillEnforceMinRequiredFieldsForQuery},
      // Disabled.
      {kAutofillEnforceMinRequiredFieldsForUpload});

  // Add a local card to allow data matching for upload votes.
  CreditCard credit_card =
      autofill::test::GetRandomCreditCard(CreditCard::LOCAL_CARD);
  personal_data_.AddCreditCard(credit_card);

  // Set up the form.
  FormData form;
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Unknown", "unknown", "", "text", &field);
  form.fields.push_back(field);

  // Have the browser encounter the form.
  FormsSeen({form});

  // Populate the form with a credit card value.
  form.fields.back().value = credit_card.number();

  // Setup expectation on the test autofill manager (these are validated
  // during the simlulated submit).
  autofill_manager_->SetExpectedSubmittedFieldTypes({{CREDIT_CARD_NUMBER}});
  autofill_manager_->SetExpectedObservedSubmission(true);
  autofill_manager_->SetCallParentUploadFormData(true);
  EXPECT_CALL(*download_manager_,
              StartUploadRequest(_, false, _, std::string(), true, _));

  base::HistogramTester histogram_tester;
  FormSubmitted(form);

  EXPECT_EQ(FormStructure(form).FormSignatureAsStr(),
            autofill_manager_->GetSubmittedFormSignature());

  histogram_tester.ExpectTotalCount("Autofill.FieldPrediction.CreditCard", 0);
}

// Test that is_all_server_suggestions is true if there are only
// full_server_card and masked_server_card on file.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_IsAllServerSuggestionsTrue) {
  // Create server credit cards.
  CreateTestServerCreditCards();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  ASSERT_TRUE(external_delegate_->is_all_server_suggestions());
}

// Test that is_all_server_suggestions is false if there is at least one
// local_card on file.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_IsAllServerSuggestionsFalse) {
  // Create server and local credit cards.
  CreateTestServerAndLocalCreditCards();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  ASSERT_FALSE(external_delegate_->is_all_server_suggestions());
}

// If the rich query feature is enabled, the IsRichQueryEnabled methods only
// returns true if the channel is neither STABLE not BETA.
TEST_F(AutofillManagerTest, IsRichQueryEnabled_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillRichMetadataQueries);

  for (auto channel :
       {version_info::Channel::STABLE, version_info::Channel::BETA,
        version_info::Channel::CANARY, version_info::Channel::DEV,
        version_info::Channel::UNKNOWN}) {
    SCOPED_TRACE(::testing::Message()
                 << "Channel " << static_cast<int>(channel));
    EXPECT_CALL(autofill_client_, GetChannel()).WillOnce(Return(channel));
    TestAutofillManager test_instance(autofill_driver_.get(), &autofill_client_,
                                      &personal_data_,
                                      autocomplete_history_manager_.get());
    switch (channel) {
      case version_info::Channel::STABLE:
      case version_info::Channel::BETA:
        EXPECT_FALSE(AutofillManager::IsRichQueryEnabled(channel));
        EXPECT_FALSE(test_instance.is_rich_query_enabled());
        break;
      case version_info::Channel::CANARY:
      case version_info::Channel::DEV:
      case version_info::Channel::UNKNOWN:
        EXPECT_TRUE(AutofillManager::IsRichQueryEnabled(channel));
        EXPECT_TRUE(test_instance.is_rich_query_enabled());
        break;
    }
  }
}

// No matter what the channel, IsRichQueryEnabled returns false if the feature
// is disabled.
TEST_F(AutofillManagerTest, IsRichQueryEnabled_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillRichMetadataQueries);

  for (auto channel :
       {version_info::Channel::STABLE, version_info::Channel::BETA,
        version_info::Channel::CANARY, version_info::Channel::DEV,
        version_info::Channel::UNKNOWN}) {
    SCOPED_TRACE(::testing::Message()
                 << "Channel " << static_cast<int>(channel));
    EXPECT_FALSE(AutofillManager::IsRichQueryEnabled(channel));
    EXPECT_CALL(autofill_client_, GetChannel()).WillOnce(Return(channel));
    TestAutofillManager test_instance(autofill_driver_.get(), &autofill_client_,
                                      &personal_data_,
                                      autocomplete_history_manager_.get());
    EXPECT_FALSE(test_instance.is_rich_query_enabled());
  }
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogAutocompleteShownMetric) {
  FormData form;
  form.name = ASCIIToUTF16("NothingSpecial");

  FormFieldData field;
  test::CreateTestFormField("Something", "something", "", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/false,
                                        form, field);
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      1);

  // No Autofill logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                        HasSubstr("Autofill.FormEvents.Address"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogAutofillAddressShownMetric) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autocomplete.Events"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogByType_AddressOnly) {
  // Create a form with name and address fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.AddressOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.AddressOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_AddressOnlyWithoutName) {
  // Create a form with address fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);

  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.AddressOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.AddressOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogByType_ContactOnly) {
  // Create a form with name and contact fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.ContactOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.ContactOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_ContactOnlyWithoutName) {
  // Create a form with contact fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.ContactOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.ContactOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogByType_Other) {
  // Create a form with name fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.Other",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.Other",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogByType_AddressPlusEmail) {
  // Create a form with name, address, and email fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailWithoutName) {
  // Create a form with address and email fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest, DidShowSuggestions_LogByType_AddressPlusPhone) {
  // Create a form with name fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusPhoneWithoutName) {
  // Create a form with name, address, and phone fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhone) {
  // Create a form with name, address, phone, and email fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhoneWithoutName) {
  // Create a form with address, phone, and email fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.button_titles = {std::make_pair(
      ASCIIToUTF16("Submit"), ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(AutofillManagerTest,
       DidShowSuggestions_LogAutofillCreditCardShownMetric) {
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form,
                                        form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or address logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, Not(AnyOf(HasSubstr("Autocomplete.Events"),
                                    HasSubstr("Autofill.FormEvents.Address"))));
}

TEST_F(AutofillManagerTest,
       DidSuppressPopup_LogAutofillAddressPopupSuppressed) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidSuppressPopup(form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                     FORM_EVENT_POPUP_SUPPRESSED, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                        HasSubstr("Autocomplete.Events"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(AutofillManagerTest,
       DidSuppressPopup_LogAutofillCreditCardPopupSuppressed) {
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);

  base::HistogramTester histogram_tester;
  autofill_manager_->DidSuppressPopup(form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.CreditCard",
                                     FORM_EVENT_POPUP_SUPPRESSED, 1);

  // No Autocomplete or address logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                                    HasSubstr("Autocomplete.Events"),
                                    HasSubstr("Autofill.FormEvents.Address"))));
}

// Test that we import data when the field type is determined by the value and
// without any heuristics on the attributes.
TEST_F(AutofillManagerTest, ImportDataWhenValueDetected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("VPA:", "vpa", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  autofill_manager_->SetExpectedObservedSubmission(true);
  autofill_manager_->SetCallParentUploadFormData(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = ASCIIToUTF16("user@indianbank");
  FormSubmitted(form);

  EXPECT_EQ(1, personal_data_.num_times_save_vpa_called());
}

// Test that we do not import VPA data when in incognito.
TEST_F(AutofillManagerTest, DontImportVPAWhenIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);
  autofill_driver_->SetIsIncognito(true);

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("VPA:", "vpa", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  autofill_manager_->SetExpectedObservedSubmission(true);
  autofill_manager_->SetCallParentUploadFormData(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = ASCIIToUTF16("user@indianbank");
  FormSubmitted(form);

  EXPECT_EQ(0, personal_data_.num_times_save_vpa_called());
}

// Test param indicates if there is an active screen reader.
class OnFocusOnFormFieldTest : public AutofillManagerTest,
                               public testing::WithParamInterface<bool> {
 protected:
  OnFocusOnFormFieldTest() = default;
  ~OnFocusOnFormFieldTest() override = default;

  void SetUp() override {
    AutofillManagerTest::SetUp();

    has_active_screen_reader_ = GetParam();
    external_delegate_->set_has_active_screen_reader(has_active_screen_reader_);

    scoped_feature_list_.InitWithFeatures(
        // Enabled
        {},
        // Disabled
        {kAutofillEnforceMinRequiredFieldsForHeuristics,
         kAutofillEnforceMinRequiredFieldsForQuery,
         kAutofillEnforceMinRequiredFieldsForUpload,
         kAutofillRestrictUnownedFieldsToFormlessCheckout});
  }

  void TearDown() override {
    external_delegate_->set_has_active_screen_reader(false);
    AutofillManagerTest::TearDown();
  }

  void CheckSuggestionsAvailableIfScreenReaderRunning() {
#if defined(OS_CHROMEOS)
    // The only existing functions for determining whether ChromeVox is in use
    // are in the src/chrome directory, which cannot be included in components.
    // Thus, if the platform is ChromeOS, we assume that ChromeVox is in use at
    // this point in the code.
    EXPECT_EQ(true,
              external_delegate_->has_suggestions_available_on_field_focus());
#else
    EXPECT_EQ(has_active_screen_reader_,
              external_delegate_->has_suggestions_available_on_field_focus());
#endif  // defined(OS_CHROMEOS)
  }

  void CheckNoSuggestionsAvailableOnFieldFocus() {
    EXPECT_FALSE(
        external_delegate_->has_suggestions_available_on_field_focus());
  }

  bool has_active_screen_reader_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Suggestions should be returned for the first field.
  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[0], gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();

  // No suggestions should be provided for the second field because of its
  // unrecognized autocomplete attribute.
  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_AutocompleteOffNotRespected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillAlwaysFillAddresses);

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  // Set an autocomplete=off attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.should_autocomplete = false;
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_AutocompleteOffRespected) {
  if (!IsDesktopPlatform())
    return;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillAlwaysFillAddresses);

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  // Set an autocomplete=off attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.should_autocomplete = false;
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_SecureContext) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_NonSecureContext) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  // In a non-HTTPS context, there will be a warning indicating the page is
  // insecure.
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_Ablation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillCreditCardAblationExperiment);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1], gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

INSTANTIATE_TEST_SUITE_P(
    AutofillManagerTest,
    ProfileMatchingTypesTest,
    testing::Combine(
        testing::ValuesIn(kProfileMatchingTypesTestCases),
        testing::Range(static_cast<int>(AutofillDataModel::UNVALIDATED),
                       static_cast<int>(AutofillDataModel::UNSUPPORTED) + 1),
        testing::Bool()));

INSTANTIATE_TEST_SUITE_P(All, OnFocusOnFormFieldTest, testing::Bool());

#if defined(OS_IOS) || defined(OS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "show-all"),
                                         std::make_tuple(1, "show-one")));
#else
INSTANTIATE_TEST_SUITE_P(,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "")));
#endif  // defined(OS_IOS) || defined(OS_ANDROID)

INSTANTIATE_TEST_SUITE_P(, CreditCardSuggestionMatchingTest, testing::Bool());

}  // namespace autofill
