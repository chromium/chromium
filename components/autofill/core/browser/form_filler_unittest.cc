// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::UTF8ToUTF16;
using mojom::SubmissionSource;
using test::CreateTestAddressFormData;
using test::CreateTestFormField;
using test::CreateTestPersonalInformationFormData;
using test::CreateTestSelectField;
using test::CreateTestSelectOrSelectListField;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::SaveArg;

struct TestAddressFillData {
  TestAddressFillData(const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* country_short,
                      const char* phone,
                      const char* email,
                      const char* company)
      : first(first),
        middle(middle),
        last(last),
        address1(address1),
        address2(address2),
        city(city),
        state(state),
        postal_code(postal_code),
        country(country),
        country_short(country_short),
        phone(phone),
        email(email),
        company(company) {}

  const char* first;
  const char* middle;
  const char* last;
  const char* address1;
  const char* address2;
  const char* city;
  const char* state;
  const char* postal_code;
  const char* country;
  const char* country_short;
  const char* phone;
  const char* email;
  const char* company;
};

struct TestCardFillData {
  TestCardFillData(const char* name_on_card,
                   const char* card_number,
                   const char* expiration_month,
                   const char* expiration_year,
                   bool use_month_type)
      : name_on_card(name_on_card),
        card_number(card_number),
        expiration_month(expiration_month),
        expiration_year(expiration_year),
        use_month_type(use_month_type) {}
  const char* name_on_card;
  const char* card_number;
  const char* expiration_month;
  const char* expiration_year;
  bool use_month_type;
};

const TestAddressFillData
    kEmptyAddressFillData("", "", "", "", "", "", "", "", "", "", "", "", "");

const TestCardFillData kEmptyCardFillData("",
                                          "",
                                          "",
                                          "",
                                          /*use_month_type=*/false);

const TestCardFillData kElvisCardFillData("Elvis Presley",
                                          "4234567890123456",
                                          "04",
                                          "2999",
                                          /*use_month_type*/ false);

TestAddressFillData GetElvisAddressFillData() {
  return {
      "Elvis",
      "Aaron",
      "Presley",
      "3734 Elvis Presley Blvd.",
      "Apt. 10",
      "Memphis",
      "Tennessee",
      "38116",
      "United States",
      "US",
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber)
          ? "2345678901"
          : "12345678901",
      "theking@gmail.com",
      "RCA"};
}

// Creates a GUID for testing. For example,
// MakeGuid(123) = "00000000-0000-0000-0000-000000000123";
std::string MakeGuid(size_t last_digit) {
  return base::StringPrintf("00000000-0000-0000-0000-%012zu", last_digit);
}

std::string kElvisProfileGuid = MakeGuid(1);

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    ON_CALL(*this, GetChannel())
        .WillByDefault(Return(version_info::Channel::UNKNOWN));
    ON_CALL(*this, IsPasswordManagerEnabled()).WillByDefault(Return(true));
  }
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
  MOCK_METHOD(AutofillOptimizationGuide*,
              GetAutofillOptimizationGuide,
              (),
              (const override));
  MOCK_METHOD(profile_metrics::BrowserProfileType,
              GetProfileType,
              (),
              (const override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));
  MOCK_METHOD(bool, IsPasswordManagerEnabled, (), (override));
  MOCK_METHOD(void,
              DidFillOrPreviewForm,
              (mojom::ActionPersistence action_persistence,
               AutofillTriggerSource trigger_source,
               bool is_refill),
              (override));
  MOCK_METHOD(bool, HasCreditCardScanFeature, (), (const override));
  MOCK_METHOD(void,
              TriggerUserPerceptionOfAutofillSurvey,
              ((const std::map<std::string, std::string>&)),
              (override));
  MOCK_METHOD(AutofillComposeDelegate*, GetComposeDelegate, (), (override));
  MOCK_METHOD(void,
              OnVirtualCardDataAvailable,
              (const VirtualCardManualFallbackBubbleOptions&),
              (override));
};

AutofillProfile FillDataToAutofillProfile(const TestAddressFillData& data) {
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&profile, data.first, data.middle, data.last, data.email,
                       data.company, data.address1, data.address2, data.city,
                       data.state, data.postal_code, data.country_short,
                       data.phone);
  return profile;
}

CreditCard FillDataToCreditCardInfo(const TestCardFillData& data) {
  CreditCard card;
  test::SetCreditCardInfo(&card, data.name_on_card, data.card_number,
                          data.expiration_month, data.expiration_year, "1");
  return card;
}

void ExpectFilledField(const char* expected_label,
                       const char* expected_name,
                       const char* expected_value,
                       FormControlType expected_form_control_type,
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
void ExpectFilledForm(
    const FormData& filled_form,
    const std::optional<TestAddressFillData>& address_fill_data,
    const std::optional<TestCardFillData>& card_fill_data) {
  // The number of fields in the address and credit card forms created above.
  const size_t kAddressFormSize = 11;
  const size_t kCreditCardFormSizeMonthType = 4;
  const size_t kCreditCardFormSizeNotMonthType = 5;

  EXPECT_EQ(u"MyForm", filled_form.name);
  EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.url);
  EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);

  size_t form_size = 0;
  if (address_fill_data) {
    form_size += kAddressFormSize;
  }
  if (card_fill_data) {
    form_size += card_fill_data->use_month_type
                     ? kCreditCardFormSizeMonthType
                     : kCreditCardFormSizeNotMonthType;
  }
  ASSERT_EQ(form_size, filled_form.fields.size());

  if (address_fill_data) {
    ExpectFilledField("First Name", "firstname", address_fill_data->first,
                      FormControlType::kInputText, filled_form.fields[0]);
    ExpectFilledField("Middle Name", "middlename", address_fill_data->middle,
                      FormControlType::kInputText, filled_form.fields[1]);
    ExpectFilledField("Last Name", "lastname", address_fill_data->last,
                      FormControlType::kInputText, filled_form.fields[2]);
    ExpectFilledField("Address Line 1", "addr1", address_fill_data->address1,
                      FormControlType::kInputText, filled_form.fields[3]);
    ExpectFilledField("Address Line 2", "addr2", address_fill_data->address2,
                      FormControlType::kInputText, filled_form.fields[4]);
    ExpectFilledField("City", "city", address_fill_data->city,
                      FormControlType::kInputText, filled_form.fields[5]);
    ExpectFilledField("State", "state", address_fill_data->state,
                      FormControlType::kInputText, filled_form.fields[6]);
    ExpectFilledField("Postal Code", "zipcode", address_fill_data->postal_code,
                      FormControlType::kInputText, filled_form.fields[7]);
    ExpectFilledField("Country", "country", address_fill_data->country,
                      FormControlType::kInputText, filled_form.fields[8]);
    ExpectFilledField("Phone Number", "phonenumber", address_fill_data->phone,
                      FormControlType::kInputTelephone, filled_form.fields[9]);
    ExpectFilledField("Email", "email", address_fill_data->email,
                      FormControlType::kInputEmail, filled_form.fields[10]);
  }

  if (card_fill_data) {
    size_t offset = address_fill_data ? kAddressFormSize : 0;
    ExpectFilledField("Name on Card", "nameoncard",
                      card_fill_data->name_on_card, FormControlType::kInputText,
                      filled_form.fields[offset + 0]);
    ExpectFilledField("Card Number", "cardnumber", card_fill_data->card_number,
                      FormControlType::kInputText,
                      filled_form.fields[offset + 1]);
    if (card_fill_data->use_month_type) {
      std::string exp_year = card_fill_data->expiration_year;
      std::string exp_month = card_fill_data->expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty()) {
        date = exp_year + "-" + exp_month;
      }

      ExpectFilledField("Expiration Date", "ccmonth", date.c_str(),
                        FormControlType::kInputMonth,
                        filled_form.fields[offset + 2]);
    } else {
      ExpectFilledField(
          "Expiration Date", "ccmonth", card_fill_data->expiration_month,
          FormControlType::kInputText, filled_form.fields[offset + 2]);
      ExpectFilledField("", "ccyear", card_fill_data->expiration_year,
                        FormControlType::kInputText,
                        filled_form.fields[offset + 3]);
    }
  }
}

void ExpectFilledAddressFormElvis(const FormData& filled_form,
                                  bool has_credit_card_fields) {
  std::optional<TestCardFillData> expected_card_fill_data;
  if (has_credit_card_fields) {
    expected_card_fill_data = kEmptyCardFillData;
  }
  ExpectFilledForm(filled_form, GetElvisAddressFillData(),
                   expected_card_fill_data);
}

void ExpectFilledCreditCardFormElvis(const FormData& filled_form,
                                     bool has_address_fields) {
  std::optional<TestAddressFillData> expected_address_fill_data;
  if (has_address_fields) {
    expected_address_fill_data = kEmptyAddressFillData;
  }
  ExpectFilledForm(filled_form, expected_address_fill_data, kElvisCardFillData);
}

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD((base::flat_set<FieldGlobalId>),
              ApplyFormAction,
              (mojom::ActionType action_type,
               mojom::ActionPersistence action_persistence,
               const FormData& data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, FieldType>&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::ActionPersistence action_persistence,
               mojom::TextReplacement text_replacement,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
  MOCK_METHOD(
      void,
      SendAutofillTypePredictionsToRenderer,
      (const std::vector<vector_experimental_raw_ptr<FormStructure>>& forms),
      (override));
};

MATCHER_P(HasValue, value, "") {
  return arg.value == value;
}

}  // namespace

class FormFillerTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance the mock clock to a fixed, arbitrary, somewhat recent date.
    base::Time year2020;
    ASSERT_TRUE(base::Time::FromString("01/01/20", &year2020));
    task_environment_.FastForwardBy(year2020 - AutofillClock::Now());

    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().set_auto_accept_address_imports_for_testing(true);
    personal_data().SetPrefService(autofill_client_.GetPrefs());
    personal_data().SetSyncServiceForTest(&sync_service_);

    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    autofill_client_.set_test_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            autofill_client_.GetURLLoaderFactory(),
            autofill_client_.GetIdentityManager(), &personal_data()));
    auto credit_card_save_manager = std::make_unique<TestCreditCardSaveManager>(
        autofill_driver_.get(), &autofill_client_, &personal_data());
    credit_card_save_manager->SetCreditCardUploadEnabled(true);
    autofill_client_.set_test_form_data_importer(
        std::make_unique<autofill::TestFormDataImporter>(
            &autofill_client_, std::move(credit_card_save_manager),
            /*iban_save_manager=*/nullptr, &personal_data(), "en-US"));

    ResetBrowserAutofillManager();
    // By default, if we offer single field form fill, suggestions should be
    // returned because it is assumed |field.should_autocomplete| is set to
    // true. This should be overridden in tests where
    // |field.should_autocomplete| is set to false.
    ON_CALL(single_field_form_fill_router(), OnGetSingleFieldSuggestions)
        .WillByDefault(Return(true));

    autofill_client_.set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());

    // Initialize the TestPersonalDataManager with some default data.
    CreateTestAutofillProfiles();
    CreateTestCreditCards();

    // Mandatory re-auth is required for credit card autofill on automotive, so
    // the authenticator response needs to be properly mocked.
#if BUILDFLAG(IS_ANDROID)
    autofill_client_.SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    browser_autofill_manager_.reset();

    personal_data().SetPrefService(nullptr);
    personal_data().ClearCreditCards();
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    browser_autofill_manager_->OnFormsSeen(/*updated_forms=*/forms,
                                           /*removed_forms=*/{});
  }

  void FormSubmitted(const FormData& form) {
    browser_autofill_manager_->OnFormSubmitted(
        form, false, SubmissionSource::FORM_SUBMISSION);
  }

  // TODO(crbug.com/1330108): Have separate functions for profile and credit
  // card filling.
  void FillAutofillFormData(
      const FormData& form,
      const FormFieldData& field,
      std::string guid,
      AutofillTriggerDetails trigger_details = {
          .trigger_source = AutofillTriggerSource::kPopup}) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, {},
        AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown);
    if (const AutofillProfile* profile =
            personal_data().GetProfileByGUID(guid)) {
      browser_autofill_manager_->FillOrPreviewProfileForm(
          mojom::ActionPersistence::kFill, form, field, *profile,
          trigger_details);
    } else if (const CreditCard* card =
                   personal_data().GetCreditCardByGUID(guid)) {
      browser_autofill_manager_->FillOrPreviewCreditCardForm(
          mojom::ActionPersistence::kFill, form, field, *card, trigger_details);
    }
  }

  // Calls |browser_autofill_manager_->OnFillAutofillFormData()| with the
  // specified input parameters after setting up the expectation that the mock
  // driver's |ApplyFormAction()| method will be called and saving the parameter
  // of that call into the |response_data| output parameter.
  FormData FillAutofillFormDataAndGetResults(
      const FormData& input_form,
      const FormFieldData& input_field,
      std::string guid,
      AutofillTriggerDetails trigger_details = {
          .trigger_source = AutofillTriggerSource::kPopup}) {
    FormData response_data;
    std::vector<FieldGlobalId> global_ids;
    for (const auto& field : input_form.fields) {
      global_ids.push_back(field.global_id());
    }
    EXPECT_CALL(*autofill_driver_, ApplyFormAction)
        .WillOnce(DoAll(SaveArg<2>(&response_data), Return(global_ids)));
    FillAutofillFormData(input_form, input_field, guid, trigger_details);
    return response_data;
  }

  FormData PreviewVirtualCardDataAndGetResults(
      mojom::ActionPersistence action_persistence,
      const FormData& input_form,
      const FormFieldData& input_field,
      const CreditCard& virtual_card) {
    FormData response_data;
    EXPECT_CALL(*autofill_driver_, ApplyFormAction)
        .WillOnce((DoAll(SaveArg<2>(&response_data),
                         Return(std::vector<FieldGlobalId>{}))));
    browser_autofill_manager_->FillOrPreviewCreditCardForm(
        action_persistence, input_form, input_field, virtual_card,
        {.trigger_source = AutofillTriggerSource::kPopup});
    return response_data;
  }

  FormData CreateTestCreditCardFormData(bool is_https, bool use_month_type) {
    FormData form;
    CreateTestCreditCardFormData(&form, is_https, use_month_type);
    return form;
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->name = u"MyForm";
    if (is_https) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpsScheme);
      autofill_client_.set_form_origin(
          autofill_client_.form_origin().ReplaceComponents(replacements));
      form->url = GURL("https://myform.com/form.html");
      form->action = GURL("https://myform.com/submit.html");
    } else {
      // If we are testing a form that submits over HTTP, we also need to set
      // the main frame to HTTP, otherwise mixed form warnings will trigger and
      // autofill will be disabled.
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpScheme);
      autofill_client_.set_form_origin(
          autofill_client_.form_origin().ReplaceComponents(replacements));
      form->url = GURL("http://myform.com/form.html");
      form->action = GURL("http://myform.com/submit.html");
    }

    form->fields.push_back(CreateTestFormField("Name on Card", "nameoncard", "",
                                               FormControlType::kInputText));
    form->fields.push_back(CreateTestFormField("Card Number", "cardnumber", "",
                                               FormControlType::kInputText));
    if (use_month_type) {
      form->fields.push_back(CreateTestFormField(
          "Expiration Date", "ccmonth", "", FormControlType::kInputMonth));
    } else {
      form->fields.push_back(CreateTestFormField(
          "Expiration Date", "ccmonth", "", FormControlType::kInputText));
      form->fields.push_back(
          CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
    }
    form->fields.push_back(
        CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  }

  void PrepareForRealPanResponse(FormData* form, CreditCard* card) {
    // This line silences the warning from PaymentsNetworkInterface about
    // matching sync and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    CreateTestCreditCardFormData(form, true, false);
    FormsSeen({*form});
    *card = CreditCard(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card->SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(AtLeast(1));
    browser_autofill_manager_->FillOrPreviewCreditCardForm(
        mojom::ActionPersistence::kFill, *form, form->fields[0], *card,
        {.trigger_source = AutofillTriggerSource::kPopup});
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            .GetCvcAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            .GetCvcAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

  void ResetBrowserAutofillManager() {
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    test_api(*browser_autofill_manager_)
        .set_single_field_form_fill_router(
            std::make_unique<NiceMock<MockSingleFieldFormFillRouter>>(
                autofill_client_.GetMockAutocompleteHistoryManager(),
                autofill_client_.GetMockIbanManager(),
                autofill_client_.GetMockMerchantPromoCodeManager()));
  }

  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

 protected:
  MockSingleFieldFormFillRouter& single_field_form_fill_router() {
    return static_cast<MockSingleFieldFormFillRouter&>(
        test_api(*browser_autofill_manager_).single_field_form_fill_router());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;

 private:
  void CreateTestAutofillProfiles() {
    AutofillProfile profile1 =
        FillDataToAutofillProfile(GetElvisAddressFillData());
    profile1.set_guid(kElvisProfileGuid);
    profile1.set_use_date(AutofillClock::Now() - base::Days(2));
    personal_data().AddProfile(profile1);

    AutofillProfile profile2(
        i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile2.set_guid(MakeGuid(2));
    profile2.set_use_date(AutofillClock::Now() - base::Days(1));
    personal_data().AddProfile(profile2);

    AutofillProfile profile3(
        i18n_model_definition::kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "",
                         "");
    profile3.set_guid(MakeGuid(3));
    profile3.set_use_date(AutofillClock::Now());
    personal_data().AddProfile(profile3);
  }

  void CreateTestCreditCards() {
    CreditCard credit_card1;
    test::SetCreditCardInfo(&credit_card1, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card1.set_guid(MakeGuid(4));
    credit_card1.set_use_count(10);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(5));
    personal_data().AddCreditCard(credit_card1);

    CreditCard credit_card2;
    test::SetCreditCardInfo(&credit_card2, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card2.set_guid(MakeGuid(5));
    credit_card2.set_use_count(5);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(4));
    personal_data().AddCreditCard(credit_card2);

    CreditCard credit_card3;
    test::SetCreditCardInfo(&credit_card3, "", "", "08", "2999", "");
    credit_card3.set_guid(MakeGuid(6));
    personal_data().AddCreditCard(credit_card3);
  }
};

// Tests that only fields from `field_types_to_fill` are filled.
TEST_F(FormFillerTest, FillingDetails_FieldTypesToFill_FillOnlySpecificFields) {
  base::test::ScopedFeatureList enabled_features(
      features::kAutofillGranularFillingAvailable);
  // Set up our form data.
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .autocomplete_attribute = "given-name"},
           {.role = NAME_LAST, .autocomplete_attribute = "family-name"}}});
  FormsSeen({form});
  // Only `NAME_FIRST` fields should be filled.
  FieldTypeSet target_fields = FieldTypeSet({NAME_FIRST});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[0], MakeGuid(1),
      {.trigger_source = AutofillTriggerSource::kPopup,
       .field_types_to_fill = target_fields});

  ASSERT_EQ(response_data.fields.size(), 2u);
  // The city field was filled.
  EXPECT_EQ(response_data.fields[0].name, u"firstName");
  EXPECT_EQ(response_data.fields[0].value,
            UTF8ToUTF16(GetElvisAddressFillData().first));
  // The city field was NOT filled.
  EXPECT_EQ(response_data.fields[1].name, u"lastName");
  EXPECT_EQ(response_data.fields[1].value, u"");
}

// Test that the call is properly forwarded to its SingleFieldFormFillRouter.
TEST_F(FormFillerTest, OnSingleFieldSuggestionSelected) {
  std::u16string test_value = u"TestValue";
  FormData form = CreateTestAddressFormData();
  FormFieldData field = form.fields[0];

  EXPECT_CALL(single_field_form_fill_router(),
              OnSingleFieldSuggestionSelected(test_value,
                                              PopupItemId::kAutocompleteEntry))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, PopupItemId::kAutocompleteEntry, form, field);

  EXPECT_CALL(single_field_form_fill_router(),
              OnSingleFieldSuggestionSelected(test_value,
                                              PopupItemId::kAutocompleteEntry))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, PopupItemId::kAutocompleteEntry, form, field);

  EXPECT_CALL(
      single_field_form_fill_router(),
      OnSingleFieldSuggestionSelected(test_value, PopupItemId::kIbanEntry))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, PopupItemId::kIbanEntry, form, field);

  EXPECT_CALL(single_field_form_fill_router(),
              OnSingleFieldSuggestionSelected(
                  test_value, PopupItemId::kMerchantPromoCodeEntry))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, PopupItemId::kMerchantPromoCodeEntry, form, field);
}

// Test that the correct section is filled.
TEST_F(FormFillerTest, FillTriggeredSection) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  size_t index_of_trigger_field = form.fields.size();
  base::ranges::move(CreateTestAddressFormData().fields,
                     std::back_inserter(form.fields));
  FormsSeen({form});

  // Check that the form has been parsed into two sections.
  ASSERT_NE(form.fields.size(), 0u);
  ASSERT_EQ(index_of_trigger_field, form.fields.size() / 2);
  {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    bool found = browser_autofill_manager_->GetCachedFormAndField(
        form, form.fields[index_of_trigger_field], &form_structure,
        &autofill_field);
    ASSERT_TRUE(found);
    for (size_t i = 0; i < form.fields.size() / 2; ++i) {
      size_t j = form.fields.size() / 2 + i;
      ASSERT_EQ(form_structure->field(i)->name, form_structure->field(j)->name);
      ASSERT_NE(form_structure->field(i)->section,
                form_structure->field(j)->section);
      ASSERT_TRUE(form_structure->field(i)->SameFieldAs(form.fields[j]));
      ASSERT_TRUE(form_structure->field(j)->SameFieldAs(form.fields[i]));
    }
  }

  AutofillProfile* profile = personal_data().GetProfileByGUID(MakeGuid(1));
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[index_of_trigger_field], MakeGuid(1));
  // Extract the sections into individual forms to reduce boiler plate code.
  size_t mid = response_data.fields.size() / 2;
  FormData section1 = response_data;
  FormData section2 = response_data;
  section1.fields.erase(section1.fields.begin() + mid, section1.fields.end());
  section2.fields.erase(section2.fields.begin(), section2.fields.end() - mid);
  // First section should be empty, second should be filled.
  ExpectFilledForm(section1, kEmptyAddressFillData,
                   /*card_fill_data=*/std::nullopt);
  ExpectFilledAddressFormElvis(section2, false);
}

// Test that if the form cache is outdated because a field has changed, filling
// is aborted after that field.
TEST_F(FormFillerTest, DoNotFillIfFormFieldChanged) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify `form` so that it doesn't match `form_structure` anymore.
  ASSERT_GE(form.fields.size(), 3u);
  for (auto it = form.fields.begin() + 2; it != form.fields.end(); ++it) {
    *it = FormFieldData();
  }

  AutofillProfile* profile =
      personal_data().GetProfileByGUID(kElvisProfileGuid);
  ASSERT_TRUE(profile);

  FormData response_data;
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .WillOnce((DoAll(SaveArg<2>(&response_data),
                       Return(std::vector<FieldGlobalId>{}))));
  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(mojom::ActionPersistence::kFill, form,
                                  form.fields.front(), profile, std::nullopt,
                                  form_structure, autofill_field);
  std::vector<FormFieldData> filled_fields(response_data.fields.begin(),
                                           response_data.fields.begin() + 2);
  std::vector<FormFieldData> skipped_fields(response_data.fields.begin() + 2,
                                            response_data.fields.end());

  EXPECT_THAT(filled_fields, Each(Not(HasValue(u""))));
  EXPECT_THAT(skipped_fields, Each(HasValue(u"")));
}

// Test that if the form cache is outdated because the form has changed, filling
// is aborted because of that change.
TEST_F(FormFillerTest, DoNotFillIfFormChanged) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify `form` so that it doesn't match `form_structure` anymore.
  ASSERT_GE(form.fields.size(), 3u);
  form.fields.pop_back();

  AutofillProfile* profile =
      personal_data().GetProfileByGUID(kElvisProfileGuid);
  ASSERT_TRUE(profile);

  FormData response_data;
  EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(0);
  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(mojom::ActionPersistence::kFill, form,
                                  form.fields.front(), profile, std::nullopt,
                                  form_structure, autofill_field);
}

TEST_F(FormFillerTest, SkipFillIfFieldIsMeaningfullyPreFilled) {
  base::test::ScopedFeatureList placeholders_feature{
      features::kAutofillOverwritePlaceholdersOnly};

  const FieldType kSkippedType = ADDRESS_HOME_LINE1;
  FormData form = test::GetFormData(
      {.fields = {{.role = NAME_FIRST, .value = u"Triggering field (filled)"},
                  {.role = NAME_LAST, .value = u"Placeholder (filled)"},
                  {.role = EMAIL_ADDRESS, .value = u"No data (filled)"},
                  {.role = kSkippedType,
                   .value = u"Meaningfully pre-filled (skipped)"}}});
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  form_structure->fields()[0]->set_may_use_prefilled_placeholder(false);
  form_structure->fields()[1]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[3]->set_may_use_prefilled_placeholder(false);

  AutofillProfile* profile =
      personal_data().GetProfileByGUID(kElvisProfileGuid);
  ASSERT_TRUE(profile);

  FormData filled_form;
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .WillOnce((DoAll(SaveArg<2>(&filled_form),
                       Return(std::vector<FieldGlobalId>{}))));
  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(mojom::ActionPersistence::kFill, form,
                                  form.fields.front(), profile, std::nullopt,
                                  form_structure, autofill_field);

  auto expect_hash = [&](const FormFieldData& field,
                         std::optional<size_t> expected_hash) {
    AutofillField* autofill_field = nullptr;
    FormStructure* form_structure = nullptr;
    ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
        form, field, &form_structure, &autofill_field));
    ASSERT_TRUE(autofill_field);
    EXPECT_THAT(
        autofill_field->field_log_events(),
        Contains(VariantWith<FillFieldLogEvent>(Field(
            "value_that_would_have_been_filled_in_a_prefilled_field_hash",
            &FillFieldLogEvent::
                value_that_would_have_been_filled_in_a_prefilled_field_hash,
            testing::Conditional(expected_hash.has_value(),
                                 testing::Optional(expected_hash),
                                 Eq(std::nullopt))))));
  };

  const auto& filled_fields = filled_form.fields;
  EXPECT_TRUE(filled_fields[0].is_autofilled);
  EXPECT_EQ(filled_fields[0].value, profile->GetRawInfo(NAME_FIRST));
  expect_hash(filled_fields[0], std::nullopt);
  EXPECT_TRUE(filled_fields[1].is_autofilled);
  EXPECT_EQ(filled_fields[1].value, profile->GetRawInfo(NAME_LAST));
  expect_hash(filled_fields[1], std::nullopt);
  EXPECT_TRUE(filled_fields[2].is_autofilled);
  EXPECT_EQ(filled_fields[2].value, profile->GetRawInfo(EMAIL_ADDRESS));
  expect_hash(filled_fields[2], std::nullopt);
  EXPECT_FALSE(filled_fields[3].is_autofilled);
  EXPECT_EQ(filled_fields[3].value, form.fields[3].value);
  expect_hash(
      filled_fields[3],
      base::FastHash(base::UTF16ToUTF8(profile->GetRawInfo(kSkippedType))));
}

TEST_F(FormFillerTest, SkipAllPreFilledFieldsExceptIfFieldIsAPlaceholder) {
  base::test::ScopedFeatureList placeholders_features;
  placeholders_features.InitWithFeatures(
      {features::kAutofillOverwritePlaceholdersOnly,
       features::kAutofillSkipPreFilledFields},
      {});

  AutofillProfile* profile =
      personal_data().GetProfileByGUID(kElvisProfileGuid);
  ASSERT_TRUE(profile);
  const std::u16string kToBeFilledState =
      profile->GetRawInfo(ADDRESS_HOME_STATE);
  const std::u16string kSelectedState = u"NC (filled)";
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .value = u"Triggering field (filled)"},
           {.role = NAME_LAST, .value = u"Placeholder (filled)"},
           {.role = EMAIL_ADDRESS, .value = u"No data (skipped)"},
           {.role = ADDRESS_HOME_LINE1, .value = u"No placeholder (skipped)"},
           {.role = ADDRESS_HOME_STATE,
            .value = kSelectedState,
            .form_control_type = FormControlType::kSelectOne,
            .select_options = {SelectOption{.value = kSelectedState,
                                            .content = kSelectedState},
                               SelectOption{.value = kToBeFilledState,
                                            .content = kToBeFilledState}}}}});
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  form_structure->fields()[0]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[1]->set_may_use_prefilled_placeholder(true);
  form_structure->fields()[3]->set_may_use_prefilled_placeholder(false);
  form_structure->fields()[4]->set_may_use_prefilled_placeholder(std::nullopt);

  FormData filled_form;
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .WillOnce((DoAll(SaveArg<2>(&filled_form),
                       Return(std::vector<FieldGlobalId>{}))));
  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(
          mojom::ActionPersistence::kFill, form, form.fields.front(), profile,
          /*cvc=*/std::nullopt, form_structure, autofill_field);

  const auto& filled_fields = filled_form.fields;
  EXPECT_TRUE(filled_fields[0].is_autofilled);
  EXPECT_EQ(filled_fields[0].value, profile->GetRawInfo(NAME_FIRST));
  EXPECT_TRUE(filled_fields[1].is_autofilled);
  EXPECT_EQ(filled_fields[1].value, profile->GetRawInfo(NAME_LAST));
  EXPECT_FALSE(filled_fields[2].is_autofilled);
  EXPECT_EQ(filled_fields[2].value, form.fields[2].value);
  EXPECT_FALSE(filled_fields[3].is_autofilled);
  EXPECT_EQ(filled_fields[3].value, form.fields[3].value);
  EXPECT_TRUE(filled_fields[4].is_autofilled);
  EXPECT_EQ(filled_fields[4].value, kToBeFilledState);
}

TEST_F(FormFillerTest, OnlySkipRefillIfFieldIsPreFilled) {
  base::test::ScopedFeatureList placeholders_features;
  placeholders_features.InitWithFeatures(
      {features::kAutofillOverwritePlaceholdersOnly,
       features::kAutofillSkipPreFilledFields},
      {});

  const std::string kCreditCardGuid = MakeGuid(4);
  TestCardFillData card_fill_data = kElvisCardFillData;
  card_fill_data.card_number = "Pre-filled (skipped)";
  card_fill_data.use_month_type = true;
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true);
  form.fields[1].value = base::UTF8ToUTF16(card_fill_data.card_number);

  FormsSeen({form});
  FormData first_filled_form = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), kCreditCardGuid);

  ExpectFilledForm(first_filled_form, /*address_fill_data=*/std::nullopt,
                   card_fill_data);

  // Prepare intercepting the filling operation to the driver and capture
  // the re-filled form data.
  FormData refilled_form;
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .Times(1)
      .WillOnce(DoAll(SaveArg<2>(&refilled_form),
                      Return(std::vector<FieldGlobalId>{})));

  // Simulate that JavaScript modifies the expiration date field.
  FormData form_after_js_modification = first_filled_form;
  form_after_js_modification.fields[2].value = u"04 / 29";
  browser_autofill_manager_->OnJavaScriptChangedAutofilledValue(
      form_after_js_modification, form_after_js_modification.fields[2],
      u"04/2999");

  testing::Mock::VerifyAndClearExpectations(autofill_driver_.get());

  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley",
                    FormControlType::kInputText, refilled_form.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", card_fill_data.card_number,
                    FormControlType::kInputText, refilled_form.fields[1]);
  ExpectFilledField("Expiration Date", "ccmonth", "04 / 99",
                    FormControlType::kInputMonth, refilled_form.fields[2]);
  ExpectFilledField("CVC", "cvc", "", FormControlType::kInputText,
                    refilled_form.fields[3]);
}

TEST_F(FormFillerTest, UndoSavesFormFillingData) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  FormStructure* form_structure;
  AutofillField* autofill_field;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  base::flat_set<FieldGlobalId> safe_fields{form.fields.front().global_id()};
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .Times(2)
      .WillRepeatedly(Return(safe_fields));

  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(
          mojom::ActionPersistence::kFill, form, form.fields.front(),
          personal_data().GetProfiles().front(), /*optional_cvc=*/std::nullopt,
          form_structure, autofill_field);

  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields.front());
}

TEST_F(FormFillerTest, UndoSavesFieldByFieldFillingData) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  FormStructure* form_structure;
  AutofillField* autofill_field;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  EXPECT_CALL(*autofill_driver_, ApplyFieldAction);
  browser_autofill_manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
      form, form.fields.front(), u"Test Value",
      PopupItemId::kAddressFieldByFieldFilling);

  // Undo early returns if it has no filling history for the trigger field,
  // which is initially empty, therefore calling the driver is proof that data
  // was successfully stored.
  EXPECT_CALL(*autofill_driver_, ApplyFormAction);
  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields.front());
}

TEST_F(FormFillerTest, UndoResetsCachedAutofillState) {
  FormData form = CreateTestAddressFormData();
  AutofillField filled_autofill_field(form.fields.front());

  FormFieldData* field_ptr = &form.fields.front();
  AutofillField* autofill_field_ptr = &filled_autofill_field;
  form.fields.front().is_autofilled = false;
  test_api(*browser_autofill_manager_)
      .AddFormFillEntry(base::make_span(&field_ptr, 1u),
                        base::make_span(&autofill_field_ptr, 1u),
                        FillingProduct::kAddress, /*is_refill=*/false);
  form.fields.front().is_autofilled = true;
  FormsSeen({form});

  FormStructure* form_structure;
  AutofillField* autofill_field;
  base::flat_set<FieldGlobalId> safe_fields{form.fields.front().global_id()};
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(autofill_field->is_autofilled);

  browser_autofill_manager_->UndoAutofill(mojom::ActionPersistence::kFill, form,
                                          form.fields.front());

  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  EXPECT_FALSE(autofill_field->is_autofilled);
}

TEST_F(FormFillerTest, FillOrPreviewDataModelFormCallsDidFillOrPreviewForm) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});
  FormStructure* form_structure;
  AutofillField* autofill_field;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  EXPECT_CALL(autofill_client_, DidFillOrPreviewForm);
  test_api(*browser_autofill_manager_)
      .FillOrPreviewDataModelForm(
          mojom::ActionPersistence::kFill, form, form.fields.front(),
          personal_data().GetCreditCards()[0], /*optional_cvc=*/std::nullopt,
          form_structure, autofill_field);
}

// Test that if the form cache is outdated because a field was removed, filling
// is aborted.
TEST_F(FormFillerTest, DoNotFillIfFormFieldRemoved) {
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify |form| so that it doesn't match |form_structure| anymore.
  ASSERT_GE(form.fields.size(), 2u);
  form.fields.pop_back();

  AutofillProfile* profile = personal_data().GetProfileByGUID(MakeGuid(1));
  ASSERT_TRUE(profile);

  EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(0);
}

// Test that we correctly fill an address form.
TEST_F(FormFillerTest, FillAddressForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  AutofillProfile* profile = personal_data().GetProfileByGUID(MakeGuid(1));
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  const base::Time last_used_date = AutofillClock::Now() - base::Hours(1);
  profile->set_use_date(last_used_date);

  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);

  EXPECT_EQ(2U, profile->use_count());
  EXPECT_LT(last_used_date, profile->use_date());
}

// Tests that `ProfileTokenQuality` is correctly integrated into
// `AutofillProfile` and that on form submit, observations are collected.
TEST_F(FormFillerTest, FillAddressForm_CollectObservations) {
  base::test::ScopedFeatureList profile_token_quality_feature{
      features::kAutofillTrackProfileTokenQuality};
  AutofillProfile* profile =
      personal_data().GetProfileByGUID(kElvisProfileGuid);
  profile->token_quality().disable_randomization_for_testing();

  // Create and fill an address form with profile `kElvisProfileGuid`.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});
  FormData filled_form = FillAutofillFormDataAndGetResults(form, form.fields[0],
                                                           kElvisProfileGuid);

  // Expect that no observations for any of the form's types were collected yet.
  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(filled_form.global_id());
  EXPECT_TRUE(base::ranges::all_of(
      *form_structure, [&](const std::unique_ptr<AutofillField>& field) {
        return profile->token_quality()
            .GetObservationTypesForFieldType(field->Type().GetStorableType())
            .empty();
      }));

  // Submit the form and expect observations for all of the form's types. This
  // updates the `profile` in `personal_data()`, invalidating the pointer.
  FormSubmitted(filled_form);
  profile = personal_data().GetProfileByGUID(kElvisProfileGuid);
  EXPECT_TRUE(base::ranges::none_of(
      *form_structure, [&](const std::unique_ptr<AutofillField>& field) {
        return profile->token_quality()
            .GetObservationTypesForFieldType(field->Type().GetStorableType())
            .empty();
      }));
}

// Tests that for ac=unrecognized fields:
// - Are not filled by default.
// - Are filled through manual fallbacks.
TEST_F(FormFillerTest, AutocompleteUnrecognizedFillingBehavior) {
  // Create a form where the middle name field has ac=unrecognized.
  FormData form = CreateTestAddressFormData();
  ASSERT_EQ(form.fields[1].name, u"middlename");
  form.fields[1].parsed_autocomplete =
      AutocompleteParsingResult{.field_type = HtmlFieldType::kUnrecognized};
  FormsSeen({form});

  // Fill the `form` regularly and expect that everything but the middle name
  // gets filled.
  FormData filled_form = FillAutofillFormDataAndGetResults(form, form.fields[0],
                                                           kElvisProfileGuid);
  TestAddressFillData fill_data = GetElvisAddressFillData();
  fill_data.middle = "";
  ExpectFilledForm(filled_form, fill_data, /*card_fill_data=*/std::nullopt);

  // Fill the `form` as-if through manual fallbacks. Expect that every field
  // gets filled.
  EXPECT_CALL(*autofill_driver_, ApplyFormAction)
      .WillOnce(DoAll(SaveArg<2>(&filled_form),
                      Return(base::flat_set<FieldGlobalId>{})));
  browser_autofill_manager_->FillOrPreviewProfileForm(
      mojom::ActionPersistence::kFill, form, form.fields[0],
      *personal_data().GetProfileByGUID(kElvisProfileGuid),
      {.trigger_source = AutofillTriggerSource::kManualFallback});

  ExpectFilledForm(filled_form, GetElvisAddressFillData(),
                   /*card_fill_data=*/std::nullopt);
}

// Test that we correctly log FIELD_WAS_AUTOFILLED event in UserHappiness.
TEST_F(FormFillerTest, FillCreditCardForm_LogFieldWasAutofill) {
  // Set up our form data.
  FormData form;
  // Construct a form with a 4 fields: cardholder name, card number,
  // expiration date and cvc.
  CreateTestCreditCardFormData(&form, true, true);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  // Cardholder name, card number, expiration data were autofilled but cvc was
  // not be autofilled.
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::FIELD_WAS_AUTOFILLED, 3);
}

// Test that we correctly fill a credit card form.
TEST_F(FormFillerTest, FillCreditCardForm_Simple) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/false);
}

// Test that whitespace is stripped from the credit card number.
TEST_F(FormFillerTest, FillCreditCardForm_StripCardNumberWhitespace) {
  // Same as the SetUp(), but generate Elvis card with whitespace in credit
  // card number.  |credit_card| will be owned by the TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid(MakeGuid(8));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(8));
  ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/false);
}

// Test that separator characters are stripped from the credit card number.
TEST_F(FormFillerTest, FillCreditCardForm_StripCardNumberSeparators) {
  // Same as the SetUp(), but generate Elvis card with separator characters in
  // credit card number.  |credit_card| will be owned by the
  // TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234-5678-9012-3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid(MakeGuid(9));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(9));
  ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/false);
}

// Test that we correctly fill a credit card form with month input type.
// Test 1 of 4: Empty month, empty year
TEST_F(FormFillerTest, FillCreditCardForm_NoYearNoMonth) {
  personal_data().ClearCreditCards();

  TestCardFillData card_fill_data = kElvisCardFillData;
  card_fill_data.expiration_month = "";
  card_fill_data.expiration_year = "";
  card_fill_data.use_month_type = true;
  CreditCard credit_card = FillDataToCreditCardInfo(card_fill_data);

  credit_card.set_guid(MakeGuid(7));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(7));
  ExpectFilledForm(response_data, /*address_fill_data=*/std::nullopt,
                   card_fill_data);
}

// Test that we correctly fill a credit card form with month input type.
// Test 2 of 4: Non-empty month, empty year
TEST_F(FormFillerTest, FillCreditCardForm_NoYearMonth) {
  personal_data().ClearCreditCards();
  TestCardFillData card_fill_data = kElvisCardFillData;
  card_fill_data.expiration_month = "04";
  card_fill_data.expiration_year = "";
  card_fill_data.use_month_type = true;
  CreditCard credit_card = FillDataToCreditCardInfo(card_fill_data);

  credit_card.set_guid(MakeGuid(7));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(7));
  ExpectFilledForm(response_data, /*address_fill_data=*/std::nullopt,
                   card_fill_data);
}

// Test that we correctly fill a credit card form with month input type.
// Test 3 of 4: Empty month, non-empty year
TEST_F(FormFillerTest, FillCreditCardForm_YearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data().ClearCreditCards();
  TestCardFillData card_fill_data = kElvisCardFillData;
  card_fill_data.expiration_month = "";
  card_fill_data.expiration_year = "2999";
  card_fill_data.use_month_type = true;
  CreditCard credit_card = FillDataToCreditCardInfo(card_fill_data);
  credit_card.set_guid(MakeGuid(7));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(7));
  ExpectFilledForm(response_data, /*address_fill_data=*/std::nullopt,
                   card_fill_data);
}

// Test that we correctly fill a credit card form with month input type.
// Test 4 of 4: Non-empty month, non-empty year
TEST_F(FormFillerTest, FillCreditCardForm_YearMonth) {
  personal_data().ClearCreditCards();
  TestCardFillData card_fill_data = kElvisCardFillData;
  card_fill_data.expiration_month = "04";
  card_fill_data.expiration_year = "2999";
  card_fill_data.use_month_type = true;
  CreditCard credit_card = FillDataToCreditCardInfo(card_fill_data);
  credit_card.set_guid(MakeGuid(7));
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/true);
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(7));
  ExpectFilledForm(response_data, /*address_fill_data=*/std::nullopt,
                   card_fill_data);
}

// Test that only the first 16 credit card number fields are filled.
TEST_F(FormFillerTest, FillOnlyFirstNineteenCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("Card Name", "cardname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last Name", "cardlastname", "",
                                     FormControlType::kInputText)};

  // Add 20 credit card number fields with distinct names.
  for (int i = 0; i < 20; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    form.fields.push_back(
        CreateTestFormField(base::UTF16ToASCII(field_name).c_str(),
                            "cardnumber", "", FormControlType::kInputText));
  }

  form.fields.push_back(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ExpectFilledField("Card Name", "cardname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);

  // Verify that the first 19 credit card number fields are filled.
  for (int i = 0; i < 19; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    ExpectFilledField(base::UTF16ToASCII(field_name).c_str(), "cardnumber",
                      "4234567890123456", FormControlType::kInputText,
                      response_data.fields[2 + i]);
  }

  // Verify that the 20th. credit card number field is not filled.
  ExpectFilledField("Card Number 20", "cardnumber", "",
                    FormControlType::kInputText, response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", FormControlType::kInputText,
                    response_data.fields[22]);
}

// Test that only the first 16 of identical fields are filled.
TEST_F(FormFillerTest, FillOnlyFirstSixteenIdenticalCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("Card Name", "cardname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last Name", "cardlastname", "",
                                     FormControlType::kInputText)};

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    form.fields.push_back(CreateTestFormField("Card Number", "cardnumber", "",
                                              FormControlType::kInputText));
  }
  form.fields.push_back(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ExpectFilledField("Card Name", "cardname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  for (int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456",
                      FormControlType::kInputText, response_data.fields[2 + i]);
  }
  // Verify that the 20th. card number field is not filled.
  ExpectFilledField("Card Number", "cardnumber", "",
                    FormControlType::kInputText, response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", FormControlType::kInputText,
                    response_data.fields[22]);
}

// Test the credit card number is filled correctly into single-digit fields.
TEST_F(FormFillerTest, FillCreditCardNumberIntoSingleDigitFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("Card Name", "cardname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last Name", "cardlastname", "",
                                     FormControlType::kInputText)};

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    FormFieldData field = CreateTestFormField("Card Number", "cardnumber", "",
                                              FormControlType::kInputText);
    field.host_frame = form.host_frame;
    field.host_form_id = form.renderer_id;
    field.max_length = i < 19 ? 1 : std::numeric_limits<int>::max();
    form.fields.push_back(std::move(field));
  }
  form.fields.push_back(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ExpectFilledField("Card Name", "cardname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  std::u16string card_number = u"4234567890123456";
  for (unsigned int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber",
                      i < card_number.length()
                          ? base::UTF16ToASCII(card_number.substr(i, 1)).c_str()
                          : "4234567890123456",
                      FormControlType::kInputText, response_data.fields[2 + i]);
  }

  // Verify that the 20th. card number field is contains the full value.
  ExpectFilledField("Card Number", "cardnumber", "",
                    FormControlType::kInputText, response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", FormControlType::kInputText,
                    response_data.fields[22]);
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_F(FormFillerTest, FillCreditCardForm_SplitName) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {
      CreateTestFormField("Card Name", "cardname", "",
                          FormControlType::kInputText),
      CreateTestFormField("Last Name", "cardlastname", "",
                          FormControlType::kInputText),
      CreateTestFormField("Card Number", "cardnumber", "",
                          FormControlType::kInputText),
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText)};

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ExpectFilledField("Card Name", "cardname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456",
                    FormControlType::kInputText, response_data.fields[2]);
}

// Test that only filled selection boxes are counted for the type filling limit.
TEST_F(FormFillerTest, OnlyCountFilledSelectionBoxesForTypeFillingLimit) {
  test::PopulateAlternativeStateNameMapForTesting(
      "US", "Tennessee",
      {{.canonical_name = "Tennessee",
        .abbreviations = {"TN"},
        .alternative_names = {}}});
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("First Name", "firstname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Middle Name", "middlename", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last Name", "lastname", "",
                                     FormControlType::kInputText),

                 // Create a selection box for the state that hat the correct
                 // entry to be filled with user data. Note, TN is the official
                 // abbreviation for Tennessee.
                 CreateTestSelectField("State", "state", "", {"AA", "BB", "TN"},
                                       {"AA", "BB", "TN"})};

  // Add 20 selection boxes that can not be filled since the correct entry
  // is missing.
  for (int i = 0; i < 20; i++) {
    form.fields.push_back(CreateTestSelectField(
        "State", "state", "", {"AA", "BB", "CC"}, {"AA", "BB", "CC"}));
  }

  // Add 20 other selection boxes that should be fillable since the correct
  // entry is present.
  for (int i = 0; i < 20; i++) {
    form.fields.push_back(CreateTestSelectField(
        "State", "state", "", {"AA", "BB", "TN"}, {"AA", "BB", "TN"}));
  }

  // Create a selection box for the state that hat the correct entry to be
  // filled with user data. Note, TN is the official abbreviation for Tennessee.
  for (int i = 0; i < 20; ++i) {
    form.fields.push_back(CreateTestSelectField(
        "Country", "country", "", {"DE", "FR", "US"}, {"DE", "FR", "US"}));
  }

  FormsSeen({form});

  TestAddressFillData profile_info_data = GetElvisAddressFillData();
  profile_info_data.company = "1987";
  AutofillProfile profile = FillDataToAutofillProfile(profile_info_data);
  profile.set_guid(MakeGuid(123));
  personal_data().AddProfile(profile);

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(123));

  // Verify the correct filling of the name entries.
  ExpectFilledField("First Name", "firstname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Middle Name", "middlename", "Aaron",
                    FormControlType::kInputText, response_data.fields[1]);
  ExpectFilledField("Last Name", "lastname", "Presley",
                    FormControlType::kInputText, response_data.fields[2]);

  // Verify that the first selection box is correctly filled.
  ExpectFilledField("State", "state", "TN", FormControlType::kSelectOne,
                    response_data.fields[3]);

  // Verify that the next 20 selection boxes are not filled.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "", FormControlType::kSelectOne,
                      response_data.fields[4 + i]);
  }

  // Verify that the remaining selection boxes are correctly filled again
  // because there's no limit on filling ADDRESS_HOME_STATE fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "TN", FormControlType::kSelectOne,
                      response_data.fields[24 + i]);
  }

  // Verify that only the first 9 of the remaining selection boxes are
  // correctly filled due to the limit on filling ADDRESS_HOME_COUNTRY fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("Country", "country", i < 9 ? "US" : "",
                      FormControlType::kSelectOne,
                      response_data.fields[44 + i]);
  }
}

// Test that we correctly fill a combined address and credit card form.
TEST_F(FormFillerTest, FillAddressAndCreditCardForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  FormData response_data;
  {
    SCOPED_TRACE("Address");
    response_data =
        FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
    ExpectFilledAddressFormElvis(response_data, true);
  }

  // Now fill the credit card data.
  {
    response_data = FillAutofillFormDataAndGetResults(form, form.fields.back(),
                                                      MakeGuid(4));
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/true);
  }
}

// Test parameter data for tests with a simple structure: Create a form,
// autofill it, check that values have been correctly filled.
struct FormFillerSimpleFormTestCase {
  struct FormFieldExpectedData {
    const char* label;
    const char* name;
    const char* value = "";
  };

  const std::string test_name;
  const std::string cc_guid = "";
  const std::string profile_guid = MakeGuid(1);

  const test::FormDescription form_description;
  const std::vector<FormFieldExpectedData> expected_form_fields;
};

class FormFillerSimpleFormTest
    : public FormFillerTest,
      public ::testing::WithParamInterface<FormFillerSimpleFormTestCase> {};

const FormFillerSimpleFormTestCase kFormFillerSimpleFormTestCases[] = {
    // Test that a field with an unrecognized autocomplete attribute is not
    // filled.
    {.test_name = "FillAddressForm_UnrecognizedAttribute",
     .form_description =
         {.fields = {{.label = u"First name",
                      .name = u"firstname",
                      .autocomplete_attribute = "given-name"},
                     {.label = u"Middle name", .name = u"middle"},
                     {.label = u"Last name",
                      .name = u"lastname",
                      .autocomplete_attribute = "unrecognized"}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname"}}},

    // Test that non credit card related fields with the autocomplete attribute
    // set to off are filled on all platforms when the feature to autofill all
    // addresses is enabled (default).
    {.test_name = "FillAddressForm_AutocompleteOffNotRespected",
     .form_description =
         {.fields = {{.label = u"First name", .name = u"firstname"},
                     {.label = u"Middle name",
                      .name = u"middle",
                      .should_autocomplete = false},
                     {.label = u"Last name", .name = u"lastname"},
                     {.label = u"Address Line 1",
                      .name = u"addr1",
                      .should_autocomplete = false}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname", .value = "Presley"},
          {.label = "Address Line 1",
           .name = "addr1",
           .value = "3734 Elvis Presley Blvd."}}},

    // Test that a field with a value equal to it's placeholder attribute is
    // filled.
    {.test_name = "FillAddressForm_PlaceholderEqualsValue",
     .form_description = {.fields = {{.label = u"First name",
                                      .name = u"firstname",
                                      .value = u"First Name",
                                      .placeholder = u"First Name"},
                                     {.label = u"Middle name",
                                      .name = u"middle",
                                      .value = u"Middle Name",
                                      .placeholder = u"Middle Name"},
                                     {.label = u"Last name",
                                      .name = u"lastname",
                                      .value = u"Last Name",
                                      .placeholder = u"Last Name"}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname", .value = "Presley"}}},

    // Test that a credit card field with an unrecognized autocomplete attribute
    // gets filled.
    {.test_name = "FillCreditCardForm_UnrecognizedAttribute",
     .cc_guid = MakeGuid(4),
     .profile_guid = "",
     .form_description =
         {.fields = {{.label = u"Name on Card",
                      .name = u"nameoncard",
                      .autocomplete_attribute = "cc-name"},
                     {.label = u"Card Number", .name = u"cardnumber"},
                     {.label = u"Expiration Date",
                      .name = u"ccmonth",
                      .autocomplete_attribute = "unrecognized"}}},
     .expected_form_fields = {{.label = "Name on Card",
                               .name = "nameoncard",
                               .value = "Elvis Presley"},
                              {.label = "Card Number",
                               .name = "cardnumber",
                               .value = "4234567890123456"},
                              {.label = "Expiration Date",
                               .name = "ccmonth",
                               .value = "04/2999"}}},

};

TEST_P(FormFillerSimpleFormTest, FillSimpleForm) {
  const FormFillerSimpleFormTestCase& params = GetParam();
  FormData form = test::GetFormData(params.form_description);
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[0],
      params.cc_guid.empty() ? params.profile_guid : params.cc_guid);

  ASSERT_EQ(response_data.fields.size(), params.expected_form_fields.size());
  for (size_t i = 0; i < response_data.fields.size(); ++i) {
    SCOPED_TRACE(params.test_name + ", fields expectations");
    const auto& [label, name, value] = params.expected_form_fields[i];
    ExpectFilledField(label, name, value, FormControlType::kInputText,
                      response_data.fields[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FormFillerTest,
    FormFillerSimpleFormTest,
    ::testing::ValuesIn(kFormFillerSimpleFormTestCases),
    [](const ::testing::TestParamInfo<FormFillerSimpleFormTest::ParamType>&
           info) { return info.param.test_name; });

// Test that credit card fields are filled even if they have the autocomplete
// attribute set to off.
TEST_F(FormFillerTest, FillCreditCardForm_AutocompleteOff) {
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);

  // Set the autocomplete=off on all fields.
  for (FormFieldData field : form.fields) {
    field.should_autocomplete = false;
  }

  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));

  // All fields should be filled.
  ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/false);
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_F(FormFillerTest, FillCreditCardForm_ExpiredCard) {
  personal_data().ClearCreditCards();
  CreditCard expired_card;
  test::SetCreditCardInfo(&expired_card, "Homer Simpson",
                          "4234567890654321",  // Visa
                          "05", "2000", "1");
  expired_card.set_guid(MakeGuid(9));
  personal_data().AddCreditCard(expired_card);

  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  // Create a credit card form.
  std::vector<const char*> kCreditCardTypes = {"Visa", "Mastercard", "AmEx",
                                               "discover"};
  form.fields = {
      CreateTestFormField("Name on Card", "nameoncard", "",
                          FormControlType::kInputText, "cc-name"),
      CreateTestSelectField("Card Type", "cardtype", "", "cc-type",
                            kCreditCardTypes, kCreditCardTypes),
      CreateTestFormField("Card Number", "cardnumber", "",
                          FormControlType::kInputText, "cc-number"),
      CreateTestFormField("Expiration Month", "ccmonth", "",
                          FormControlType::kInputText, "cc-exp-month"),
      CreateTestFormField("Expiration Year", "ccyear", "",
                          FormControlType::kInputText, "cc-exp-year")};
  FormsSeen({form});

  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(9));

  // The credit card name, type and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Homer Simpson",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Card Type", "cardtype", "Visa",
                    FormControlType::kSelectOne, response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890654321",
                    FormControlType::kInputText, response_data.fields[2]);

  // The expiration month and year should not be filled.
  ExpectFilledField("Expiration Month", "ccmonth", "",
                    FormControlType::kInputText, response_data.fields[3]);
  ExpectFilledField("Expiration Year", "ccyear", "",
                    FormControlType::kInputText, response_data.fields[4]);
}

TEST_F(FormFillerTest, PreviewCreditCardForm_VirtualCard) {
  personal_data().ClearCreditCards();
  CreditCard virtual_card = test::GetVirtualCard();
  personal_data().AddServerCreditCard(virtual_card);
  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  FormData response_data = PreviewVirtualCardDataAndGetResults(
      mojom::ActionPersistence::kPreview, form, form.fields[1], virtual_card);

  std::u16string expected_cardholder_name = u"Lorem Ipsum";
  // Virtual card number using obfuscated dots only: Virtual card Mastercard
  // ••••4444
  std::u16string expected_card_number =
      u"Virtual card Mastercard  " +
      virtual_card.ObfuscatedNumberWithVisibleLastFourDigits();
  // Virtual card expiration month using obfuscated dots: ••
  std::u16string expected_exp_month =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/2);
  // Virtual card expiration year using obfuscated dots: ••••
  std::u16string expected_exp_year =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/4);
  // Virtual card cvc using obfuscated dots: •••
  std::u16string expected_cvc =
      CreditCard::GetMidlineEllipsisPlainDots(/*num_dots=*/3);

  EXPECT_EQ(response_data.fields[0].value, expected_cardholder_name);
  EXPECT_EQ(response_data.fields[1].value, expected_card_number);
  EXPECT_EQ(response_data.fields[2].value, expected_exp_month);
  EXPECT_EQ(response_data.fields[3].value, expected_exp_year);
  EXPECT_EQ(response_data.fields[4].value, expected_cvc);
}

// Test that unfocusable fields aren't filled, except for <select> fields (but
// not <selectlist> fields).
TEST_F(FormFillerTest, DoNotFillUnfocusableFieldsExceptForSelect) {
  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {
      CreateTestFormField("First Name", "firstname", "",
                          FormControlType::kInputText),
      CreateTestFormField("", "lastname", "", FormControlType::kInputText),
      CreateTestFormField("Postal Code", "postal_code", "",
                          FormControlType::kInputText)};
  form.fields.back().is_focusable = false;

  form.fields.push_back(CreateTestSelectOrSelectListField(
      "Country", "country", "", "", {"CA", "US"}, {"Canada", "United States"},
      FormControlType::kSelectList));
  form.fields.back().is_focusable = false;

  form.fields.push_back(CreateTestSelectOrSelectListField(
      "Country", "country", "", "", {"CA", "US"}, {"Canada", "United States"},
      FormControlType::kSelectList));
  form.fields.push_back(CreateTestSelectOrSelectListField(
      "Country", "country", "", "", {"CA", "US"}, {"Canada", "United States"},
      FormControlType::kSelectOne));
  form.fields.back().is_focusable = false;

  FormsSeen({form});

  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));

  ASSERT_EQ(6U, response_data.fields.size());
  ExpectFilledField("First Name", "firstname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("", "lastname", "Presley", FormControlType::kInputText,
                    response_data.fields[1]);
  ExpectFilledField("Postal Code", "postal_code", "",
                    FormControlType::kInputText, response_data.fields[2]);
  ExpectFilledField("Country", "country", "", FormControlType::kSelectList,
                    response_data.fields[3]);
  ExpectFilledField("Country", "country", "US", FormControlType::kSelectList,
                    response_data.fields[4]);
  ExpectFilledField("Country", "country", "US", FormControlType::kSelectOne,
                    response_data.fields[5]);
}

// Test that non-focusable field is ignored while inferring boundaries between
// sections, but not filled.
TEST_F(FormFillerTest, FillFormWithNonFocusableFields) {
  bool default_to_city_and_number =
      base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber);

  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {
      CreateTestFormField("First Name", "firstname", "",
                          FormControlType::kInputText),
      CreateTestFormField("", "lastname", "", FormControlType::kInputText),
      CreateTestFormField("", "email", "", FormControlType::kInputText),
      CreateTestFormField("Phone Number", "phonenumber", "",
                          FormControlType::kInputTelephone),
      CreateTestFormField("", "email_", "", FormControlType::kInputText)};
  form.fields.back().is_focusable = false;
  form.fields.push_back(CreateTestFormField("Country", "country", "",
                                            FormControlType::kInputText));

  FormsSeen({form});

  // Fill the form
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));

  // All the visible fields should be filled as all the fields belong to the
  // same logical section.
  ASSERT_EQ(6U, response_data.fields.size());
  ExpectFilledField("First Name", "firstname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("", "lastname", "Presley", FormControlType::kInputText,
                    response_data.fields[1]);
  ExpectFilledField("", "email", "theking@gmail.com",
                    FormControlType::kInputText, response_data.fields[2]);
  ExpectFilledField("Phone Number", "phonenumber",
                    default_to_city_and_number ? "2345678901" : "12345678901",
                    FormControlType::kInputTelephone, response_data.fields[3]);
  ExpectFilledField("", "email_", "", FormControlType::kInputText,
                    response_data.fields[4]);
  ExpectFilledField("Country", "country", "United States",
                    FormControlType::kInputText, response_data.fields[5]);
}

// Test that we correctly fill a form that has multiple logical sections, e.g.
// both a billing and a shipping address.
TEST_F(FormFillerTest, FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  const size_t kAddressFormSize = form.fields.size();
  base::ranges::move(CreateTestAddressFormData().fields,
                     std::back_inserter(form.fields));
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + u"_";
  }
  FormsSeen({form});

  // Fill the first section.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(std::u16string(), response_data.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    response_data.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(response_data, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  ASSERT_LT(9U, kAddressFormSize);
  response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[kAddressFormSize + 9], MakeGuid(1));
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(response_data.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      EXPECT_EQ(std::u16string(), response_data.fields[i].value);
    }

    // The second address section should be filled with Elvis's data.
    FormData secondSection = response_data;
    secondSection.fields.erase(secondSection.fields.begin(),
                               secondSection.fields.begin() + kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      // Restore the expected field names.
      std::u16string name = secondSection.fields[i].name;
      std::u16string original_name = name.substr(0, name.size() - 1);
      secondSection.fields[i].name = original_name;
    }
    ExpectFilledAddressFormElvis(secondSection, false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(FormFillerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {
      CreateTestFormField("", "country", "", FormControlType::kInputText,
                          "section-billing country"),
      CreateTestFormField("", "firstname", "", FormControlType::kInputText,
                          "given-name"),
      CreateTestFormField("", "lastname", "", FormControlType::kInputText,
                          "family-name"),
      CreateTestFormField("", "address", "", FormControlType::kInputText,
                          "section-billing address-line1"),
      CreateTestFormField("", "city", "", FormControlType::kInputText,
                          "section-billing locality"),
      CreateTestFormField("", "state", "", FormControlType::kInputText,
                          "section-billing region"),
      CreateTestFormField("", "zip", "", FormControlType::kInputText,
                          "section-billing postal-code"),
      CreateTestFormField("", "ccname", "", FormControlType::kInputText,
                          "section-billing cc-name"),
      CreateTestFormField("", "ccnumber", "", FormControlType::kInputText,
                          "section-billing cc-number"),
      CreateTestFormField("", "ccexp", "", FormControlType::kInputText,
                          "section-billing cc-exp"),
      CreateTestFormField("", "email", "", FormControlType::kInputText,
                          "email")};

  FormsSeen({form});

  // Fill the unnamed section.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[1], MakeGuid(1));
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", FormControlType::kInputText,
                      response_data.fields[0]);
    ExpectFilledField("", "firstname", "Elvis", FormControlType::kInputText,
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "Presley", FormControlType::kInputText,
                      response_data.fields[2]);
    ExpectFilledField("", "address", "", FormControlType::kInputText,
                      response_data.fields[3]);
    ExpectFilledField("", "city", "", FormControlType::kInputText,
                      response_data.fields[4]);
    ExpectFilledField("", "state", "", FormControlType::kInputText,
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "", FormControlType::kInputText,
                      response_data.fields[6]);
    ExpectFilledField("", "ccname", "", FormControlType::kInputText,
                      response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", FormControlType::kInputText,
                      response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", FormControlType::kInputText,
                      response_data.fields[9]);
    ExpectFilledField("", "email", "theking@gmail.com",
                      FormControlType::kInputText, response_data.fields[10]);
  }

  // Fill the address portion of the billing section.
  response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "US", FormControlType::kInputText,
                      response_data.fields[0]);
    ExpectFilledField("", "firstname", "", FormControlType::kInputText,
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "", FormControlType::kInputText,
                      response_data.fields[2]);
    ExpectFilledField("", "address", "3734 Elvis Presley Blvd.",
                      FormControlType::kInputText, response_data.fields[3]);
    ExpectFilledField("", "city", "Memphis", FormControlType::kInputText,
                      response_data.fields[4]);
    ExpectFilledField("", "state", "Tennessee", FormControlType::kInputText,
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "38116", FormControlType::kInputText,
                      response_data.fields[6]);
    ExpectFilledField("", "ccname", "", FormControlType::kInputText,
                      response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", FormControlType::kInputText,
                      response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", FormControlType::kInputText,
                      response_data.fields[9]);
    ExpectFilledField("", "email", "", FormControlType::kInputText,
                      response_data.fields[10]);
  }

  // Fill the credit card portion of the billing section.
  response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[form.fields.size() - 2], MakeGuid(4));
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", FormControlType::kInputText,
                      response_data.fields[0]);
    ExpectFilledField("", "firstname", "", FormControlType::kInputText,
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "", FormControlType::kInputText,
                      response_data.fields[2]);
    ExpectFilledField("", "address", "", FormControlType::kInputText,
                      response_data.fields[3]);
    ExpectFilledField("", "city", "", FormControlType::kInputText,
                      response_data.fields[4]);
    ExpectFilledField("", "state", "", FormControlType::kInputText,
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "", FormControlType::kInputText,
                      response_data.fields[6]);
    ExpectFilledField("", "ccname", "Elvis Presley",
                      FormControlType::kInputText, response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "4234567890123456",
                      FormControlType::kInputText, response_data.fields[8]);
    ExpectFilledField("", "ccexp", "04/2999", FormControlType::kInputText,
                      response_data.fields[9]);
    ExpectFilledField("", "email", "", FormControlType::kInputText,
                      response_data.fields[10]);
  }
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(FormFillerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  form.fields.push_back(CreateTestFormField("Confirm email", "email2", "",
                                            FormControlType::kInputText));

  FormsSeen({form});

  // Fill the form.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));

  // The second email address should be filled.
  EXPECT_EQ(u"theking@gmail.com", response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(FormFillerTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  // Mark the address fields as autofilled.
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(1));
  {
    SCOPED_TRACE("Address");
    TestAddressFillData expected_address_fill_data = kEmptyAddressFillData;
    expected_address_fill_data.first = "Elvis";
    ExpectFilledForm(response_data, expected_address_fill_data,
                     kEmptyCardFillData);
  }

  // Now fill the credit card data.
  response_data =
      FillAutofillFormDataAndGetResults(form, form.fields.back(), MakeGuid(4));
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  response_data = FillAutofillFormDataAndGetResults(
      form, form.fields[form.fields.size() - 2], MakeGuid(4));
  {
    SCOPED_TRACE("Credit card 2");
    TestCardFillData expected_card_fill_data = kEmptyCardFillData;
    expected_card_fill_data.expiration_year = "2999";
    ExpectFilledForm(response_data, kEmptyAddressFillData,
                     expected_card_fill_data);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_F(FormFillerTest, FillPartlyAutofilledForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  // Mark couple of the address fields as autofilled.
  form.fields[3].is_autofilled = true;
  form.fields[4].is_autofilled = true;
  form.fields[5].is_autofilled = true;
  form.fields[6].is_autofilled = true;
  form.fields[10].is_autofilled = true;

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(1));
  {
    SCOPED_TRACE("Address");
    TestAddressFillData expected_address_fill_data = kEmptyAddressFillData;
    expected_address_fill_data.first = "Elvis";
    expected_address_fill_data.middle = "Aaron";
    expected_address_fill_data.last = "Presley";
    expected_address_fill_data.postal_code = "38116";
    expected_address_fill_data.country = "United States";
    bool default_to_city_and_number =
        base::FeatureList::IsEnabled(features::kAutofillDefaultToCityAndNumber);
    expected_address_fill_data.phone =
        default_to_city_and_number ? "2345678901" : "12345678901";

    ExpectFilledForm(response_data, expected_address_fill_data,
                     kEmptyCardFillData);
  }

  // Now fill the credit card data.
  response_data =
      FillAutofillFormDataAndGetResults(form, form.fields.back(), MakeGuid(4));
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_data, /*has_address_fields=*/true);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_F(FormFillerTest, FillPartlyManuallyFilledForm) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  form.fields[0].value = u"Michael";
  form.fields[0].properties_mask |= kUserTyped;

  // Jackson will be preserved.
  form.fields[2].value = u"Jackson";
  form.fields[2].properties_mask |= kUserTyped;

  FormsSeen({form});

  // First fill the address data.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(1));
  {
    SCOPED_TRACE("Address");
    TestAddressFillData expected_address_fill_data = GetElvisAddressFillData();
    expected_address_fill_data.last = "Jackson";
    ExpectFilledForm(response_data, expected_address_fill_data,
                     kEmptyCardFillData);
  }

  // Now fill the credit card data.
  response_data =
      FillAutofillFormDataAndGetResults(form, form.fields.back(), MakeGuid(4));
  {
    SCOPED_TRACE("Credit card 1");
    TestAddressFillData expected_address_fill_data = kEmptyAddressFillData;
    expected_address_fill_data.first = "Michael";
    expected_address_fill_data.last = "Jackson";
    ExpectFilledForm(response_data, expected_address_fill_data,
                     kElvisCardFillData);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(FormFillerTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.renderer_id = test::MakeFormRendererId();
  form_with_us_number_max_length.name = u"MyMaxlengthPhoneForm";
  form_with_us_number_max_length.url =
      GURL("https://myform.com/phone_form.html");
  form_with_us_number_max_length.action =
      GURL("https://myform.com/phone_submit.html");
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.renderer_id = test::MakeFormRendererId();
  form_with_autocompletetype.name = u"MyAutocompletetypePhoneForm";

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

  constexpr uint64_t default_max_length = 0;
  for (const auto& test_field : test_fields) {
    FormFieldData field = CreateTestFormField(test_field.label, test_field.name,
                                              "", FormControlType::kInputText,
                                              "", test_field.max_length);
    form_with_us_number_max_length.fields.push_back(field);

    field.max_length = default_max_length;
    field.autocomplete_attribute = test_field.autocomplete_attribute;
    field.parsed_autocomplete =
        ParseAutocompleteAttribute(test_field.autocomplete_attribute);
    form_with_autocompletetype.fields.push_back(field);
  }

  FormsSeen({form_with_us_number_max_length, form_with_autocompletetype});

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();
  FormData response_data1 = FillAutofillFormDataAndGetResults(
      form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(), guid);

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(u"1", response_data1.fields[0].value);
  EXPECT_EQ(u"650", response_data1.fields[1].value);
  EXPECT_EQ(u"555", response_data1.fields[2].value);
  EXPECT_EQ(u"4567", response_data1.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data1.fields[4].value);

  FormData response_data2 = FillAutofillFormDataAndGetResults(
      form_with_autocompletetype, *form_with_autocompletetype.fields.begin(),
      guid);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(u"1", response_data2.fields[0].value);
  EXPECT_EQ(u"650", response_data2.fields[1].value);
  EXPECT_EQ(u"555", response_data2.fields[2].value);
  EXPECT_EQ(u"4567", response_data2.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data2.fields[4].value);

  // For other countries, fill prefix and suffix fields with best effort.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  FormData response_data3 = FillAutofillFormDataAndGetResults(
      form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(), guid);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(u"4", response_data3.fields[0].value);
  EXPECT_EQ(u"700", response_data3.fields[1].value);
  EXPECT_EQ(u"95", response_data3.fields[2].value);
  EXPECT_EQ(u"4321", response_data3.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data3.fields[4].value);

  FormData response_data4 = FillAutofillFormDataAndGetResults(
      form_with_autocompletetype, *form_with_autocompletetype.fields.begin(),
      guid);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(u"44", response_data4.fields[0].value);
  EXPECT_EQ(u"7700", response_data4.fields[1].value);
  EXPECT_EQ(u"95", response_data4.fields[2].value);
  EXPECT_EQ(u"4321", response_data4.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data4.fields[4].value);
}

TEST_F(FormFillerTest, FillPhoneNumber_ForPhonePrefixOrSuffix) {
  FormData form =
      test::GetFormData({.fields = {
                             {.label = u"country code",
                              .name = u"country_code",
                              .max_length = 1,
                              .autocomplete_attribute = "tel-country-code"},
                             {.label = u"area code",
                              .name = u"area_code",
                              .max_length = 3,
                              .autocomplete_attribute = "tel-area-code"},
                             {.label = u"prefix",
                              .name = u"phone_prefix",
                              .max_length = 3,
                              .autocomplete_attribute = "tel-local-prefix"},
                             {.label = u"-",
                              .name = u"phone_suffix",
                              .max_length = 4,
                              .autocomplete_attribute = "tel-local-suffix"},
                             {.label = u"Phone Extension",
                              .name = u"ext",
                              .max_length = 5,
                              .autocomplete_attribute = "tel-extension"},
                         }});

  FormsSeen({form});

  personal_data().ClearProfiles();
  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.set_guid(MakeGuid(104));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800FLOWERS");
  personal_data().AddProfile(profile);

  FormData response_data1 = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), profile.guid());

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(u"356", response_data1.fields[2].value);
  EXPECT_EQ(u"9377", response_data1.fields[3].value);
}

// Tests that the suggestion consists of phone number without the country code
// when a length limit is imposed in the field due to which filling with
// country code is not possible.
TEST_F(FormFillerTest, FillPhoneNumber_WithMaxLengthLimit) {
  FormData form = CreateTestAddressFormData();
  form.fields[9].max_length = 10;
  FormsSeen({form});

  AutofillProfile profile(i18n_model_definition::kLegacyHierarchyCountryCode);
  profile.set_guid(MakeGuid(103));
  profile.SetInfo(NAME_FULL, u"Natty Bumppo", "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+886123456789");
  personal_data().ClearProfiles();
  personal_data().AddProfile(profile);

  std::string guid = profile.guid();
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, *form.fields.begin(), guid);

  ASSERT_EQ(11U, response_data.fields.size());
  EXPECT_EQ(u"123456789", response_data.fields[9].value);
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_ComponentizedNumbers) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
  form_with_multiple_componentized_phone_fields.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("country code", "country_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("area code", "area_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping country code", "shipping_country_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping area code", "shipping_area_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};

  FormsSeen({form_with_multiple_componentized_phone_fields});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(), guid);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"1", response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(u"5554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_WholeNumbers) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  form_with_multiple_whole_number_fields.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};

  FormsSeen({form_with_multiple_whole_number_fields});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(), guid);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_FillPartsOnceOnly) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
  form_with_multiple_componentized_phone_fields.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("country code", "country_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("area code", "area_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "tel-national",
                          kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping country code", "shipping_country_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping area code", "shipping_area_code", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};

  FormsSeen({form_with_multiple_componentized_phone_fields});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(), guid);

  // Verify only the first complete set of phone number fields are filled,
  // and phone components are not filled more than once.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"1", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
}

// Verify when extension is misclassified, and there is a complete
// phone field, we do not fill anything to extension field.
TEST_F(FormFillerTest, FillFirstPhoneNumber_NotFillMisclassifiedExtention) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_misclassified_extension;
  form_with_misclassified_extension.renderer_id = test::MakeFormRendererId();
  form_with_misclassified_extension.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_misclassified_extension.name =
      u"complete_phone_form_with_extension";
  form_with_misclassified_extension.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "name", kMaxLength),
      CreateTestFormField("address", "address", "", FormControlType::kInputText,
                          "addresses", kMaxLength),
      CreateTestFormField("area code", "area_code", "",
                          FormControlType::kInputText, "tel-area-code",
                          kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "tel-local", kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "tel-local",
                          kMaxLength)};

  FormsSeen({form_with_misclassified_extension});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_misclassified_extension,
      *form_with_misclassified_extension.fields.begin(), guid);

  // Verify the misclassified extension field is not filled.
  ASSERT_EQ(5U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(u"5554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
}

// Verify that phone number fields annotated with the autocomplete attribute
// are filled best-effort.
// Phone number local heuristics only succeed if a PHONE_HOME_NUMBER field is
// present.
TEST_F(FormFillerTest, FillFirstPhoneNumber_BestEffortFilling) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_no_complete_number;
  form_with_no_complete_number.renderer_id = test::MakeFormRendererId();
  form_with_no_complete_number.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_no_complete_number.name = u"no_complete_phone_form";
  form_with_no_complete_number.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "name", kMaxLength),
      CreateTestFormField("address", "address", "", FormControlType::kInputText,
                          "address", kMaxLength),
      CreateTestFormField("area code", "area_code", "",
                          FormControlType::kInputText, "tel-area-code",
                          kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "extension",
                          kMaxLength)};

  FormsSeen({form_with_no_complete_number});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_no_complete_number,
      *form_with_no_complete_number.fields.begin(), guid);

  // Verify when there is no complete phone number fields, we do best effort
  // filling.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"123 Apple St., unit 6", response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
}

// When the focus is on second phone field explicitly, we will fill the
// entire form, both first phone field and second phone field included.
TEST_F(FormFillerTest, FillFirstPhoneNumber_FocusOnSecondPhoneNumber) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  form_with_multiple_whole_number_fields.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};

  FormsSeen({form_with_multiple_whole_number_fields});
  auto it = form_with_multiple_whole_number_fields.fields.begin();
  // Move it to point to "shipping number".
  std::advance(it, 3);
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_whole_number_fields, *it, guid);

  // Verify when the second phone number field is being focused, we fill
  // that field *AND* the first phone number field.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_HiddenFieldShouldNotCount) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  form_with_multiple_whole_number_fields.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};
  form_with_multiple_whole_number_fields.fields.back().is_focusable = false;
  form_with_multiple_whole_number_fields.fields.push_back(
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength));
  form_with_multiple_whole_number_fields.fields.push_back(
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength));

  FormsSeen({form_with_multiple_whole_number_fields});
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(), guid);

  // Verify hidden/non-focusable phone field is set to only_fill_when_focused.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
}

// The hidden and the presentational fields should be filled, only if their
// control type is 'select-one'. This exception is made to support synthetic
// fields.
TEST_F(FormFillerTest, FormWithHiddenOrPresentationalSelects) {
  FormData form;
  form.renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("First name", "firstname", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Last name", "lastname", "",
                                     FormControlType::kInputText)};

  {
    const std::vector<const char*> values{"CA", "US", "BR"};
    const std::vector<const char*> contents{"Canada", "United States",
                                            "Banana Republic"};
    form.fields.push_back(
        CreateTestSelectField("Country", "country", "", values, contents));
    form.fields.back().is_focusable = false;
  }
  {
    const std::vector<const char*> values{"NY", "CA", "TN"};
    const std::vector<const char*> contents{"New York", "California",
                                            "Tennessee"};
    form.fields.push_back(
        CreateTestSelectField("State", "state", "", values, contents));
    form.fields.back().role = FormFieldData::RoleAttribute::kPresentation;
  }

  form.fields.push_back(
      CreateTestFormField("City", "city", "", FormControlType::kInputText));
  form.fields.back().is_focusable = false;
  form.fields.push_back(CreateTestFormField("Street Address", "address", "",
                                            FormControlType::kInputText));
  form.fields.back().role = FormFieldData::RoleAttribute::kPresentation;

  FormsSeen({form});

  base::HistogramTester histogram_tester;

  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  histogram_tester.ExpectTotalCount(
      "Autofill.HiddenOrPresentationalSelectFieldsFilled", 2);

  ExpectFilledField("First name", "firstname", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last name", "lastname", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);
  ExpectFilledField("Country", "country", "US", FormControlType::kSelectOne,
                    response_data.fields[2]);
  ExpectFilledField("State", "state", "TN", FormControlType::kSelectOne,
                    response_data.fields[3]);
  ExpectFilledField("City", "city", "", FormControlType::kInputText,
                    response_data.fields[4]);
  ExpectFilledField("Street Address", "address", "",
                    FormControlType::kInputText, response_data.fields[5]);
}

TEST_F(FormFillerTest, FillFirstPhoneNumber_MultipleSectionFilledCorrectly) {
  AutofillProfile* work_profile = personal_data().GetProfileByGUID(MakeGuid(2));
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid = work_profile->guid();

  FormData form_with_multiple_sections;
  form_with_multiple_sections.renderer_id = test::MakeFormRendererId();
  form_with_multiple_sections.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form_with_multiple_sections.name = u"multiple_section_fields";
  form_with_multiple_sections.fields = {
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("Address", "address", "", FormControlType::kInputText,
                          "", kMaxLength),
      CreateTestFormField("number", "phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("other number", "other_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("extension", "extension", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("Full Name", "full_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("Shipping Address", "shipping_address", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("shipping number", "shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("other shipping number",
                          "other_shipping_phone_number", "",
                          FormControlType::kInputText, "", kMaxLength)};

  FormsSeen({form_with_multiple_sections});
  // Fill first sections.
  FormData response_data = FillAutofillFormDataAndGetResults(
      form_with_multiple_sections, *form_with_multiple_sections.fields.begin(),
      guid);

  // Verify first section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"123 Apple St.", response_data.fields[1].value);
  EXPECT_EQ(u"6505554567", response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
  EXPECT_EQ(std::u16string(), response_data.fields[8].value);

  // Fill second section.
  auto it = form_with_multiple_sections.fields.begin();
  std::advance(it, 6);  // Pointing to second section.

  response_data =
      FillAutofillFormDataAndGetResults(form_with_multiple_sections, *it, guid);

  // Verify second section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(std::u16string(), response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[5].value);
  EXPECT_EQ(u"123 Apple St.", response_data.fields[6].value);
  EXPECT_EQ(u"6505554567", response_data.fields[7].value);
  EXPECT_EQ(std::u16string(), response_data.fields[8].value);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(FormFillerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  // Add a field -- we'll remove it again later.
  form.fields.insert(
      form.fields.begin() + 3,
      CreateTestFormField("Some", "field", "", FormControlType::kInputText));

  FormsSeen({form});

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  FormsSeen({form});

  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(FormFillerTest, FormChangesAddField) {
  // The offset of the phone field in the address form.
  const int kPhoneFieldOffset = 9;

  // Set up our form data.
  FormData form = CreateTestAddressFormData();

  // Remove the phone field -- we'll add it back later.
  auto pos = form.fields.begin() + kPhoneFieldOffset;
  FormFieldData field = *pos;
  pos = form.fields.erase(pos);

  FormsSeen({form});

  // Now, after the call to |FormsSeen|, we restore the field before filling.
  form.fields.insert(pos, field);

  FormsSeen({form});

  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we can still fill a form when the visibility of some fields
// changes.
TEST_F(FormFillerTest, FormChangesVisibilityOfFields) {
  // Set up our form data.
  FormData form;
  form.renderer_id = test::MakeFormRendererId();
  form.url = GURL("https://www.foo.com/");

  // Default is zero, have to set to a number autofill can process.
  constexpr uint64_t kMaxLength = 10;
  form.name = u"multiple_groups_fields";
  form.fields = {
      CreateTestFormField("First Name", "first_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("Last Name", "last_name", "",
                          FormControlType::kInputText, "", kMaxLength),
      CreateTestFormField("Address", "address", "", FormControlType::kInputText,
                          "", kMaxLength),
      CreateTestFormField("Postal Code", "postal_code", "",
                          FormControlType::kInputText, "", kMaxLength)};
  form.fields.back().is_focusable = false;
  form.fields.push_back(CreateTestFormField("Country", "country", "",
                                            FormControlType::kInputText));
  form.fields.back().is_focusable = false;

  FormsSeen({form});

  // Fill the form with the first profile. The hidden fields will not get
  // filled.
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], MakeGuid(1));

  ASSERT_EQ(5U, response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis",
                    FormControlType::kInputText, response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley",
                    FormControlType::kInputText, response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.",
                    FormControlType::kInputText, response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "",
                    FormControlType::kInputText, response_data.fields[3]);
  ExpectFilledField("Country", "country", "", FormControlType::kInputText,
                    response_data.fields[4]);

  // Two other fields will show up. Select the second profile. The fields that
  // were already filled, would be left unchanged, and the rest would be filled
  // with the second profile. (Two different profiles are selected, to make sure
  // the right fields are getting filled.)
  response_data.fields[3].is_focusable = true;
  response_data.fields[4].is_focusable = true;

  FormData later_response_data = FillAutofillFormDataAndGetResults(
      response_data, response_data.fields[4], MakeGuid(2));
  ASSERT_EQ(5U, later_response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis",
                    FormControlType::kInputText, later_response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley",
                    FormControlType::kInputText, later_response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.",
                    FormControlType::kInputText, later_response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "79401",
                    FormControlType::kInputText, later_response_data.fields[3]);
  ExpectFilledField("Country", "country", "United States",
                    FormControlType::kInputText, later_response_data.fields[4]);
}

TEST_F(FormFillerTest, FillInUpdatedExpirationDate) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"02";
  details.exp_year = u"2018";
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
}

TEST_F(FormFillerTest, ProfileDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);

  // Set up our form data.
  FormData form = CreateTestAddressFormData();
  FormsSeen({form});

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(0);

  FillAutofillFormData(form, *form.fields.begin(), MakeGuid(1));
}

TEST_F(FormFillerTest, CreditCardDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillPaymentMethodsEnabled(autofill_client_,
                                                              false);

  // Set up our form data.
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(0);
  FillAutofillFormData(form, *form.fields.begin(), MakeGuid(4));
}

// Test that fields will be assigned with the source profile that was used for
// autofill.
TEST_F(FormFillerTest, TrackFillingOrigin) {
  // Set up our form data.
  FormData form = CreateTestPersonalInformationFormData();

  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  personal_data().AddProfile(profile);
  FillAutofillFormData(form, form.fields[0], profile.guid());

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  for (const auto& autofill_field_ptr : form_structure->fields()) {
    EXPECT_THAT(autofill_field_ptr->autofill_source_profile_guid(),
                testing::Optional(profile.guid()));
  }
}

// Test that filling with multiple autofill profiles will set different source
// profiles for fields.
TEST_F(FormFillerTest, TrackFillingOriginWithUsingMultipleProfiles) {
  // Set up our form data.
  FormData form = CreateTestPersonalInformationFormData();

  FormsSeen({form});

  // Fill the form with a profile without email
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.ClearFields({EMAIL_ADDRESS});
  personal_data().AddProfile(profile1);
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], profile1.guid());

  // Check that the email field has no filling source.
  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_EQ(form.fields[3].label, u"Email");
  EXPECT_EQ(form_structure->field(3)->autofill_source_profile_guid(),
            std::nullopt);

  // Then fill the email field using the second profile
  AutofillProfile profile2 = test::GetFullProfile();
  personal_data().AddProfile(profile2);
  FormData later_response_data = FillAutofillFormDataAndGetResults(
      response_data, form.fields[3], profile2.guid());

  // Check that the first three fields have the first profile as filling source
  // and the last field has the second profile.
  form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  EXPECT_THAT(form_structure->field(0)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(1)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(2)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(3)->autofill_source_profile_guid(),
              testing::Optional(profile2.guid()));
}

// Test that an autofilled and edited field will be assigned with the autofill
// profile.
TEST_F(FormFillerTest, TrackFillingOriginOnEditedField) {
  // Set up our form data.
  FormData form = CreateTestPersonalInformationFormData();

  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  personal_data().AddProfile(profile);
  FormData response_data =
      FillAutofillFormDataAndGetResults(form, form.fields[0], profile.guid());

  // Simulate editing the first field.
  response_data.fields[0].value = u"Michael";
  browser_autofill_manager_->OnTextFieldDidChange(
      response_data, response_data.fields[0], gfx::RectF(),
      base::TimeTicks::Now());

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  AutofillField* edited_field = form_structure->field(0);
  ASSERT_FALSE(edited_field->is_autofilled);
  ASSERT_TRUE(edited_field->previously_autofilled());
  EXPECT_THAT(edited_field->autofill_source_profile_guid(),
              testing::Optional(profile.guid()));
}

// Test that only autofilled fields will be assigned with the autofill profile.
TEST_F(FormFillerTest, TrackFillingOriginWorksOnlyOnFilledField) {
  // Set up our form data.
  FormData form = CreateTestPersonalInformationFormData();

  FormsSeen({form});

  // Fill the form with a profile without email field.
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({EMAIL_ADDRESS});
  personal_data().AddProfile(profile);
  FillAutofillFormData(form, form.fields[0], profile.guid());

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  // Check that the email field has no filling source.
  EXPECT_EQ(form_structure->field(3)->autofill_source_profile_guid(),
            std::nullopt);
}

// The following Refill Tests ensure that Autofill can handle the situation
// where it fills a credit card form with an expiration date like 04/2999
// and the website tries to reformat the input with whitespaces around the
// slash and then sacrifices the wrong digits in the expiration date. I.e.,
// the website replaces "04/2099" with "04 / 20". The tests ensure that this
// triggers a refill with "04 / 29".
struct RefillTestCase {
  // The value that JavaScript owned by the website sets for the expiration
  // date filed.
  std::u16string exp_date_from_js;
  // Whether we expect a refill from in this test case.
  bool triggers_refill;
  // What value we expect in the refill.
  const char* refilled_exp_date = nullptr;
};

class FormFillerRefillTest
    : public FormFillerTest,
      public testing::WithParamInterface<RefillTestCase> {};

TEST_P(FormFillerRefillTest, RefillModifiedCreditCardExpirationDates) {
  RefillTestCase test_case = GetParam();

  // Set up a CC form with name, cc number and expiration date.
  FormData form;
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.fields = {CreateTestFormField("Name on Card", "nameoncard", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Card Number", "cardnumber", "",
                                     FormControlType::kInputText),
                 CreateTestFormField("Expiration date", "exp_date", "",
                                     FormControlType::kInputText)};

  // Notify BrowserAutofillManager of the form.
  FormsSeen({form});

  // Simulate filling and store the data to be filled in |first_fill_data|.
  FormData first_fill_data = FillAutofillFormDataAndGetResults(
      form, *form.fields.begin(), MakeGuid(4));
  ASSERT_EQ(3u, first_fill_data.fields.size());
  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley",
                    FormControlType::kInputText, first_fill_data.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456",
                    FormControlType::kInputText, first_fill_data.fields[1]);
  ExpectFilledField("Expiration date", "exp_date", "04/2999",
                    FormControlType::kInputText, first_fill_data.fields[2]);

  FormData refilled_form;
  if (test_case.triggers_refill) {
    // Prepare intercepting the filling operation to the driver and capture
    // the re-filled form data.
    EXPECT_CALL(*autofill_driver_, ApplyFormAction)
        .Times(1)
        .WillOnce(DoAll(SaveArg<2>(&refilled_form),
                        Return(std::vector<FieldGlobalId>{})));
  } else {
    EXPECT_CALL(*autofill_driver_, ApplyFormAction).Times(0);
  }
  // Simulate that JavaScript modifies the expiration date field.
  FormData form_after_js_modification = first_fill_data;
  form_after_js_modification.fields[2].value = test_case.exp_date_from_js;
  browser_autofill_manager_->OnJavaScriptChangedAutofilledValue(
      form_after_js_modification, form_after_js_modification.fields[2],
      u"04/2999");

  testing::Mock::VerifyAndClearExpectations(autofill_driver_.get());

  if (test_case.triggers_refill) {
    ASSERT_EQ(3u, refilled_form.fields.size());
    ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley",
                      FormControlType::kInputText, refilled_form.fields[0]);
    EXPECT_FALSE(refilled_form.fields[0].force_override);
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456",
                      FormControlType::kInputText, refilled_form.fields[1]);
    EXPECT_FALSE(refilled_form.fields[1].force_override);
    ExpectFilledField("Expiration date", "exp_date",
                      test_case.refilled_exp_date, FormControlType::kInputText,
                      refilled_form.fields[2]);
    EXPECT_TRUE(refilled_form.fields[2].force_override);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FormFillerRefillTest,
    testing::Values(
        // This is the classic case: Autofill filled 04/2999, website overrode
        // 04 / 29, we need to fix this to 04 / 99.
        RefillTestCase{.exp_date_from_js = u"04 / 29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04 / 99"},
        // Maybe the website replaced the separator and added whitespaces.
        RefillTestCase{.exp_date_from_js = u"04 - 29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04 - 99"},
        // Maybe the website only replaced the separator.
        RefillTestCase{.exp_date_from_js = u"04-29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04-99"},
        // Maybe the website was smart and dropped the correct digits.
        RefillTestCase{
            .exp_date_from_js = u"04 / 99",
            .triggers_refill = false,
        },
        // Maybe the website did not modify the values at all.
        RefillTestCase{
            .exp_date_from_js = u"04/2999",
            .triggers_refill = false,
        },
        // Maybe the website did something we don't support.
        RefillTestCase{
            .exp_date_from_js = u"April / 2999",
            .triggers_refill = false,
        },
        // Maybe the website just added some whitespaces.
        RefillTestCase{
            .exp_date_from_js = u"04 / 2999",
            .triggers_refill = false,
        },
        // Don't trigger refill on 3 digit years.
        RefillTestCase{
            .exp_date_from_js = u"04 / 299",
            .triggers_refill = false,
        }));

}  // namespace autofill
