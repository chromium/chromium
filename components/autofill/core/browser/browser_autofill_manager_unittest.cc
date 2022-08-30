// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/browser_autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_strike_database.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_download_manager.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
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
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/translate/core/common/language_detection_details.h"
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
#include "url/url_canon.h"

using base::UTF8ToUTF16;
using testing::_;
using testing::AnyOf;
using testing::AtLeast;
using testing::Contains;
using testing::DoAll;
using testing::Each;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace autofill {

using features::kAutofillRemoveCardExpiryFromDownstreamSuggestion;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

const int kDefaultPageID = 137;
const std::string kArbitraryNickname = "Grocery Card";
const std::u16string kArbitraryNickname16 = u"Grocery Card";

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

  MOCK_METHOD(bool, ShouldShowSigninPromo, (), (override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD(void,
              ConfirmSaveUpiIdLocally,
              (const std::string& upi_id,
               base::OnceCallback<void(bool user_decision)> callback),
              (override));
#endif
  MOCK_METHOD(profile_metrics::BrowserProfileType,
              GetProfileType,
              (),
              (const override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));
  MOCK_METHOD(bool, IsPasswordManagerEnabled, (), (override));
};

class MockAutofillDownloadManager : public TestAutofillDownloadManager {
 public:
  MockAutofillDownloadManager(AutofillDriver* driver,
                              AutofillDownloadManager::Observer* observer)
      : TestAutofillDownloadManager(driver, observer) {}
  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*),
              (override));
};

class MockTouchToFillDelegateImpl : public TouchToFillDelegateImpl {
 public:
  explicit MockTouchToFillDelegateImpl(BrowserAutofillManager* manager)
      : TouchToFillDelegateImpl(manager) {}
  MockTouchToFillDelegateImpl(const MockTouchToFillDelegateImpl&) = delete;
  MockTouchToFillDelegateImpl& operator=(const MockTouchToFillDelegateImpl&) =
      delete;
  ~MockTouchToFillDelegateImpl() override = default;

  MOCK_METHOD(bool,
              TryToShowTouchToFill,
              (int query_id, const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(void, HideTouchToFill, (), (override));
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
  EXPECT_EQ(u"MyForm", filled_form.name);
  EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.url);
  EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);

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
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD((std::vector<FieldGlobalId>),
              FillOrPreviewForm,
              (int query_id,
               mojom::RendererFormDataAction action,
               const FormData& data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, ServerFieldType>&)),
              (override));
  MOCK_METHOD(void,
              SendAutofillTypePredictionsToRenderer,
              (const std::vector<FormStructure*>& forms),
              (override));
  MOCK_METHOD(void,
              SendFieldsEligibleForManualFillingToRenderer,
              (const std::vector<FieldGlobalId>& fields),
              (override));
};

}  // namespace

class BrowserAutofillManagerTest : public testing::Test {
 public:
  BrowserAutofillManagerTest() = default;

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
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
    personal_data().SetPrefService(autofill_client_.GetPrefs());

    autocomplete_history_manager_ =
        std::make_unique<NiceMock<MockAutocompleteHistoryManager>>();
    autocomplete_history_manager_->Init(
        /*profile_database=*/database_,
        /*pref_service=*/autofill_client_.GetPrefs(),
        /*is_off_the_record=*/false);
    merchant_promo_code_manager_ =
        std::make_unique<NiceMock<MockMerchantPromoCodeManager>>();
    merchant_promo_code_manager_->Init(&personal_data(),
                                       /*is_off_the_record=*/false);

    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    auto payments_client = std::make_unique<payments::TestPaymentsClient>(
        autofill_driver_->GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data());
    payments_client_ = payments_client.get();
    autofill_client_.set_test_payments_client(std::move(payments_client));
    TestCreditCardSaveManager* credit_card_save_manager =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data());
    credit_card_save_manager->SetCreditCardUploadEnabled(true);
    auto test_form_data_importer =
        std::make_unique<autofill::TestFormDataImporter>(
            &autofill_client_, payments_client_,
            std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager),
            &personal_data(), "en-US");
    test_form_data_importer_ = test_form_data_importer.get();
    autofill_client_.set_test_form_data_importer(
        std::move(test_form_data_importer));
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    auto single_field_form_fill_router =
        std::make_unique<NiceMock<MockSingleFieldFormFillRouter>>(
            autocomplete_history_manager_.get(),
            merchant_promo_code_manager_.get());
    // By default, if we offer single field form fill, suggestions should be
    // returned because it is assumed |field.should_autocomplete| is set to
    // true. This should be overridden in tests where
    // |field.should_autocomplete| is set to false.
    ON_CALL(*single_field_form_fill_router, OnGetSingleFieldSuggestions)
        .WillByDefault(testing::Return(true));
    single_field_form_fill_router_ = single_field_form_fill_router.get();
    browser_autofill_manager_->set_single_field_form_fill_router_for_test(
        std::move(single_field_form_fill_router));

    auto download_manager = std::make_unique<MockAutofillDownloadManager>(
        autofill_driver_.get(), browser_autofill_manager_.get());
    download_manager_ = download_manager.get();
    browser_autofill_manager_->set_download_manager_for_test(
        std::move(download_manager));

    auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
        browser_autofill_manager_.get(), autofill_driver_.get(),
        /*call_parent_methods=*/false);
    external_delegate_ = external_delegate.get();
    browser_autofill_manager_->SetExternalDelegateForTest(
        std::move(external_delegate));

    auto touch_to_fill_delegate = std::make_unique<MockTouchToFillDelegateImpl>(
        browser_autofill_manager_.get());
    ON_CALL(*touch_to_fill_delegate, IsShowingTouchToFill())
        .WillByDefault(Return(false));
    touch_to_fill_delegate_ = touch_to_fill_delegate.get();
    browser_autofill_manager_->SetTouchToFillDelegateImplForTest(
        std::move(touch_to_fill_delegate));

    auto test_strike_database = std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));

    // Initialize the TestPersonalDataManager with some default data.
    CreateTestAutofillProfiles();
    CreateTestCreditCards();
  }

  void CreateTestServerCreditCards() {
    personal_data().ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data().AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data().AddServerCreditCard(full_server_card);
  }

  void CreateTestServerAndLocalCreditCards() {
    personal_data().ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data().AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data().AddServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    personal_data().AddCreditCard(local_card);
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    browser_autofill_manager_.reset();

    personal_data().SetPrefService(nullptr);
    personal_data().ClearCreditCards();
  }

  void GetAutofillSuggestions(int query_id,
                              const FormData& form,
                              const FormFieldData& field) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, gfx::RectF(), query_id,
        /*autoselect_first_suggestion=*/false, TouchToFillEligible(false));
  }

  void GetAutofillSuggestions(const FormData& form,
                              const FormFieldData& field) {
    GetAutofillSuggestions(kDefaultPageID, form, field);
  }

  void TryToShowTouchToFill(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            TouchToFillEligible touch_to_fill_eligible) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, gfx::RectF(), query_id,
        /*autoselect_first_suggestion=*/false, touch_to_fill_eligible);
  }

  void AutocompleteSuggestionsReturned(
      const std::vector<std::u16string>& results,
      int query_id = kDefaultPageID) {
    std::vector<Suggestion> suggestions;
    std::transform(results.begin(), results.end(),
                   std::back_inserter(suggestions),
                   [](auto result) { return Suggestion(result); });

    browser_autofill_manager_->OnSuggestionsReturned(
        query_id, /*autoselect_first_suggestion=*/false, suggestions);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    browser_autofill_manager_->OnFormsSeen(/*updated_forms=*/forms,
                                           /*removed_forms=*/{});
  }

  void FormSubmitted(const FormData& form) {
    browser_autofill_manager_->OnFormSubmitted(
        form, false, SubmissionSource::FORM_SUBMISSION);
  }

  void FillAutofillFormData(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, {}, query_id, /*autoselect_first_suggestion=*/true,
        TouchToFillEligible(false));
    browser_autofill_manager_->FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, query_id, form, field, unique_id);
  }

  // Calls |browser_autofill_manager_->OnFillAutofillFormData()| with the
  // specified input parameters after setting up the expectation that the mock
  // driver's |FillOrPreviewForm()| method will be called and saving the
  // parameters of that call into the |response_query_id| and |response_data|
  // output parameters.
  void FillAutofillFormDataAndSaveResults(int input_query_id,
                                          const FormData& input_form,
                                          const FormFieldData& input_field,
                                          int unique_id,
                                          int* response_query_id,
                                          FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _))
        .WillOnce(DoAll(testing::SaveArg<0>(response_query_id),
                        testing::SaveArg<2>(response_data),
                        testing::Return(std::vector<FieldGlobalId>{})));
    FillAutofillFormData(input_query_id, input_form, input_field, unique_id);
  }

  void PreviewVirtualCardDataAndSaveResults(
      mojom::RendererFormDataAction action,
      const std::string& guid,
      int input_query_id,
      const FormData& input_form,
      const FormFieldData& input_field,
      int* response_query_id,
      FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _))
        .WillOnce((DoAll(testing::SaveArg<0>(response_query_id),
                         testing::SaveArg<2>(response_data),
                         testing::Return(std::vector<FieldGlobalId>{}))));
    browser_autofill_manager_->FillOrPreviewVirtualCardInformation(
        action, guid, input_query_id, input_form, input_field);
  }

  int MakeFrontendId(const std::string& cc_sid,
                     const std::string& profile_sid) const {
    return browser_autofill_manager_->suggestion_generator()->MakeFrontendId(
        cc_sid, profile_sid);
  }

  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field) {
    return browser_autofill_manager_->WillFillCreditCardNumber(form, field);
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->name = u"MyForm";
    if (is_https) {
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

    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _))
        .Times(AtLeast(1));
    browser_autofill_manager_->FillOrPreviewCreditCardForm(
        mojom::RendererFormDataAction::kFill, kDefaultPageID, *form,
        form->fields[0], card);
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            ->GetCVCAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsClient::UnmaskResponseDetails response;
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
            ->GetCVCAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

  void DisableAutofillViaAblation(
      base::test::ScopedFeatureList& scoped_feature_list,
      bool for_addresses,
      bool for_credit_cards) {
    base::FieldTrialParams feature_parameters{
        {features::kAutofillAblationStudyEnabledForAddressesParam.name,
         for_addresses ? "true" : "false"},
        {features::kAutofillAblationStudyEnabledForPaymentsParam.name,
         for_credit_cards ? "true" : "false"},
        {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
         "1000"},
    };
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableAblationStudy, feature_parameters);
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

  void ResetBrowserAutofillManager(TestAutofillClient* client) {
    // |browser_autofill_manager_| owns the |single_field_form_fill_router_| and
    // clears it upon being recreated. Clear it first and then give it a new
    // SingleFieldFormFillRouter to avoid referencing deleted memory.
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    auto single_field_form_fill_router =
        std::make_unique<NiceMock<MockSingleFieldFormFillRouter>>(
            autocomplete_history_manager_.get(),
            merchant_promo_code_manager_.get());
    single_field_form_fill_router_ = single_field_form_fill_router.get();
    browser_autofill_manager_->set_single_field_form_fill_router_for_test(
        std::move(single_field_form_fill_router));
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TestAutofillExternalDelegate> external_delegate_;
  raw_ptr<MockTouchToFillDelegateImpl> touch_to_fill_delegate_;
  scoped_refptr<AutofillWebDataService> database_;
  raw_ptr<MockAutofillDownloadManager> download_manager_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  std::unique_ptr<MockMerchantPromoCodeManager> merchant_promo_code_manager_;
  raw_ptr<MockSingleFieldFormFillRouter> single_field_form_fill_router_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestStrikeDatabase> strike_database_;
  raw_ptr<payments::TestPaymentsClient> payments_client_;
  raw_ptr<TestFormDataImporter> test_form_data_importer_;

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
    personal_data().AddProfile(profile1);

    AutofillProfile profile2;
    test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile2.set_guid("00000000-0000-0000-0000-000000000002");
    personal_data().AddProfile(profile2);

    AutofillProfile profile3;
    test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "",
                         "");
    profile3.set_guid("00000000-0000-0000-0000-000000000003");
    personal_data().AddProfile(profile3);
  }

  void CreateTestCreditCards() {
    CreditCard credit_card1;
    test::SetCreditCardInfo(&credit_card1, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card1.set_guid("00000000-0000-0000-0000-000000000004");
    credit_card1.set_use_count(10);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(5));
    personal_data().AddCreditCard(credit_card1);

    CreditCard credit_card2;
    test::SetCreditCardInfo(&credit_card2, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card2.set_guid("00000000-0000-0000-0000-000000000005");
    credit_card2.set_use_count(5);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(4));
    personal_data().AddCreditCard(credit_card2);

    CreditCard credit_card3;
    test::SetCreditCardInfo(&credit_card3, "", "", "", "", "");
    credit_card3.set_guid("00000000-0000-0000-0000-000000000006");
    personal_data().AddCreditCard(credit_card3);
  }
};

// Subclass of BrowserAutofillManagerTest that parameterizes the finch flag to
// enable structured names.
// TODO(crbug.com/1103421): Clean legacy implementation once structured names
// are fully launched. Here, the changes applied in CL 2333204 must be reverted
// by deleting this class and use TEST_F(BrowserAutofillManagerTest, ) for all
// test cases again.
class BrowserAutofillManagerStructuredProfileTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    InitializeFeatures();
    BrowserAutofillManagerTest::SetUp();
  }

  void InitializeFeatures();

  bool StructuredNamesAndAddresses() const {
    return structured_names_and_addresses_;
  }

 private:
  bool structured_names_and_addresses_;
  base::test::ScopedFeatureList scoped_features_;
};

void BrowserAutofillManagerStructuredProfileTest::InitializeFeatures() {
  structured_names_and_addresses_ = GetParam();

  // The BrowserAutofillManagerStructuredProfileTest test suite will run every
  // test once with these flags on, and once with these flags off.
  std::vector<base::Feature> features = {
      features::kAutofillEnableSupportForMoreStructureInAddresses,
      features::kAutofillEnableSupportForMoreStructureInNames,
      // TODO(crbug.com/1190334): Remove this feature flag once the GPay
      // activated promo code autofill project is fully launched and stable.
      features::kAutofillFillMerchantPromoCodeFields};
  if (structured_names_and_addresses_) {
    scoped_features_.InitWithFeatures(features, {});
  } else {
    scoped_features_.InitWithFeatures({}, features);
  }
}

class SuggestionMatchingTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<std::tuple<bool, std::string>> {
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    InitializeFeatures();
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void InitializeFeatures();
#else
  void InitializeFeatures();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  std::string MakeLabel(const std::vector<std::string>& parts);
  std::string MakeMobileLabel(const std::vector<std::string>& parts);

  enum class EnabledFeature { kNone, kDesktop, kMobileShowAll, kMobileShowOne };
  EnabledFeature enabled_feature_;
  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

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

// Credit card suggestion tests related with keyboard accessory.
class CreditCardSuggestionTest : public BrowserAutofillManagerTest,
                                 public testing::WithParamInterface<bool> {
 protected:
  CreditCardSuggestionTest() : is_keyboard_accessory_enabled_(GetParam()) {}

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    features_.InitWithFeatureState(features::kAutofillKeyboardAccessory,
                                   is_keyboard_accessory_enabled_);
  }

  int ObfuscationLength() {
#if BUILDFLAG(IS_ANDROID)
    return is_keyboard_accessory_enabled_ ? 2 : 4;
#else
    return 4;
#endif
  }

 private:
  base::test::ScopedFeatureList features_;
  const bool is_keyboard_accessory_enabled_;
};

// Test that calling OnFormsSeen with an empty set of forms (such as when
// reloading a page or when the renderer processes a set of forms but detects
// no changes) does not load the forms again.
TEST_P(BrowserAutofillManagerStructuredProfileTest, OnFormsSeen_Empty) {
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnFormsSeen_DifferentFormStructures) {
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
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"MyForm";
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnFormsSeen_SendAutofillTypePredictionsToRenderer) {
  // Set up a queryable form.
  FormData form1;
  test::CreateTestAddressFormData(&form1);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"NonQueryable";
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

// Test that when forms are seen, the renderer is sent the fields that are
// eligible for manual filling.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnFormsSeen_SendFieldsEligibleForManualFillingToRenderer) {
  // Set up a queryable form.
  FormData form1;
  CreateTestCreditCardFormData(&form1, true, false);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"NonQueryable";
  form2.url = form1.url;
  form2.action = GURL("https://myform.com/submit.html");
  form2.fields.push_back(field);

  // Package the forms for observation.
  std::vector<FormData> forms{form1, form2};

  // Set up expectations.
  EXPECT_CALL(*autofill_driver_,
              SendFieldsEligibleForManualFillingToRenderer(_))
      .Times(2);
  FormsSeen(forms);
}

// Test that no autofill suggestions are returned for a field with an
// unrecognized autocomplete attribute.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_UnrecognizedAttribute) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
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

  // Ensure that the SingleFieldFormFillRouter is not called for
  // suggestions either.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_MinFieldsEnforced_NoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that the SingleFieldFormFillRouter is called for both fields.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(2);

  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());

  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that when small forms are disabled (min required fields enforced)
// for a form with two fields with one that has an autocomplete attribute,
// suggestions are only made for the one that has the attribute.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_MinFieldsEnforced_WithOneAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
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

// Test that for a form with two fields with autocomplete attributes,
// suggestions are made for both fields. This is true even if a minimum number
// of fields is enforced.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_SmallFormWithTwoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
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

// Test that the call is properly forwarded to its SingleFieldFormFillRouter.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnSingleFieldSuggestionSelected) {
  std::u16string test_value = u"TestValue";
  EXPECT_CALL(*single_field_form_fill_router_,
              OnSingleFieldSuggestionSelected(test_value,
                                              POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
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
  profile1.SetInfo(NAME_FIRST, u"Robin", "en-US");
  profile1.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, u"Carl", "en-US");
  profile2.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile2);

  AutofillProfile profile3;
  profile3.set_guid("00000000-0000-0000-0000-000000000126");
  profile3.SetInfo(NAME_FIRST, u"Aaron", "en-US");
  profile3.SetInfo(NAME_LAST, u"Googler", "en-US");
  profile3.SetInfo(ADDRESS_HOME_LINE1, u"1600 Amphitheater pkwy", "en-US");
  personal_data().AddProfile(profile3);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_UnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  AutofillProfile duplicate_profile = *(personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000001"));
  personal_data().AddProfile(duplicate_profile);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_AutofillDisabledByUser) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Disable Autofill.
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnSuggestionsReturned_CallsExternalDelegate) {
  std::vector<Suggestion> suggestions = {
      Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2)};

  {
    browser_autofill_manager_->OnSuggestionsReturned(
        kDefaultPageID, /*autoselect_first_suggestion=*/false, suggestions);

    EXPECT_FALSE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(kDefaultPageID, suggestions[0], suggestions[1]);
  }
  {
    browser_autofill_manager_->OnSuggestionsReturned(
        kDefaultPageID, /*autoselect_first_suggestion=*/true, suggestions);

    EXPECT_TRUE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(kDefaultPageID, suggestions[0], suggestions[1]);
  }
}

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has whitespace in it.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_Whitespace) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = u"       ";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it, which should be removed.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_StopCharsOnly) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = u"____-____-____-____";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has some invisible unicode characters in it.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_InvisibleUnicodeOnly) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = std::u16string({0x200E, 0x200F});
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it and some input.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_StopCharsWithInput) {
  // Add a credit card with particular numbers that we will attempt to recall.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Smith",
                          "5255667890123123",  // Mastercard
                          "08", "2017", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];

  field.value = u"5255-66__-____-____";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string master_card_label = std::string("08/17");
#else
  const std::string master_card_label = std::string("Expires on 08/17");
#endif

  // Test that we sent the right value to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("3123"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return only matching credit card profile suggestions when the
// selected form field has been partially filled out.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "78", "text", &field);
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
#else
  const std::string visa_label = std::string("Expires on 04/99");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that we return credit card profile suggestions when the selected form
// field is the credit card number field.
TEST_P(CreditCardSuggestionTest, GetCreditCardSuggestions_CCNumber) {
  // Set nickname with the corresponding guid of the Mastercard 8765.
  personal_data().SetNicknameForCardWithGUID(
      "00000000-0000-0000-0000-000000000005", kArbitraryNickname);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& credit_card_number_field = form.fields[1];
  GetAutofillSuggestions(form, credit_card_number_field);
  int obfuscation_length = base::FeatureList::IsEnabled(
                               autofill::features::kAutofillKeyboardAccessory)
                               ? 2
                               : 4;
  const std::string visa_value =
      std::string("Visa  ") +
      test::ObfuscatedCardDigitsAsUTF8("3456", obfuscation_length);
  // Mastercard has a valid nickname. Display nickname + last four in the
  // suggestion title.
  const std::string master_card_value =
      kArbitraryNickname + "  " +
      test::ObfuscatedCardDigitsAsUTF8("8765", obfuscation_length);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(visa_value, visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(master_card_value, master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card profile suggestions when the selected form
// field is not the credit card number field.
TEST_P(CreditCardSuggestionTest, GetCreditCardSuggestions_NonCCNumber) {
  // Set nickname with the corresponding guid of the Mastercard 8765.
  personal_data().SetNicknameForCardWithGUID(
      "00000000-0000-0000-0000-000000000005", kArbitraryNickname);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& cardholder_name_field = form.fields[0];
  GetAutofillSuggestions(form, cardholder_name_field);

  const std::string obfuscated_last_four_digits1 =
      test::ObfuscatedCardDigitsAsUTF8("3456", ObfuscationLength());
  const std::string obfuscated_last_four_digits2 =
      test::ObfuscatedCardDigitsAsUTF8("8765", ObfuscationLength());

#if BUILDFLAG(IS_ANDROID)
  // For Android, when keyboard accessary is enabled, always show obfuscated
  // last four. When keyboard accessary is not enabled (drop-down suggestion):
  // 1) if nickname feature is enabled and nickname is available, show nickname
  // + last four. 2) Otherwise, show network + last four.
  // Visa card does not have a nickname.
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits1
          : std::string("Visa  ") + obfuscated_last_four_digits1;
  // Mastercard has a valid nickname.
  const std::string master_card_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits2
          : kArbitraryNickname + "  " + obfuscated_last_four_digits2;

#elif BUILDFLAG(IS_IOS)
  const std::string visa_label = obfuscated_last_four_digits1;
  const std::string master_card_label = obfuscated_last_four_digits2;

#else
  // If no nickname available, we will show network.
  const std::string visa_label = base::JoinString(
      {"Visa  ", obfuscated_last_four_digits1, ", expires on 04/99"}, "");
  // When nickname is available, show nickname. Otherwise, show network.
  const std::string master_card_label =
      base::JoinString({kArbitraryNickname + "  ", obfuscated_last_four_digits2,
                        ", expires on 10/98"},
                       "");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion("Elvis Presley", visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion("Buddy Holly", master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we will eventually return the credit card signin promo when there
// are no credit card suggestions and the promo is active. See the tests in
// AutofillExternalDelegateTest that test whether the promo is added.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_OnlySigninPromo) {
  personal_data().ClearCreditCards();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[1];

  ON_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillByDefault(Return(true));
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(2);
  EXPECT_TRUE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Single field form fill suggestions are not queried.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", -26));

  // Clear the test credit cards and try again -- we should still show the
  // mixed form warning.
  personal_data().ClearCreditCards();
  GetAutofillSuggestions(form, field);
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", -26));
}

// Test that we return credit card suggestions for secure pages that have an
// empty form action target URL.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card suggestions for secure pages that have a
// form action set to "javascript:something".
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_RepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "5231567890123456",  // Mastercard
                          "05", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  credit_card.set_use_date(AutofillClock::Now() - base::Days(15));
  personal_data().AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label1, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("3456"),
                 master_card_label2, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that a masked server card is not suggested if more that six digits
// have been typed in the field.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_MaskedCardWithMoreThan6Digits) {
  // Add a masked server card.
  personal_data().ClearCreditCards();

  CreditCard masked_server_card;
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
  masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
  personal_data().AddServerCreditCard(masked_server_card);
  EXPECT_EQ(1U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = u"12345678";
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

// Test that expired cards are ordered by their ranking score and are always
// suggested after non expired cards even if they have a higher ranking score.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_ExpiredCards) {
  personal_data().ClearCreditCards();

  // Add a never used non expired credit card.
  CreditCard credit_card0("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2055",
                          "1");
  credit_card0.set_guid("00000000-0000-0000-0000-000000000001");
  personal_data().AddCreditCard(credit_card0);

  // Add an expired card with a higher ranking score.
  CreditCard credit_card1("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2010", "1");
  credit_card1.set_guid("00000000-0000-0000-0000-000000000002");
  credit_card1.set_use_count(300);
  credit_card1.set_use_date(AutofillClock::Now() - base::Days(10));
  personal_data().AddCreditCard(credit_card1);

  // Add an expired card with a lower ranking score.
  CreditCard credit_card2("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card2.set_use_count(3);
  credit_card2.set_use_date(AutofillClock::Now() - base::Days(1));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2011", "1");
  credit_card2.set_guid("00000000-0000-0000-0000-000000000003");
  personal_data().AddCreditCard(credit_card2);

  ASSERT_EQ(3U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("01/11");
  const std::string master_card_label = std::string("04/55");
  const std::string amex_card_label = std::string("04/10");
#else
  const std::string visa_label = std::string("Expires on 01/11");
  const std::string master_card_label = std::string("Expires on 04/55");
  const std::string amex_card_label = std::string("Expires on 04/10");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("5100"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(1)),
      Suggestion(
          std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005"),
          amex_card_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(2)),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(3)));
}

// Test cards that are expired AND disused are suppressed when suppression is
// enabled and the input field is empty.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_SuppressDisusedCreditCardsOnEmptyField) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());

  // Add a never used non expired local credit card.
  CreditCard credit_card0("00000000-0000-0000-0000-000000000000",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data().AddCreditCard(credit_card0);

  auto now = AutofillClock::Now();

  // Add an expired local card last used 10 days ago
  CreditCard credit_card1("00000000-0000-0000-0000-000000000001",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "4234567890123456" /* Visa */, "04", "2010", "1");
  credit_card1.set_use_date(now - base::Days(10));
  personal_data().AddCreditCard(credit_card1);

  // Add an expired local card last used 180 days ago.
  CreditCard credit_card2("00000000-0000-0000-0000-000000000002",
                          test::kEmptyOrigin);
  credit_card2.set_use_date(now - base::Days(182));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "378282246310005" /* American Express */, "01",
                          "2010", "1");
  personal_data().AddCreditCard(credit_card2);

  ASSERT_EQ(3U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Query with empty string only returns card0 and card1. Note expired
  // masked card2 is not suggested on empty fields.
  {
    FormFieldData field = form.fields[0];
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100");
    const std::string visa_label =
        std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
    const std::string mastercard_label =
        test::ObfuscatedCardDigitsAsUTF8("5100");
    const std::string visa_label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100") +
        std::string(", expires on 04/99");
    const std::string visa_label = std::string("Visa  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("3456") +
                                   std::string(", expires on 04/10");
#endif

    CheckSuggestions(
        kDefaultPageID,
        Suggestion("Bonnie Parker", mastercard_label, kMasterCard,
                   browser_autofill_manager_->GetPackedCreditCardID(0)),
        Suggestion("Clyde Barrow", visa_label, kVisaCard,
                   browser_autofill_manager_->GetPackedCreditCardID(1)));
  }

  // Query with name prefix for card0 returns card0.
  {
    FormFieldData field = form.fields[0];
    field.value = u"B";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100");
#elif BUILDFLAG(IS_IOS)
    const std::string mastercard_label =
        test::ObfuscatedCardDigitsAsUTF8("5100");
#else
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100") +
        std::string(", expires on 04/99");
#endif

    CheckSuggestions(
        kDefaultPageID,
        Suggestion("Bonnie Parker", mastercard_label, kMasterCard,
                   browser_autofill_manager_->GetPackedCreditCardID(0)));
  }

  // Query with name prefix for card1 returns card1.
  {
    FormFieldData field = form.fields[0];
    field.value = u"Cl";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string visa_label =
        std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
    const std::string visa_label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
    const std::string visa_label = std::string("Visa  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("3456") +
                                   std::string(", expires on 04/10");
#endif

    CheckSuggestions(
        kDefaultPageID,
        Suggestion("Clyde Barrow", visa_label, kVisaCard,
                   browser_autofill_manager_->GetPackedCreditCardID(1)));
  }

  // Query with name prefix for card2 returns card2.
  {
    FormFieldData field = form.fields[0];
    field.value = u"Jo";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string amex_label =
        std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005");
#elif BUILDFLAG(IS_IOS)
    const std::string amex_label = test::ObfuscatedCardDigitsAsUTF8("0005");
#else
    const std::string amex_label = std::string("Amex  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("0005") +
                                   std::string(", expires on 01/10");
#endif

    CheckSuggestions(
        kDefaultPageID,
        Suggestion("John Dillinger", amex_label, kAmericanExpressCard,
                   browser_autofill_manager_->GetPackedCreditCardID(2)));
  }
}

// Test that a card that doesn't have a number is not shown in the
// suggestions when querying credit cards by their number, and is shown when
// querying other fields.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_NumberMissing) {
  // Create one normal credit card and one credit card with the number
  // missing.
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card0.set_guid("00000000-0000-0000-0000-000000000001");
  personal_data().AddCreditCard(credit_card0);

  CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2999",
                          "1");
  credit_card1.set_guid("00000000-0000-0000-0000-000000000002");
  personal_data().AddCreditCard(credit_card1);

  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Query by card number field.
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

// Sublabel is expiration date when filling card number. The second card
// doesn't have a number so it should not be included in the suggestions.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string amex_card_exp_label = std::string("04/99");
#else
  const std::string amex_card_exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005"),
          amex_card_exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(1)));

  // Query by cardholder name field.
  field = form.fields[0];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
  const std::string amex_card_label =
      std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005");
#elif BUILDFLAG(IS_IOS)
  const std::string amex_card_label = test::ObfuscatedCardDigitsAsUTF8("0005");
#else
  const std::string amex_card_label = std::string("Amex  ") +
                                      test::ObfuscatedCardDigitsAsUTF8("0005") +
                                      std::string(", expires on 04/99");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion("John Dillinger", "", kGenericCard,
                 browser_autofill_manager_->GetPackedCreditCardID(2)),
      Suggestion("Clyde Barrow", amex_card_label, kAmericanExpressCard,
                 browser_autofill_manager_->GetPackedCreditCardID(1)));
}

// Test that a suggestion does not have label (expiration date) if the
// suggestion is poped up from a credit card number field.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_NoLabelForCCNumberField) {
  scoped_feature_list_.InitAndEnableFeature(
      kAutofillRemoveCardExpiryFromDownstreamSuggestion);

  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card0.set_guid("00000000-0000-0000-0000-000000000001");
  personal_data().AddCreditCard(credit_card0);

  ASSERT_EQ(1U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Query by card number field.
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005"),
          std::string(), kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(1)));
}

TEST_F(BrowserAutofillManagerTest, OnCreditCardFetched_StoreInstrumentId) {
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  CreditCard credit_card = test::GetMaskedServerCard();
  browser_autofill_manager_->FillOrPreviewCreditCardForm(
      mojom::RendererFormDataAction::kFill, kDefaultPageID, form,
      form.fields[0], &credit_card);

  browser_autofill_manager_->OnCreditCardFetched(
      CreditCardFetchResult::kSuccess, &credit_card,
      /*cvc=*/u"123");

  ASSERT_TRUE(
      test_form_data_importer_->fetched_card_instrument_id().has_value());
  EXPECT_EQ(test_form_data_importer_->fetched_card_instrument_id().value(),
            credit_card.instrument_id());
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

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that for non-https forms with both address and credit card fields, we
// only return address suggestions. Instead of credit card suggestions, we
// should return a warning explaining that credit card profile suggestions are
// unavailable when the form is not https.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetAddressAndCreditCardSuggestionsNonHttps) {
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
  personal_data().ClearCreditCards();
  GetAutofillSuggestions(form, field);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ShouldShowAddressSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ShouldShowCreditCardSuggestionsIfAddressAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[0];

  GetAutofillSuggestions(form, field);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the correct section is filled.
TEST_F(BrowserAutofillManagerTest, FillTriggeredSection) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  size_t index_of_trigger_field = form.fields.size();
  test::CreateTestAddressFormData(&form);
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

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, form, form.fields[index_of_trigger_field],
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  // Extract the sections into individual forms to reduce boiler plate code.
  size_t mid = response_data.fields.size() / 2;
  FormData section1 = response_data;
  FormData section2 = response_data;
  section1.fields.erase(section1.fields.begin() + mid, section1.fields.end());
  section2.fields.erase(section2.fields.begin(), section2.fields.end() - mid);
  // First section should be empty, second should be filled.
  ExpectFilledForm(response_page_id, section1, kDefaultPageID, "", "", "", "",
                   "", "", "", "", "", "", "", "", "", "", "", true, false,
                   false);
  ExpectFilledAddressFormElvis(response_page_id, section2, kDefaultPageID,
                               false);
}

MATCHER_P(HasValue, value, "") {
  return arg.value == value;
}

// Test that if the form cache is outdated because a field has changed, filling
// is aborted after that field.
TEST_F(BrowserAutofillManagerTest, DoNotFillIfFormFieldChanged) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify |form| so that it doesn't match |form_structure| anymore.
  ASSERT_GE(form.fields.size(), 3u);
  for (auto it = form.fields.begin() + 2; it != form.fields.end(); ++it)
    *it = FormFieldData();

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);

  int response_query_id = 0;
  FormData response_data;
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _))
      .WillOnce((DoAll(testing::SaveArg<0>(&response_query_id),
                       testing::SaveArg<2>(&response_data),
                       testing::Return(std::vector<FieldGlobalId>{}))));
  browser_autofill_manager_->FillOrPreviewDataModelForm(
      mojom::RendererFormDataAction::kFill, kDefaultPageID, form,
      form.fields.front(), profile, nullptr, form_structure, autofill_field);
  std::vector<FormFieldData> filled_fields(response_data.fields.begin(),
                                           response_data.fields.begin() + 2);
  std::vector<FormFieldData> skipped_fields(response_data.fields.begin() + 2,
                                            response_data.fields.end());

  EXPECT_THAT(filled_fields, Each(Not(HasValue(u""))));
  EXPECT_THAT(skipped_fields, Each(HasValue(u"")));
}

// Test that if the form cache is outdated because a field was removed, filling
// is aborted.
TEST_F(BrowserAutofillManagerTest, DoNotFillIfFormFieldRemoved) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify |form| so that it doesn't match |form_structure| anymore.
  ASSERT_GE(form.fields.size(), 2u);
  form.fields.pop_back();

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);

  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
}

// Tests that BrowserAutofillManager ignores loss of focus events sent from the
// renderer if the renderer did not have a previously-interacted form.
// TODO(crbug.com/1140473): Remove this test when workaround is no longer
// needed.
TEST_F(BrowserAutofillManagerTest,
       ShouldIgnoreLossOfFocusWithNoPreviouslyInteractedForm) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->UpdatePendingForm(form);
  ASSERT_TRUE(browser_autofill_manager_->pending_form_data()->SameFormAs(form));

  // Receiving a notification that focus is no longer on the form *without* the
  // renderer having a previously-interacted form should not result in
  // any changes to the pending form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(
      /*had_interacted_form=*/false);
  EXPECT_TRUE(browser_autofill_manager_->pending_form_data()->SameFormAs(form));
}

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowCreditCardsSuggestionsIfCreditCardAutofillDisabled) {
  DisableAutofillViaAblation(scoped_feature_list_, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

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

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowAddressSuggestionsIfAddressAutofillDisabled) {
  DisableAutofillViaAblation(scoped_feature_list_, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Check that credit card suggestions will not be available.
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

struct LogAblationTestParams {
  const char* description;
  // Whether any autofillable data is stored.
  bool run_with_data_on_file = true;
  // If true, the credit card owner name field is filled with value that is not
  // a prefix of any stored credit card and then autofill suggestions are
  // queried a second time.
  bool second_query_for_suggestions_with_typed_prefix = false;
  // Whether the form should be submitted before validating the metrics.
  bool submit_form = true;
};

enum class LogAblationFormType {
  kAddress,
  kPayment,
  kMixed,  // address fields followed by payment fields
};

class BrowserAutofillManagerLogAblationTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<
          std::tuple<LogAblationTestParams, LogAblationFormType>> {
 public:
  BrowserAutofillManagerLogAblationTest() = default;
  ~BrowserAutofillManagerLogAblationTest() override = default;
};

// Validate that UMA logging works correctly for ablation studies.
INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerLogAblationTest,
    testing::Combine(
        testing::Values(
            // Test that if autofillable data is stored and the ablation is
            // enabled, we record metrics as expected.
            LogAblationTestParams{.description = "Having data to fill"},
            // Test that if NO autofillable data is stored and the ablation is
            // enabled, we record "UnconditionalAblation" metrics but no
            // "ConditionalAblation" metrics. The latter only recoded on the
            // condition that we have data to fill.
            LogAblationTestParams{.description = "Having NO data to fill",
                                  .run_with_data_on_file = false},
            // In this test we trigger the GetAutofillSuggestions call twice. By
            // the second time the user has typed a value that is not the prefix
            // of any existing autofill data. This means that autofill would not
            // create any suggestions. We still want to consider this a
            // conditional ablation (the condition to have fillable data on file
            // is met).
            LogAblationTestParams{
                .description = "Typed unknown prefix",
                .second_query_for_suggestions_with_typed_prefix = false},
            // Test that the right events are recorded in case the user
            // interacts with a form but does not submit it.
            LogAblationTestParams{.description = "No form submission",
                                  .submit_form = false}),
        testing::Values(LogAblationFormType::kAddress,
                        LogAblationFormType::kPayment,
                        LogAblationFormType::kMixed)));

TEST_P(BrowserAutofillManagerLogAblationTest, TestLogging) {
  const LogAblationTestParams& params = std::get<0>(GetParam());
  LogAblationFormType form_type = std::get<1>(GetParam());

  SCOPED_TRACE(testing::Message() << params.description << " Form type: "
                                  << static_cast<int>(form_type));

  if (!params.run_with_data_on_file) {
    personal_data().ClearAllServerData();
    personal_data().ClearAllLocalData();
  }

  DisableAutofillViaAblation(scoped_feature_list_, /*for_addresses=*/true,
                             /*for_credit_cards=*/true);
  TestAutofillTickClock clock(AutofillTickClock::NowTicks());
  base::HistogramTester histogram_tester;

  // Set up our form data. In the kMixed case the form will contain the fields
  // of an address form followed by the fields of fields of a payment form. The
  // triggering for autofill suggestions will happen on an address field in this
  // case.
  FormData form;
  if (form_type == LogAblationFormType::kAddress ||
      form_type == LogAblationFormType::kMixed) {
    test::CreateTestAddressFormData(&form);
  }
  if (form_type == LogAblationFormType::kPayment ||
      form_type == LogAblationFormType::kMixed) {
    CreateTestCreditCardFormData(&form, true, false);
  }
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Simulate retrieving autofill suggestions with the first field as a trigger
  // script. This should emit signals that lead to recorded metrics later on.
  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Simulate user typing into field (due to the ablation we would not fill).
  field.value = u"Unknown User";
  browser_autofill_manager_->OnTextFieldDidChange(
      form, field, gfx::RectF(), AutofillTickClock::NowTicks());

  if (params.second_query_for_suggestions_with_typed_prefix) {
    // Do another lookup. We won't have any suggestions because they would not
    // be compatible with the "Unknown User" username.
    GetAutofillSuggestions(form, field);
  }

  // Advance time and possibly submit the form.
  base::TimeDelta time_delta = base::Seconds(42);
  clock.Advance(time_delta);
  if (params.submit_form)
    FormSubmitted(form);

  // Flush FormEventLoggers.
  browser_autofill_manager_->Reset();

  // Validate the recorded metrics.
  std::string form_type_str = (form_type == LogAblationFormType::kAddress ||
                               form_type == LogAblationFormType::kMixed)
                                  ? "Address"
                                  : "CreditCard";

  // If data was on file, we expect conditional ablation metrics.
  if (params.run_with_data_on_file) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*sample=*/params.submit_form ? 1 : 0,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*count=*/0);
  }
  // Only if data was on file an a submission happened, we can record the
  // duration from interaction to submission.
  if (params.run_with_data_on_file && params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        /*count=*/0);
  }
  // The unconditional ablation metrics should always be logged as this the
  // ablation study is always enabled.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
          ".UnconditionalAblation",
      /*sample=*/params.submit_form ? 1 : 0,
      /*expected_bucket_count=*/1);
  // Expect a time from interaction to submission the form was submitted.
  if (params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        /*count=*/0);
  }

  // Ensure that no metrics are recorded for the complementary form type.
  // I.e. if the trigger element is of an address form, the credit card form
  // should have no metrics.
  std::string complementary_form_type_str =
      (form_type == LogAblationFormType::kAddress ||
       form_type == LogAblationFormType::kMixed)
          ? "CreditCard"
          : "Address";
  for (const char* metric :
       {"FormSubmissionAfterInteraction", "FillDurationSinceInteraction"}) {
    for (const char* ablation_type :
         {"UnconditionalAblation", "ConditionalAblation"}) {
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Autofill.Ablation.", metric, ".",
                        complementary_form_type_str.c_str(), ".",
                        ablation_type}),
          /*count=*/0);
    }
  }
}

// Test that we properly match typed values to stored state data.
TEST_F(BrowserAutofillManagerTest, DetermineStateFieldTypeForUpload) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillUseAlternativeStateNameMap);

  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillProfile profile;
  test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "Bavaria", "",
                       "DE", "");

  const char* const kValidMatches[] = {"by", "Bavaria", "Bayern",
                                       "BY", "B.Y",     "B-Y"};
  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    FormData form;
    FormFieldData field;

    test::CreateTestFormField("Name", "Name", "Test", "text", &field);
    form.fields.push_back(field);

    test::CreateTestFormField("State", "state", valid_match, "text", &field);
    form.fields.push_back(field);

    FormStructure form_structure(form);
    EXPECT_EQ(form_structure.field_count(), 2U);

    browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
        {profile}, &form_structure);
    EXPECT_TRUE(form_structure.field(1)->state_is_a_matching_type());
  }

  const char* const kInvalidMatches[] = {"Garbage", "BYA",   "BYA is a state",
                                         "Bava",    "Empty", ""};
  for (const char* invalid_match : kInvalidMatches) {
    SCOPED_TRACE(invalid_match);
    FormData form;
    FormFieldData field;

    test::CreateTestFormField("Name", "Name", "Test", "text", &field);
    form.fields.push_back(field);

    test::CreateTestFormField("State", "state", invalid_match, "text", &field);
    form.fields.push_back(field);

    FormStructure form_structure(form);
    EXPECT_EQ(form_structure.field_count(), 2U);

    browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
        {profile}, &form_structure);
    EXPECT_FALSE(form_structure.field(1)->state_is_a_matching_type());
  }

  test::PopulateAlternativeStateNameMapForTesting(
      "US", "California",
      {{.canonical_name = "California",
        .abbreviations = {"CA"},
        .alternative_names = {}}});

  test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "California",
                       "", "US", "");

  FormData form;
  FormFieldData field;

  test::CreateTestFormField("Name", "Name", "Test", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("State", "state", "CA", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  EXPECT_EQ(form_structure.field_count(), 2U);

  browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
      {profile}, &form_structure);
  EXPECT_TRUE(form_structure.field(1)->state_is_a_matching_type());
}

// Ensures that if autofill is disabled but the password manager is enabled,
// Autofill still performs a lookup to the server.
TEST_F(BrowserAutofillManagerTest,
       OnFormsSeen_AutofillDisabledPasswordManagerEnabled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);

  // Disable autofill and the password manager.
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  ON_CALL(autofill_client_, IsPasswordManagerEnabled())
      .WillByDefault(Return(false));

  // As neither autofill nor password manager are enabled, the form should
  // not be parsed.
  {
    base::HistogramTester histogram_tester;
    FormsSeen(forms);
    EXPECT_EQ(0, histogram_tester.GetBucketCount("Autofill.UserHappiness",
                                                 0 /* FORMS_LOADED */));
  }

  // Now enable the password manager.
  ON_CALL(autofill_client_, IsPasswordManagerEnabled())
      .WillByDefault(Return(true));
  // If the password manager is enabled, that's enough to parse the form.
  {
    base::HistogramTester histogram_tester;
    FormsSeen(forms);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        0 /* FORMS_LOADED */, 1);
    download_manager_->VerifyLastQueriedForms(forms);
  }
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

// Test that nothing breaks when there are single field form fill (Autocomplete)
// suggestions but no autofill suggestions.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetFieldSuggestionsForSingleFieldFormFillOnly) {
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
  std::vector<std::u16string> suggestions;
  suggestions.push_back(u"one");
  suggestions.push_back(u"two");
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
  personal_data().AddProfile(profile);

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = u"Elvis";
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
  profile.SetInfo(NAME_FULL, u"Natty Bumppo", "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800PRAIRIE");
  personal_data().AddProfile(profile);

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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_ForPhonePrefixOrSuffix) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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

  personal_data().ClearProfiles();
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000104");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800FLOWERS");
  personal_data().AddProfile(profile);

  const FormFieldData& phone_prefix = form.fields[2];
  GetAutofillSuggestions(form, phone_prefix);

  // Test that we sent the right prefix values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("356", "1800FLOWERS", "", 1));

  const FormFieldData& phone_suffix = form.fields[3];
  GetAutofillSuggestions(form, phone_suffix);

  // Test that we sent the right suffix values to the external delegate.
  CheckSuggestions(kDefaultPageID, Suggestion("9377", "1800FLOWERS", "", 1));
}

// Tests that the suggestion consists of phone number without the country code
// when a length limit is imposed in the field due to which filling with
// country code is not possible.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_ForPhoneField) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[9].max_length = 10;
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetInfo(NAME_FULL, u"Natty Bumppo", "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+886123456789");
  personal_data().ClearProfiles();
  personal_data().AddProfile(profile);

  const FormFieldData& field = form.fields[9];
  GetAutofillSuggestions(form, field);

  CheckSuggestions(kDefaultPageID,
                   Suggestion("123456789", "Natty Bumppo", "", 1));
}

// Tests that we return email profile suggestions values
// when the email field with username autocomplete attribute exist.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetProfileSuggestions_ForEmailFieldWithUserNameAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"First Name", "firstname", 30, "given-name"},
                     {"Last Name", "lastname", 30, "family-name"},
                     {"Email", "email", 30, "username"},
                     {"Password", "password", 30, "new-password"}};

  FormFieldData field;
  for (const auto& test_field : test_fields) {
    const char* const field_type =
        strcmp(test_field.name, "password") == 0 ? "password" : "text";
    test::CreateTestFormField(test_field.label, test_field.name, "", field_type,
                              &field);
    field.max_length = test_field.max_length;
    field.autocomplete_attribute = test_field.autocomplete_attribute;
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  personal_data().ClearProfiles();
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetRawInfo(NAME_FULL, u"Natty Bumppo");
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  personal_data().AddProfile(profile);

  GetAutofillSuggestions(form, form.fields[2]);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("test@example.com", "Natty Bumppo", "", 1));
}

// Test that we correctly fill an address form.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  EXPECT_EQ(2U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());
}

TEST_P(BrowserAutofillManagerStructuredProfileTest, WillFillCreditCardNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData* number_field = nullptr;
  FormFieldData* name_field = nullptr;
  FormFieldData* month_field = nullptr;
  for (auto& field : form.fields) {
    if (field.name == u"cardnumber")
      number_field = &field;
    else if (field.name == u"nameoncard")
      name_field = &field;
    else if (field.name == u"ccmonth")
      month_field = &field;
  }

  // Empty form - whole form is Autofilled.
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));

  // If the user has entered a value, it won't be overridden.
  number_field->value = u"gibberish";
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *month_field));

  // But if that value is removed, it will be Autofilled.
  number_field->value.clear();
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));

  // When the number is already autofilled, we won't fill it.
  number_field->is_autofilled = true;
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *month_field));

  // If another field is filled, we would still fill other non-filled fields in
  // the section.
  number_field->is_autofilled = false;
  name_field->is_autofilled = true;
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));
}

// Test that we correctly log FIELD_WAS_AUTOFILLED event in UserHappiness.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_LogFieldWasAutofill) {
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
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  // Cardholder name, card number, expiration data were autofilled but cvc was
  // not be autofilled.
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::FIELD_WAS_AUTOFILLED, 3);
}

// Test that we correctly fill a credit card form.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FillCreditCardForm_Simple) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that whitespace is stripped from the credit card number.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_StripCardNumberWhitespace) {
  // Same as the SetUp(), but generate Elvis card with whitespace in credit
  // card number.  |credit_card| will be owned by the TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000008");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000008";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that separator characters are stripped from the credit card number.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_StripCardNumberSeparators) {
  // Same as the SetUp(), but generate Elvis card with separator characters in
  // credit card number.  |credit_card| will be owned by the
  // TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234-5678-9012-3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that we correctly fill a credit card form with month input type.
// Test 1 of 4: Empty month, empty year
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_NoYearNoMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 2 of 4: Non-empty month, empty year
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_NoYearMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// Test 3 of 4: Empty month, non-empty year
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_YearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 4 of 4: Non-empty month, non-empty year
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_YearMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "04");
}

// Test that only the first 16 credit card number fields are filled.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillOnlyFirstNineteenCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 credit card number fields with distinct names.
  for (int i = 0; i < 20; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    test::CreateTestFormField(base::UTF16ToASCII(field_name).c_str(),
                              "cardnumber", "", "text", &field);
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 credit card number fields are filled.
  for (int i = 0; i < 19; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    ExpectFilledField(base::UTF16ToASCII(field_name).c_str(), "cardnumber",
                      "4234567890123456", "text", response_data.fields[2 + i]);
  }

  // Verify that the 20th. credit card number field is not filled.
  ExpectFilledField("Card Number 20", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test that only the first 16 of identical fields are filled.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillOnlyFirstSixteenIdenticalCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  for (int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                      response_data.fields[2 + i]);
  }
  // Verify that the 20th. card number field is not filled.
  ExpectFilledField("Card Number", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test the credit card number is filled correctly into single-digit fields.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardNumberIntoSingleDigitFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    // Limit the length the field to 1.
    field.max_length = 1;
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  std::u16string card_number = u"4234567890123456";
  for (unsigned int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber",
                      i < card_number.length()
                          ? base::UTF16ToASCII(card_number.substr(i, 1)).c_str()
                          : "4234567890123456",
                      "text", response_data.fields[2 + i]);
  }

  // Verify that the 20th. card number field is contains the full value.
  ExpectFilledField("Card Number", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_SplitName) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
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
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[2]);
}

// Test that only filled selection boxes are counted for the type filling limit.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnlyCountFilledSelectionBoxesForTypeFillingLimit) {
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

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  // Create a selection box for the state that hat the correct entry to be
  // filled with user data. Note, TN is the official abbreviation for Tennessee.
  test::CreateTestSelectField("State", "state", "", {"AA", "BB", "TN"},
                              {"AA", "BB", "TN"}, 3, &field);
  form.fields.push_back(field);

  // Add 20 selection boxes that can not be filled since the correct entry
  // is missing.
  for (int i = 0; i < 20; i++) {
    test::CreateTestSelectField("State", "state", "", {"AA", "BB", "CC"},
                                {"AA", "BB", "CC"}, 3, &field);
    form.fields.push_back(field);
  }

  // Add 20 other selection boxes that should be fillable since the correct
  // entry is present.
  for (int i = 0; i < 20; i++) {
    test::CreateTestSelectField("State", "state", "", {"AA", "BB", "TN"},
                                {"AA", "BB", "TN"}, 3, &field);
    form.fields.push_back(field);
  }

  // Create a selection box for the state that hat the correct entry to be
  // filled with user data. Note, TN is the official abbreviation for Tennessee.
  for (int i = 0; i < 20; ++i) {
    test::CreateTestSelectField("Country", "country", "", {"DE", "FR", "US"},
                                {"DE", "FR", "US"}, 3, &field);
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000123";
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "1987", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile.set_guid(guid);
  personal_data().AddProfile(profile);

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);

  // Verify the correct filling of the name entries.
  ExpectFilledField("First Name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle Name", "middlename", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last Name", "lastname", "Presley", "text",
                    response_data.fields[2]);

  // Verify that the first selection box is correctly filled.
  ExpectFilledField("State", "state", "TN", "select-one",
                    response_data.fields[3]);

  // Verify that the next 20 selection boxes are not filled.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "", "select-one",
                      response_data.fields[4 + i]);
  }

  // Verify that the remaining selection boxes are correctly filled again
  // because there's no limit on filling ADDRESS_HOME_STATE fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "TN", "select-one",
                      response_data.fields[24 + i]);
  }

  // Verify that only the first 9 of the remaining selection boxes are
  // correctly filled due to the limit on filling ADDRESS_HOME_COUNTRY fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("Country", "country", i < 9 ? "US" : "", "select-one",
                      response_data.fields[44 + i]);
  }
}

// Test that we correctly fill a combined address and credit card form.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillAddressAndCreditCardForm) {
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
                                       MakeFrontendId(std::string(), guid),
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
                                       MakeFrontendId(guid2, std::string()),
                                       &response_page_id, &response_data);
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }
}

// Test that a field with an unrecognized autocomplete attribute is not filled.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillAddressForm_UnrecognizedAttribute) {
  FormData address_form;
  address_form.name = u"MyForm";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);

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
// to off are filled on all platforms when the feature to autofill all addresses
// is enabled (default).
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillAddressForm_AutocompleteOffNotRespected) {
  FormData address_form;
  address_form.name = u"MyForm";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillAddressForm_CompanyBirthyear) {
  // Set up our form data.
  FormData address_form;
  address_form.name = u"MyForm";
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
  personal_data().AddProfile(profile);

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, *address_form.fields.begin(),
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillAddressForm_PlaceholderEqualsValue) {
  FormData address_form;
  address_form.name = u"MyForm";
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set the same placeholder and value for each field.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.placeholder = u"First Name";
  field.value = u"First Name";
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.placeholder = u"Middle Name";
  field.value = u"Middle Name";
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.placeholder = u"Last Name";
  field.value = u"Last Name";
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
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
                                     MakeFrontendId(guid, std::string()),
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_AutocompleteOff) {
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
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &response_data);

  // All fields should be filled.
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillCreditCardForm_ExpiredCard) {
  personal_data().ClearCreditCards();
  CreditCard expired_card;
  test::SetCreditCardInfo(&expired_card, "Homer Simpson",
                          "4234567890654321",  // Visa
                          "05", "2000", "1");
  expired_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data().AddCreditCard(expired_card);

  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
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
                                     MakeFrontendId(guid, std::string()),
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       PreviewCreditCardForm_VirtualCard) {
  personal_data().ClearCreditCards();
  CreditCard virtual_card = test::GetVirtualCard();
  personal_data().AddServerCreditCard(virtual_card);
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  int response_page_id = 0;
  FormData response_data;
  PreviewVirtualCardDataAndSaveResults(
      mojom::RendererFormDataAction::kPreview, virtual_card.guid(),
      kDefaultPageID, form, form.fields[1], &response_page_id, &response_data);

  std::u16string expected_cardholder_name = u"Lorem Ipsum";
  // Virtual card number using obfuscated dots only: Virtual card Mastercard
  // ••••4444
  std::u16string expected_card_number =
      u"Virtual card Mastercard  " + virtual_card.ObfuscatedLastFourDigits();
  // Virtual card expiration month using obfuscated dots: ••
  std::u16string expected_exp_month = CreditCard::GetMidlineEllipsisDots(2);
  // Virtual card expiration year using obfuscated dots: ••••
  std::u16string expected_exp_year = CreditCard::GetMidlineEllipsisDots(4);
  // Virtual card cvc using obfuscated dots: •••
  std::u16string expected_cvc = CreditCard::GetMidlineEllipsisDots(3);

  EXPECT_EQ(response_data.fields[0].value, expected_cardholder_name);
  EXPECT_EQ(response_data.fields[1].value, expected_card_number);
  EXPECT_EQ(response_data.fields[2].value, expected_exp_month);
  EXPECT_EQ(response_data.fields[3].value, expected_exp_year);
  EXPECT_EQ(response_data.fields[4].value, expected_cvc);
}

// Test that non-focusable field is ignored while inferring boundaries between
// sections, but not filled.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFormWithNonFocusableFields) {
  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = u"MyForm";
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
                                     MakeFrontendId(std::string(), guid),
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  const size_t kAddressFormSize = form.fields.size();
  test::CreateTestAddressFormData(&form);
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + u"_";
  }
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the first section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(std::u16string(), response_data.fields[i].value);
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
      MakeFrontendId(std::string(), guid2), &response_page_id, &response_data);
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
    ExpectFilledAddressFormElvis(response_page_id, secondSection, kPageID2,
                                 false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = u"MyForm";
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
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(kDefaultPageID, response_page_id);
    EXPECT_EQ(u"MyForm", response_data.name);
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
                                     MakeFrontendId(std::string(), guid2),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(kPageID2, response_page_id);
    EXPECT_EQ(u"MyForm", response_data.name);
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
      MakeFrontendId(guid3, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(kPageID3, response_page_id);
    EXPECT_EQ(u"MyForm", response_data.name);
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFormWithMultipleEmails) {
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
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);

  // The second email address should be filled.
  EXPECT_EQ(u"theking@gmail.com", response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark the address fields as autofilled.
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(std::string(), guid),
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
                                     MakeFrontendId(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  const int kPageID3 = 3;
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      MakeFrontendId(guid2, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(response_page_id, response_data, kPageID3, "", "", "", "",
                     "", "", "", "", "", "", "", "", "", "", "2999", true, true,
                     false);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FillPartlyAutofilledForm) {
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
                                     MakeFrontendId(std::string(), guid),
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
                                     MakeFrontendId(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillPartlyManuallyFilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

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
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_page_id, response_data, kDefaultPageID, "Elvis",
                     "Aaron", "Jackson", "3734 Elvis Presley Blvd.", "Apt. 10",
                     "Memphis", "Tennessee", "38116", "United States",
                     "12345678901", "theking@gmail.com", "", "", "", "", true,
                     true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                     MakeFrontendId(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledForm(response_page_id, response_data, kPageID2, "Michael", "",
                     "Jackson", "", "", "", "", "", "", "", "", "Elvis Presley",
                     "4234567890123456", "04", "2999", true, true, false);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_us_number_max_length.name = u"MyMaxlengthPhoneForm";
  form_with_us_number_max_length.url =
      GURL("https://myform.com/phone_form.html");
  form_with_us_number_max_length.action =
      GURL("https://myform.com/phone_submit.html");
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.unique_renderer_id = test::MakeFormRendererId();
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
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data1);
  EXPECT_EQ(1, response_page_id);

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(u"1", response_data1.fields[0].value);
  EXPECT_EQ(u"650", response_data1.fields[1].value);
  EXPECT_EQ(u"555", response_data1.fields[2].value);
  EXPECT_EQ(u"4567", response_data1.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data1.fields[4].value);

  page_id = 2;
  response_page_id = 0;
  FormData response_data2;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data2);
  EXPECT_EQ(2, response_page_id);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(u"1", response_data2.fields[0].value);
  EXPECT_EQ(u"650", response_data2.fields[1].value);
  EXPECT_EQ(u"555", response_data2.fields[2].value);
  EXPECT_EQ(u"4567", response_data2.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data2.fields[4].value);

  // For other countries, fill prefix and suffix fields with best effort.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  page_id = 3;
  response_page_id = 0;
  FormData response_data3;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data3);
  EXPECT_EQ(3, response_page_id);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(u"4", response_data3.fields[0].value);
  EXPECT_EQ(u"700", response_data3.fields[1].value);
  EXPECT_EQ(u"95", response_data3.fields[2].value);
  EXPECT_EQ(u"4321", response_data3.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data3.fields[4].value);

  page_id = 4;
  response_page_id = 0;
  FormData response_data4;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data4);
  EXPECT_EQ(4, response_page_id);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(u"44", response_data4.fields[0].value);
  EXPECT_EQ(u"7700", response_data4.fields[1].value);
  EXPECT_EQ(u"95", response_data4.fields[2].value);
  EXPECT_EQ(u"4321", response_data4.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data4.fields[4].value);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_ComponentizedNumbers) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_WholeNumbers) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_FillPartsOnceOnly) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_NotFillMisclassifiedExtention) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_misclassified_extension;
  form_with_misclassified_extension.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_misclassified_extension.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_misclassified_extension.name =
      u"complete_phone_form_with_extension";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_BestEfforFilling) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_no_complete_number;
  form_with_no_complete_number.unique_renderer_id = test::MakeFormRendererId();
  form_with_no_complete_number.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_no_complete_number.name = u"no_complete_phone_form";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_FocusOnSecondPhoneNumber) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

  // Verify when the second phone number field is being focused, we fill
  // that field *AND* the first phone number field.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_HiddenFieldShouldNotCount) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormWithHiddenOrPresentationalSelects) {
  FormData form;
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
                                     MakeFrontendId(std::string(), guid),
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillFirstPhoneNumber_MultipleSectionFilledCorrectly) {
  AutofillProfile* work_profile = personal_data().GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_sections;
  form_with_multiple_sections.unique_renderer_id = test::MakeFormRendererId();
  form_with_multiple_sections.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_sections.name = u"multiple_section_fields";
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
      MakeFrontendId(std::string(), guid), &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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

  FillAutofillFormDataAndSaveResults(page_id, form_with_multiple_sections, *it,
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  EXPECT_EQ(1, response_page_id);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormFieldData field;
  test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  FormsSeen({form});

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FormChangesAddField) {
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

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we can still fill a form when the visibility of some fields
// changes.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormChangesVisibilityOfFields) {
  // Set up our form data.
  FormData form;
  form.unique_renderer_id = test::MakeFormRendererId();
  form.url = GURL("https://www.foo.com/");

  FormFieldData field;

  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form.name = u"multiple_groups_fields";
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
                                     MakeFrontendId(std::string(), guid),
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
                                     MakeFrontendId(std::string(), guid2),
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
TEST_P(BrowserAutofillManagerStructuredProfileTest, FormSubmitted) {
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
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test that we are saving form data when the FormSubmitted event is sent.
TEST_P(BrowserAutofillManagerStructuredProfileTest, FormSubmittedSaveData) {
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
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  browser_autofill_manager_->OnFormSubmitted(response_data, false,
                                             SubmissionSource::FORM_SUBMISSION);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test that when Autocomplete is enabled and Autofill is disabled, form
// submissions are still received by the SingleFieldFormFillRouter.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormSubmittedAutocompleteEnabled) {
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  EXPECT_CALL(*single_field_form_fill_router_, OnWillSubmitForm(_, _, true));
  FormSubmitted(form);
}

// Test that the value patterns metric is reported.
TEST_P(BrowserAutofillManagerStructuredProfileTest, ValuePatternsMetric) {
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
    form.name = u"my-form";
    form.url = GURL("https://myform.com/form.html");
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

// Test that when Autofill is disabled, single field form fill suggestions are
// still queried as a fallback.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       SingleFieldFormFillSuggestions_SomeWhenAutofillDisabled) {
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];

  // Expect the SingleFieldFormFillRouter to be called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, field);

  // Single field form fill suggestions were returned, so we should not go
  // through the normal autofill flow.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we do not query for single field form fill suggestions when there
// are Autofill suggestions available.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       SingleFieldFormFillSuggestions_NoneWhenAutofillPresent) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];

  // SingleFieldFormFillRouter is not called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we query for single field form fill suggestions when there are no
// Autofill suggestions available.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       SingleFieldFormFillSuggestions_SomeWhenAutofillEmpty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Single field form fill manager is called for suggestions because Autofill
  // is empty.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card name
// field, single field form fill suggestions are queried.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       SingleFieldFormFillSuggestions_CreditCardNameFieldShouldAutocomplete) {
  TestAutofillClient client;
  // Since we are testing a form that submits over HTTP, we also need to set
  // the main frame to HTTP in the client, otherwise mixed form warnings will
  // trigger and autofill will be disabled.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  client.set_form_origin(client.form_origin().ReplaceComponents(replacements));
  ResetBrowserAutofillManager(&client);
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The first field is "Name on card", which should autocomplete.
  FormFieldData field = form.fields[0];
  field.should_autocomplete = true;

  // SingleFieldFormFillRouter is called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card number
// field, single field form fill suggestions are not queried.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       SingleFieldFormFillSuggestions_CreditCardNumberShouldNotAutocomplete) {
  TestAutofillClient client;
  // Since we are testing a form that submits over HTTP, we also need to set
  // the main frame to HTTP in the client, otherwise mixed form warnings will
  // trigger and autofill will be disabled.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  client.set_form_origin(client.form_origin().ReplaceComponents(replacements));
  ResetBrowserAutofillManager(&client);
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The second field is "Card Number", which should not autocomplete.
  FormFieldData field = form.fields[1];
  field.should_autocomplete = true;

  // SingleFieldFormFillRouter is not called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that the situation where there are no Autofill suggestions available,
// and no single field form fill conditions were met is correctly handled. The
// single field form fill conditions were not met because autocomplete is set to
// off and the field is not recognized as a promo code field.
TEST_F(
    BrowserAutofillManagerTest,
    SingleFieldFormFillSuggestions_NoneWhenAutofillEmptyAndSingleFieldFormFillConditionsNotMet) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  field.should_autocomplete = false;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete is set to off, so suggestions should not get returned from
  // |single_field_form_fill_router_|.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .WillRepeatedly(testing::Return(false));

  GetAutofillSuggestions(form, field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the situation where no single field form fill conditions were met
// is handled correctly. The single field form fill conditions were not met
// because autocomplete is set to off and the field is not recognized as a promo
// code field.
TEST_P(
    BrowserAutofillManagerStructuredProfileTest,
    SingleFieldFormFillSuggestions_NoneWhenSingleFieldFormFillConditionsNotMet) {
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData* field = &form.fields[0];
  field->should_autocomplete = false;

  // Autocomplete is set to off, so suggestions should not get returned from
  // |single_field_form_fill_router_|.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .WillRepeatedly(testing::Return(false));

  GetAutofillSuggestions(form, *field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DestructorCancelsSingleFieldFormFillQueries) {
  EXPECT_CALL(*single_field_form_fill_router_, CancelPendingQueries).Times(1);
  browser_autofill_manager_.reset();
}

// Make sure that we don't error out when AutocompleteHistoryManager was
// destroyed before BrowserAutofillManager.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       Destructor_DeletedAutocomplete_Works) {
  // The assertion here is that no exceptions will be thrown.
  autocomplete_history_manager_.reset();
  browser_autofill_manager_.reset();
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form from the API and apply type predictions.
// What we test here:
//  * The API response parser is used.
//  * The query can be processed with a response from the API.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnLoadedServerPredictionsFromApi) {
  // First form on the page.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  // |form_structure_instance| will be owned by |browser_autofill_manager_|.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance));

  // Second form on the page.
  FormData form2;
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"MyForm2";
  form2.url = GURL("https://myform.com/form.html");
  form2.action = GURL("https://myform.com/submit.html");
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form2.fields.push_back(field);
  auto form_structure_instance2 = std::make_unique<FormStructure>(form2);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure2 = form_structure_instance2.get();
  form_structure2->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance2));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form 1.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldSuggestionToForm(form.fields[0], ADDRESS_HOME_CITY,
                                           form_suggestion);
  autofill::test::AddFieldSuggestionToForm(form.fields[1], ADDRESS_HOME_STATE,
                                           form_suggestion);
  autofill::test::AddFieldSuggestionToForm(form.fields[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);
  // Set suggestions for form 2.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldSuggestionToForm(form2.fields[0], NAME_LAST,
                                           form_suggestion);
  autofill::test::AddFieldSuggestionToForm(form2.fields[1], NAME_MIDDLE,
                                           form_suggestion);
  autofill::test::AddFieldSuggestionToForm(form2.fields[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);

  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures({form_structure, form_structure2});

  // Run method under test.
  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      encoded_response_string, signatures);

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
// BrowserAutofillManager has been reset between the time the query was sent and
// the response received.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnLoadedServerPredictions_ResetManager) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |browser_autofill_manager_|.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures(*form_structure);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  for (int i = 0; i < 7; ++i) {
    form_suggestion->add_field_suggestions()->add_predictions()->set_type(0);
  }
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(2);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(61);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::string response_string_base64;
  base::Base64Encode(response_string, &response_string_base64);

  // Reset the manager (such as during a navigation).
  browser_autofill_manager_->Reset();

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      response_string_base64, signatures);

  // Verify that FormStructure::ParseQueryResponse was NOT called.
  histogram_tester.ExpectTotalCount("Autofill.ServerQueryResponse", 0);
}

// Test that when server predictions disagree with the heuristic ones, the
// overall types and sections would be set based on the server one.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DetermineHeuristicsWithOverallPrediction) {
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
  // |form_structure| will be owned by |browser_autofill_manager_|.
  FormStructure* form_structure = [&] {
    auto form_structure = std::make_unique<FormStructure>(form);
    FormStructure* ptr = form_structure.get();
    form_structure->DetermineHeuristicTypes(nullptr, nullptr);
    browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));
    return ptr;
  }();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldSuggestionToForm(
      form.fields[0], CREDIT_CARD_NAME_FIRST, form_suggestion);
  autofill::test::AddFieldSuggestionToForm(
      form.fields[1], CREDIT_CARD_NAME_LAST, form_suggestion);
  autofill::test::AddFieldSuggestionToForm(form.fields[2], CREDIT_CARD_NUMBER,
                                           form_suggestion);
  autofill::test::AddFieldSuggestionToForm(
      form.fields[3], CREDIT_CARD_EXP_MONTH, form_suggestion);
  autofill::test::AddFieldSuggestionToForm(
      form.fields[4], CREDIT_CARD_EXP_4_DIGIT_YEAR, form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::string response_string_base64;
  base::Base64Encode(response_string, &response_string_base64);

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      response_string_base64, test::GetEncodedSignatures(*form_structure));
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
TEST_P(BrowserAutofillManagerStructuredProfileTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen(std::vector<FormData>(1, form));

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |browser_autofill_manager_|.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->heuristic_type());
  }
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test that we are able to save form data after the possible types have been
// determined. We do two submissions and verify that only at the second
// submission are the possible types able to be inferred.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormSubmittedPossibleTypesTwoSubmissions) {
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
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  personal_data().ClearProfiles();
  // The default credit card is a Elvis card. It must be removed because name
  // fields would be detected. However at least one profile or card is needed to
  // start the upload process, which is why this other card is created.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  ASSERT_EQ(0u, personal_data().GetProfiles().size());

  // Simulate form submission. The first submission should not count the data
  // towards possible types. Therefore we expect all UNKNOWN_TYPE entries.
  ServerFieldTypeSet type_set;
  type_set.insert(UNKNOWN_TYPE);
  std::vector<ServerFieldTypeSet> unknown_types(expected_types.size(),
                                                type_set);
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(unknown_types);
  FormSubmitted(response_data);
  ASSERT_EQ(1u, personal_data().GetProfiles().size());

  // The second submission should now have data by which to infer types.
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  FormSubmitted(response_data);
  ASSERT_EQ(1u, personal_data().GetProfiles().size());
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormSubmittedWithDifferentFields) {
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
  EXPECT_EQ(signature, browser_autofill_manager_->GetSubmittedFormSignature());
}

// Test that we do not save form data when submitted fields contain default
// values.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormSubmittedWithDefaultValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[3].value = u"Enter your address";

  // Convert the state field to a <select> popup, to make sure that we only
  // reject default values for text fields.
  ASSERT_TRUE(form.fields[6].name == u"state");
  form.fields[6].form_control_type = "select-one";
  form.fields[6].value = u"Tennessee";

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[3],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);

  // Simulate form submission.  We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());

  // Set the address field's value back to the default value.
  response_data.fields[3].value = u"Enter your address";

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

struct ProfileMatchingTypesTestCase {
  const char* input_value;         // The value to input in the field.
  ServerFieldTypeSet field_types;  // The expected field types to be determined.
  ServerFieldTypeSet
      structured_field_types;  // The expected field types to be determined.
};

class ProfileMatchingTypesTest
    : public BrowserAutofillManagerTest,
      public ::testing::WithParamInterface<
          std::tuple<ProfileMatchingTypesTestCase,
                     bool>> {  // kAutofillEnableSupportForMoreStructureInNames
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    InitializeFeatures();
  }

  bool StructuredNamesAndAddresses() const {
    return structured_names_and_addresses_;
  }

  void InitializeFeatures();

 private:
  bool structured_names_and_addresses_;
  base::test::ScopedFeatureList scoped_features_;
};

void ProfileMatchingTypesTest::InitializeFeatures() {
  structured_names_and_addresses_ = std::get<1>(GetParam());

  std::vector<base::Feature> features = {
      features::kAutofillEnableSupportForMoreStructureInAddresses,
      features::kAutofillEnableSupportForMoreStructureInNames};
  if (structured_names_and_addresses_) {
    scoped_features_.InitWithFeatures(features, {});
  } else {
    scoped_features_.InitWithFeatures({}, features);
  }
}

const ProfileMatchingTypesTestCase kProfileMatchingTypesTestCases[] = {
    // Profile fields matches.
    {"Elvis", {NAME_FIRST}, {NAME_FIRST}},
    {"Aaron", {NAME_MIDDLE}, {NAME_MIDDLE}},
    {"A", {NAME_MIDDLE_INITIAL}, {NAME_MIDDLE_INITIAL}},
    {"Presley", {NAME_LAST}, {NAME_LAST, NAME_LAST_SECOND}},
    {"Elvis Aaron Presley", {NAME_FULL}, {NAME_FULL}},
    {"theking@gmail.com", {EMAIL_ADDRESS}, {EMAIL_ADDRESS}},
    {"RCA", {COMPANY_NAME}, {COMPANY_NAME}},
    {"3734 Elvis Presley Blvd.", {ADDRESS_HOME_LINE1}, {ADDRESS_HOME_LINE1}},
    {"3734", {UNKNOWN_TYPE}, {ADDRESS_HOME_HOUSE_NUMBER}},
    {"Elvis Presley Blvd.", {UNKNOWN_TYPE}, {ADDRESS_HOME_STREET_NAME}},
    {"Apt. 10",
     {ADDRESS_HOME_LINE2},
     {ADDRESS_HOME_LINE2, ADDRESS_HOME_SUBPREMISE}},
    {"Memphis", {ADDRESS_HOME_CITY}, {ADDRESS_HOME_CITY}},
    {"Tennessee", {ADDRESS_HOME_STATE}, {ADDRESS_HOME_STATE}},
    {"38116", {ADDRESS_HOME_ZIP}, {ADDRESS_HOME_ZIP}},
    {"USA", {ADDRESS_HOME_COUNTRY}, {ADDRESS_HOME_COUNTRY}},
    {"United States", {ADDRESS_HOME_COUNTRY}, {ADDRESS_HOME_COUNTRY}},
    {"12345678901", {PHONE_HOME_WHOLE_NUMBER}, {PHONE_HOME_WHOLE_NUMBER}},
    {"+1 (234) 567-8901", {PHONE_HOME_WHOLE_NUMBER}, {PHONE_HOME_WHOLE_NUMBER}},
    {"(234)567-8901",
     {PHONE_HOME_CITY_AND_NUMBER},
     {PHONE_HOME_CITY_AND_NUMBER}},
    {"2345678901", {PHONE_HOME_CITY_AND_NUMBER}, {PHONE_HOME_CITY_AND_NUMBER}},
    {"1", {PHONE_HOME_COUNTRY_CODE}, {PHONE_HOME_COUNTRY_CODE}},
    {"234", {PHONE_HOME_CITY_CODE}, {PHONE_HOME_CITY_CODE}},
    {"5678901", {PHONE_HOME_NUMBER}, {PHONE_HOME_NUMBER}},
    {"567", {PHONE_HOME_NUMBER_PREFIX}, {PHONE_HOME_NUMBER_PREFIX}},
    {"8901", {PHONE_HOME_NUMBER_SUFFIX}, {PHONE_HOME_NUMBER_SUFFIX}},

    // Test a European profile.
    {"Paris", {ADDRESS_HOME_CITY}, {ADDRESS_HOME_CITY}},
    {"Île de France",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Exact match
    {"Ile de France",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Missing accent.
    {"-Ile-de-France-",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Extra punctuation.
    {"île dÉ FrÃÑÇË",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Other accents & case mismatch.
    {"75008", {ADDRESS_HOME_ZIP}, {ADDRESS_HOME_ZIP}},
    {"FR", {ADDRESS_HOME_COUNTRY}, {ADDRESS_HOME_COUNTRY}},
    {"France", {ADDRESS_HOME_COUNTRY}, {ADDRESS_HOME_COUNTRY}},
    {"33249197070", {PHONE_HOME_WHOLE_NUMBER}, {PHONE_HOME_WHOLE_NUMBER}},
    {"+33 2 49 19 70 70", {PHONE_HOME_WHOLE_NUMBER}, {PHONE_HOME_WHOLE_NUMBER}},
    {"02 49 19 70 70",
     {PHONE_HOME_CITY_AND_NUMBER},
     {PHONE_HOME_CITY_AND_NUMBER}},
    {"0249197070", {PHONE_HOME_CITY_AND_NUMBER}, {PHONE_HOME_CITY_AND_NUMBER}},
    {"33", {PHONE_HOME_COUNTRY_CODE}, {PHONE_HOME_COUNTRY_CODE}},
    {"2", {PHONE_HOME_CITY_CODE}, {PHONE_HOME_CITY_CODE}},

    // Credit card fields matches.
    {"John Doe", {CREDIT_CARD_NAME_FULL}, {CREDIT_CARD_NAME_FULL}},
    {"John", {CREDIT_CARD_NAME_FIRST}, {CREDIT_CARD_NAME_FIRST}},
    {"Doe", {CREDIT_CARD_NAME_LAST}, {CREDIT_CARD_NAME_LAST}},
    {"4234-5678-9012-3456", {CREDIT_CARD_NUMBER}, {CREDIT_CARD_NUMBER}},
    {"04", {CREDIT_CARD_EXP_MONTH}, {CREDIT_CARD_EXP_MONTH}},
    {"April", {CREDIT_CARD_EXP_MONTH}, {CREDIT_CARD_EXP_MONTH}},
    {"2999", {CREDIT_CARD_EXP_4_DIGIT_YEAR}, {CREDIT_CARD_EXP_4_DIGIT_YEAR}},
    {"99", {CREDIT_CARD_EXP_2_DIGIT_YEAR}, {CREDIT_CARD_EXP_2_DIGIT_YEAR}},
    {"04/2999",
     {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},
     {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},

    // Make sure whitespace and invalid characters are handled properly.
    {"", {EMPTY_TYPE}, {EMPTY_TYPE}},
    {" ", {EMPTY_TYPE}, {EMPTY_TYPE}},
    {"***", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {" Elvis", {NAME_FIRST}, {NAME_FIRST}},
    {"Elvis ", {NAME_FIRST}, {NAME_FIRST}},

    // Make sure fields that differ by case match.
    {"elvis ", {NAME_FIRST}, {NAME_FIRST}},
    {"UnItEd StAtEs", {ADDRESS_HOME_COUNTRY}, {ADDRESS_HOME_COUNTRY}},

    // Make sure fields that differ by punctuation match.
    {"3734 Elvis Presley Blvd", {ADDRESS_HOME_LINE1}, {ADDRESS_HOME_LINE1}},
    {"3734, Elvis    Presley Blvd.",
     {ADDRESS_HOME_LINE1},
     {ADDRESS_HOME_LINE1}},

    // Make sure that a state's full name and abbreviation match.
    {"TN",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Saved as "Tennessee" in profile.
    {"Texas",
     {ADDRESS_HOME_STATE},
     {ADDRESS_HOME_STATE}},  // Saved as "TX" in profile.

    // Special phone number case. A profile with no country code should
    // only match PHONE_HOME_CITY_AND_NUMBER.
    {"5142821292", {PHONE_HOME_CITY_AND_NUMBER}, {PHONE_HOME_CITY_AND_NUMBER}},

    // Make sure unsupported variants do not match.
    {"Elvis Aaron", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"Mr. Presley", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"3734 Elvis Presley", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"38116-1023", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"5", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"56", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
    {"901", {UNKNOWN_TYPE}, {UNKNOWN_TYPE}},
};

// Tests that DeterminePossibleFieldTypesForUpload finds accurate possible
// types.
TEST_P(ProfileMatchingTypesTest, DeterminePossibleFieldTypesForUpload) {
  // Unpack the test parameters
  const auto& test_case = std::get<0>(GetParam());

  SCOPED_TRACE(base::StringPrintf(
      "Test: input_value='%s', field_type=%s, structured_names=%s ",
      test_case.input_value,
      AutofillType(*test_case.field_types.begin()).ToString().c_str(),
      StructuredNamesAndAddresses() ? "true" : "false"));

  // Take the field types depending on the state of the structured names
  // feature.
  const ServerFieldTypeSet& expected_possible_types =
      StructuredNamesAndAddresses() ? test_case.structured_field_types
                                    : test_case.field_types;

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

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("", "1", "", "text", &field);
  field.value = UTF8ToUTF16(test_case.input_value);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  base::HistogramTester histogram_tester;
  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  ASSERT_EQ(1U, form_structure.field_count());

  ServerFieldTypeSet possible_types = form_structure.field(0)->possible_types();
  EXPECT_EQ(possible_types, expected_possible_types);
}

// Tests that DeterminePossibleFieldTypesForUpload is called when a form is
// submitted.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DeterminePossibleFieldTypesForUpload_IsTriggered) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  std::vector<std::u16string> expected_values;

  // These fields should all match.
  FormFieldData field;
  ServerFieldTypeSet types;

  test::CreateTestFormField("", "1", "", "text", &field);
  expected_values.push_back(u"Elvis");
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "2", "", "text", &field);
  expected_values.push_back(u"Aaron");
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "3", "", "text", &field);
  expected_values.push_back(u"A");
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

  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  FormSubmitted(form);
}

// Tests that DisambiguateUploadTypes makes the correct choices.
TEST_P(BrowserAutofillManagerStructuredProfileTest, DisambiguateUploadTypes) {
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

  struct TestFieldData {
    std::u16string input_value;
    ServerFieldType predicted_type;
    bool expect_disambiguation;
    ServerFieldType expected_upload_type;
  };
  using TestCase = std::vector<TestFieldData>;

  std::vector<TestCase> test_cases;

  // Address disambiguation.
  // An ambiguous address line followed by a field predicted as a line 2 and
  // that is empty should be disambiguated as an ADDRESS_HOME_LINE1.
  test_cases.push_back({{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1, true,
                         ADDRESS_HOME_LINE1},
                        {u"", ADDRESS_HOME_LINE2, true, EMPTY_TYPE}});

  // An ambiguous address line followed by a field predicted as a line 2 but
  // filled with another know profile value should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS},
       {u"38116", ADDRESS_HOME_LINE2, true, ADDRESS_HOME_ZIP}});

  // An ambiguous address line followed by an empty field predicted as
  // something other than a line 2 should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS},
       {u"", ADDRESS_HOME_ZIP, true, EMPTY_TYPE}});

  // An ambiguous address line followed by no other field should be
  // disambiguated as an ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS}});

  // Name disambiguation.
  // An ambiguous name field that has no next field and that is preceded by
  // a non credit card field should be disambiguated as a non credit card
  // name.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST}});

  // An ambiguous name field that has no next field and that is preceded by
  // a credit card field should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER},
       {u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST}});

  // An ambiguous name field that has no previous field and that is
  // followed by a non credit card field should be disambiguated as a non
  // credit card name.
  test_cases.push_back(
      {{u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST},

       {u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY}});

  // An ambiguous name field that has no previous field and that is followed
  // by a credit card field should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST},
       {u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER}});

  // An ambiguous name field that is preceded and followed by non credit
  // card fields should be disambiguated as a non credit card name.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST},
       {u"Tennessee", ADDRESS_HOME_STATE, true, ADDRESS_HOME_STATE}});

  // An ambiguous name field that is preceded and followed by credit card
  // fields should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER},
       {u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST},
       {u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR}});

  // An ambiguous name field that is preceded by a non credit card field and
  // followed by a credit card field should not be disambiguated.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST},
       {u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR}});

  // An ambiguous name field that is preceded by a credit card field and
  // followed by a non credit card field should not be disambiguated.
  test_cases.push_back(
      {{u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR},
       {u"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST},
       {u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY}});

  for (const TestCase& test_fields : test_cases) {
    FormData form;
    form.name = u"MyForm";
    form.url = GURL("https://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");

    // Create the form fields specified in the test case.
    FormFieldData field;
    for (const TestFieldData& test_field : test_fields) {
      test::CreateTestFormField("", "1", "", "text", &field);
      field.value = test_field.input_value;
      form.fields.push_back(field);
    }

    // Assign the specified predicted type for each field in the test case.
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
          prediction;
      prediction.set_type(test_fields[i].predicted_type);
      form_structure.field(i)->set_server_predictions({prediction});
    }

    BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
        profiles, credit_cards, std::u16string(), "en-us", &form_structure);
    ASSERT_EQ(test_fields.size(), form_structure.field_count());

    // Make sure the disambiguation method selects the expected upload type.
    ServerFieldTypeSet possible_types;
    for (size_t i = 0; i < test_fields.size(); ++i) {
      possible_types = form_structure.field(i)->possible_types();
      if (test_fields[i].expect_disambiguation) {
        // For structured names it is possible that a field as two out of three
        // possible classifications: NAME_FULL, NAME_LAST,
        // NAME_LAST_FIRST/SECOND. Note, all cases contain NAME_LAST.
        if (StructuredNamesAndAddresses() && possible_types.size() == 2) {
          EXPECT_TRUE(possible_types.count(NAME_LAST) &&
                      (possible_types.count(NAME_LAST_SECOND) ||
                       possible_types.count(NAME_LAST_FIRST) ||
                       possible_types.count(NAME_FULL)));
        } else if (StructuredNamesAndAddresses() &&
                   possible_types.size() == 3) {
          // Or even all three.
          EXPECT_TRUE(possible_types.count(NAME_FULL) &&
                      possible_types.count(NAME_LAST) &&
                      (possible_types.count(NAME_LAST_SECOND) ||
                       possible_types.count(NAME_LAST_FIRST)));
        } else {
          EXPECT_EQ(1U, possible_types.size());
        }
        EXPECT_NE(possible_types.end(),
                  possible_types.find(test_fields[i].expected_upload_type));
      } else {
        // In the context of those tests, it is expected that the type is
        // ambiguous.
        EXPECT_NE(1U, possible_types.size());
      }
    }
  }
}

// When a field contains fields with UPI ID values, a crowdsourcing vote should
// be uploaded.
TEST_P(BrowserAutofillManagerStructuredProfileTest, CrowdsourceUPIVPA) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  FormData form;
  FormFieldData field;
  test::CreateTestFormField("", "name1", "1234@indianbank", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "name2", "not-upi@gmail.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  EXPECT_THAT(form_structure.field(0)->possible_types(), ElementsAre(UPI_VPA));
  EXPECT_THAT(form_structure.field(1)->possible_types(),
              Not(Contains(UPI_VPA)));
}

// If a server-side credit card is unmasked by entering the CVC, the
// BrowserAutofillManager reuses the CVC value to identify a potentially
// existing CVC form field to cast a |CREDIT_CARD_VERIFICATION_CODE|-type vote.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CrowdsourceCVCFieldByValue) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  const char kCvc[] = "1234";
  const char16_t kCvc16[] = u"1234";
  const char kFourDigitButNotCvc[] = "6676";
  const char kCreditCardNumber[] = "4234-5678-9012-3456";

  FormData form;
  FormFieldData field1;
  test::CreateTestFormField("number", "number", kCreditCardNumber, "text",
                            &field1);
  form.fields.push_back(field1);

  // This field would not be detected as CVC heuristically if the CVC value
  // wouldn't be known.
  FormFieldData field2;
  test::CreateTestFormField("not_cvc", "not_cvc", kFourDigitButNotCvc, "text",
                            &field2);
  form.fields.push_back(field2);

  // This field has the CVC value used to unlock the card and should be detected
  // as the CVC field.
  FormFieldData field3;
  test::CreateTestFormField("c_v_c", "c_v_c", kCvc, "text", &field3);
  form.fields.push_back(field3);

  FormStructure form_structure(form);
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, kCvc16, "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(
      form_structure, 2, CREDIT_CARD_VERIFICATION_CODE,
      FieldPropertiesFlags::kKnownValue);
}

// Expiration year field was detected by the server. The other field with a
// 4-digit value should be detected as CVC.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it appears after the
// expiration year field as it was predicted by the server.
// The value in the CVC field would be a valid expiration year value.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CrowdsourceCVCFieldAfterExpDateByHeuristics) {
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

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it contains a value which
// is not a valid expiration year.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CrowdsourceCVCFieldBeforeExpDateByHeuristics) {
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

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 1,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if no CVC field is heuristically detected due to the missing of a
// credit card number field.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CrowdsourceNoCVCFieldDueToMissingCreditCardNumber) {
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

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);
  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

// Test if no CVC is found because the candidate has no valid CVC value.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CrowdsourceNoCVCDueToInvalidCandidateValue) {
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

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest, RemoveProfile) {
  // Add and remove an Autofill profile.
  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000102";
  profile.set_guid(guid);
  personal_data().AddProfile(profile);

  int id = MakeFrontendId(std::string(), guid);

  browser_autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data().GetProfileWithGUID(guid));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest, RemoveCreditCard) {
  // Add and remove an Autofill credit card.
  CreditCard credit_card;
  const char guid[] = "00000000-0000-0000-0000-000000100007";
  credit_card.set_guid(guid);
  personal_data().AddCreditCard(credit_card);

  int id = MakeFrontendId(guid, std::string());

  browser_autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data().GetCreditCardWithGUID(guid));
}

// Test our external delegate is called at the right time.
TEST_P(BrowserAutofillManagerStructuredProfileTest, TestExternalDelegate) {
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnTextFieldDidChangeAndUnfocus_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  // For structured names, this type cannot be differentiated from
  // NAME_LAST_SECOND.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInNames))
    types.insert(NAME_LAST_SECOND);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);
}

// Test that navigating with a filled form sends an upload with types matching
// the fields.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnTextFieldDidChangeAndNavigation_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  if (StructuredNamesAndAddresses())
    types.insert(NAME_LAST_SECOND);
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
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate a navigation so that the pending form is uploaded.
  browser_autofill_manager_->Reset();
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       OnDidFillAutofillFormDataAndUnfocus_Upload) {
  // Set up our form data (empty).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  if (StructuredNamesAndAddresses())
    types.insert(NAME_LAST_SECOND);
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
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // Form was autofilled with user data.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  browser_autofill_manager_->OnDidFillAutofillFormData(
      form, AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);
}

// Test that suggestions are returned for credit card fields with an
// unrecognized
// autocomplete attribute.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_ForNumberSplitAcrossFields) {
  // Set up our form data with credit card number split across fields.
  FormData form;
  form.name = u"MyForm";
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
  number_field.value = u"901";

  // Get the suggestions for already filled credit card |number_field|.
  GetAutofillSuggestions(form, number_field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that inputs detected to be CVC inputs are forced to
// !should_autocomplete for SingleFieldFormFillRouter::OnWillSubmitForm.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DontSaveCvcInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  EXPECT_CALL(*single_field_form_fill_router_, OnWillSubmitForm(_, _, true))
      .WillOnce(SaveArg<0>(&form_seen_by_ahm));

  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  ASSERT_EQ(std::size(test_fields), form_seen_by_ahm.fields.size());
  for (size_t i = 0; i < std::size(test_fields); ++i) {
    EXPECT_EQ(
        form_seen_by_ahm.fields[i].should_autocomplete,
        test_fields[i].expected_field_type != CREDIT_CARD_VERIFICATION_CODE);
  }
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DontOfferToSavePaymentsCard) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  // Manually fill out |form| so we can use it in OnFormSubmitted.
  for (auto& field : form.fields) {
    if (field.name == u"cardnumber")
      field.value = u"4012888888881881";
    else if (field.name == u"nameoncard")
      field.value = u"John H Dillinger";
    else if (field.name == u"ccmonth")
      field.value = u"01";
    else if (field.name == u"ccyear")
      field.value = u"2017";
  }

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
  browser_autofill_manager_->OnFormSubmitted(form, false,
                                             SubmissionSource::FORM_SUBMISSION);
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FillInUpdatedExpirationDate) {
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ProfileDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillProfileEnabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _)).Times(0);

  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       MakeFrontendId(std::string(), guid));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ProfileDisabledDoesNotSuggest) {
  browser_autofill_manager_->SetAutofillProfileEnabled(false);

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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CreditCardDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _)).Times(0);

  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       MakeFrontendId(guid, std::string()));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       CreditCardDisabledDoesNotSuggest) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);

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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       NoSuggestionForNonPrefixTokenMatch) {
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
TEST_P(CreditCardSuggestionTest,
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
  credit_card.SetNickname(kArbitraryNickname16);
  personal_data().AddCreditCard(credit_card);

#if BUILDFLAG(IS_ANDROID)
  // When keyboard accessary is enabled, always show "7777".
  // When keyboard accessary is disabled, if nickname is valid, show "Nickname
  // ****7777", otherwise, show "Visa  ****7777".
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength())
          : kArbitraryNickname + "  " +
                test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength());

#elif BUILDFLAG(IS_IOS)
  const std::string visa_label =
      test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength());

#else
  const std::string visa_label = base::JoinString(
      {kArbitraryNickname + "  ",
       test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength()),
       ", expires on 01/30"},
      "");
#endif

  GetAutofillSuggestions(form, field);
  CheckSuggestions(kDefaultPageID,
                   Suggestion("Nancy Drew", visa_label, kVisaCard,
                              MakeFrontendId(guid, std::string())));
}

// Verify that typing "lvis" will not match any of the credit card name when
// substring matching is enabled.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       NoCreditCardSuggestionsForNonPrefixTokenMatch) {
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetPopupType_CreditCardForm) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kCreditCards,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

TEST_P(BrowserAutofillManagerStructuredProfileTest, GetPopupType_AddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kAddresses,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetPopupType_PersonalInformationForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kPersonalInformation,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit of three and no impressions yet.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
  EXPECT_TRUE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Expect to now have an impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // No additional impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit that has been attained already.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions stay the same.
  EXPECT_EQ(kCreditCardSigninPromoImpressionLimit,
            autofill_client_.GetPrefs()->GetInteger(
                prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form on a non-secure page.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form targeting a non-secure page.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for an address
// form.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ShouldShowCreditCardSigninPromo_AddressField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);

  // Call will now return false, because it is initiated from an address field.
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(0);
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));
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
  profile1.SetInfo(NAME_FIRST, u"Robin", "en-US");
  profile1.SetInfo(NAME_MIDDLE, u"Adam Smith", "en-US");
  profile1.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, u"Carl", "en-US");
  profile2.SetInfo(NAME_MIDDLE, u"Shawn Smith", "en-US");
  profile2.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile2);

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

TEST_P(BrowserAutofillManagerStructuredProfileTest, ShouldUploadForm) {
  // Note: The enforcement of a minimum number of required fields for upload
  // is disabled by default. This tests validates both the disabled and enabled
  // scenarios.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");

  // Empty Form.
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Add a field to the form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Add a second field to the form.
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has less than 3 fields but has autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields, no autocomplete attribute.
  form.fields[0].autocomplete_attribute = "";
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure_3(form);
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields and at least one autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Is off the record.
  autofill_driver_->SetIsIncognito(true);
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Make sure it's reset for the next test case.
  autofill_driver_->SetIsIncognito(false);
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has one field which is appears to be a password field.
  form.fields.clear();
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);

  // With min required fields disabled.
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Autofill disabled.
  browser_autofill_manager_->SetAutofillProfileEnabled(false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DisplaySuggestions_AutocompleteOffNotRespected_AddressField) {
  // Set up an address form.
  FormData mixed_form;
  mixed_form.name = u"MyForm";
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

// Verify that suggestions are shown on desktop for credit card related fields
// even if the initiating field has the "autocomplete" attribute set to off.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DisplaySuggestions_AutocompleteOff_CreditCardField) {
  // Set up a credit card form.
  FormData mixed_form;
  mixed_form.name = u"MyForm";
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
  for (const FormFieldData& mixed_form_field : mixed_form.fields) {
    // Single field form fill suggestions being returned are directly correlated
    // to whether or not the field has autocomplete set to true or false. We
    // know autocomplete must be the single field form filler in this case due
    // to the field not having a type that would route to any of the other
    // single field form fillers.
    ON_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
        .WillByDefault(testing::Return(mixed_form_field.should_autocomplete));
    GetAutofillSuggestions(mixed_form, mixed_form_field);

    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with server only types is still autofillable if the form
// gets updated in cache.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DisplaySuggestionsForUpdatedServerTypedForm) {
  // Create a form with unknown heuristic fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Field 1", "field1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 2", "field2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 3", "field3", "", "text", &field);
  form.fields.push_back(field);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
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
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Make sure the form can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    ASSERT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify one of the fields in the original form.
  form.fields[0].css_classes += u"a";

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify form action URL. This can happen on in-page navitaion if the form
  // doesn't have an actual action (attribute is empty).
  form.action = net::AppendQueryParameter(form.action, "arg", "value");

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with <select> field is accepted if <option> value (not
// content) is quite long. Some websites use value to propagate long JSON to
// JS-backed logic.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       FormWithLongOptionValuesIsAcceptable) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

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
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Test that is_all_server_suggestions is true if there are only
// full_server_card and masked_server_card on file.
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       GetCreditCardSuggestions_VirtualCard) {
  personal_data().ClearCreditCards();
  CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                /*server_id=*/"a123");
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
  masked_server_card.set_virtual_card_enrollment_state(CreditCard::ENROLLED);
  masked_server_card.SetNickname(u"nickname");
  personal_data().AddServerCreditCard(masked_server_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Card number field.
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  std::string label = std::string("04/99");
#else
  std::string label = std::string("Expires on 04/99");
#endif

  Suggestion virtual_card_suggestion = Suggestion(
      "Virtual card",
      std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
      label, kVisaCard, autofill::POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);

  CheckSuggestions(
      kDefaultPageID, virtual_card_suggestion,
      Suggestion(
          std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)));

  // Non card number field (cardholder name field).
  field = form.fields[0];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
  label = std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
  label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
  label = std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456") +
          std::string(", expires on 04/99");
#endif

  virtual_card_suggestion =
      Suggestion("Virtual card", std::string("Elvis Presley"), label, kVisaCard,
                 autofill::POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);

  CheckSuggestions(
      kDefaultPageID, virtual_card_suggestion,
      Suggestion("Elvis Presley", label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));

  // Incomplete form.
  field = form.fields[0];
  form.fields.pop_back();
  GetAutofillSuggestions(form, field);

  CheckSuggestions(
      kDefaultPageID, virtual_card_suggestion,
      Suggestion("Elvis Presley", label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogAutocompleteShownMetric) {
  FormData form;
  form.name = u"NothingSpecial";

  FormFieldData field;
  test::CreateTestFormField("Something", "something", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/false, form, field);
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogAutofillAddressShownMetric) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressOnly) {
  // Create a form with name and address fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressOnlyWithoutName) {
  // Create a form with address fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_ContactOnly) {
  // Create a form with name and contact fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_ContactOnlyWithoutName) {
  // Create a form with contact fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_PhoneOnly) {
  // Create a form with phone field.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address.PhoneOnly",
                                     FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_Other) {
  // Create a form with name fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusEmail) {
  // Create a form with name, address, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusEmailWithoutName) {
  // Create a form with address and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusPhone) {
  // Create a form with name fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusPhoneWithoutName) {
  // Create a form with name, address, and phone fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhone) {
  // Create a form with name, address, phone, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhoneWithoutName) {
  // Create a form with address, phone, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
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
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidShowSuggestions_LogAutofillCreditCardShownMetric) {
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
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

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidSuppressPopup_LogAutofillAddressPopupSuppressed) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  browser_autofill_manager_->OnFormsSeen({form}, {});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidSuppressPopup(form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.FormEvents.Address",
                                     FORM_EVENT_POPUP_SUPPRESSED, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                        HasSubstr("Autocomplete.Events"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_P(BrowserAutofillManagerStructuredProfileTest,
       DidSuppressPopup_LogAutofillCreditCardPopupSuppressed) {
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidSuppressPopup(form, form.fields[0]);
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
TEST_P(BrowserAutofillManagerStructuredProfileTest,
       ImportDataWhenValueDetected) {
  const std::string test_upi_id_value = "user@indianbank";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_CALL(autofill_client_, ConfirmSaveUpiIdLocally(test_upi_id_value, _))
      .WillOnce([](std::string upi_id,
                   base::OnceCallback<void(bool user_decision)> callback) {
        std::move(callback).Run(true);
      });
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  browser_autofill_manager_->SetExpectedObservedSubmission(true);
  browser_autofill_manager_->SetCallParentUploadFormData(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = base::UTF8ToUTF16(test_upi_id_value);
  FormSubmitted(form);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // The feature is not implemented for mobile.
  EXPECT_EQ(0, personal_data().num_times_save_upi_id_called());
#else
  EXPECT_EQ(1, personal_data().num_times_save_upi_id_called());
#endif
}

// Test that we do not import UPI data when in incognito.
TEST_F(BrowserAutofillManagerTest, DontImportUpiIdWhenIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);
  autofill_driver_->SetIsIncognito(true);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_CALL(autofill_client_, ConfirmSaveUpiIdLocally(_, _)).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  browser_autofill_manager_->SetExpectedObservedSubmission(true);
  browser_autofill_manager_->SetCallParentUploadFormData(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = u"user@indianbank";
  FormSubmitted(form);

  EXPECT_EQ(0, personal_data().num_times_save_upi_id_called());
}

TEST_F(BrowserAutofillManagerTest, PageLanguageGetsCorrectlySet) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  FormStructure* parsed_form =
      browser_autofill_manager_->FindCachedFormByRendererId(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  autofill_client_.GetLanguageState()->SetCurrentLanguage("zh");

  browser_autofill_manager_->OnFormsSeen({form}, {});
  parsed_form =
      browser_autofill_manager_->FindCachedFormByRendererId(form.global_id());

  ASSERT_EQ(LanguageCode("zh"), parsed_form->current_page_language());
}

// Test language detection on frames depending on whether the frame is active or
// not.
class BrowserAutofillManagerTestPageLanguageDetection
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserAutofillManagerTestPageLanguageDetection() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillPageLanguageDetection);
  }

  bool is_in_active_frame() const { return GetParam(); };

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_P(BrowserAutofillManagerTestPageLanguageDetection, GetsCorrectlyDetected) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  FormStructure* parsed_form =
      browser_autofill_manager_->FindCachedFormByRendererId(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  translate::LanguageDetectionDetails language_detection_details;
  language_detection_details.adopted_language = "hu";
  autofill_driver_->SetIsInActiveFrame(is_in_active_frame());
  browser_autofill_manager_->OnLanguageDetermined(language_detection_details);

  autofill_client_.GetLanguageState()->SetCurrentLanguage("hu");

  parsed_form =
      browser_autofill_manager_->FindCachedFormByRendererId(form.global_id());

  // Language detection is used only for active frames.
  auto expected_language_code =
      is_in_active_frame() ? LanguageCode("hu") : LanguageCode();

  ASSERT_EQ(*expected_language_code, *parsed_form->current_page_language());
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserAutofillManagerTestPageLanguageDetection,
                         testing::Bool());

// BrowserAutofillManagerTest with different browser profile types.
class BrowserAutofillManagerProfileMetricsTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<profile_metrics::BrowserProfileType> {
 public:
  BrowserAutofillManagerProfileMetricsTest() : profile_type_(GetParam()) {
    EXPECT_CALL(autofill_client_, GetProfileType())
        .WillRepeatedly(Return(profile_type_));
  }

  const profile_metrics::BrowserProfileType profile_type_;
};

// Tests if submitting a form in different browser profile types records correct
// |Autofill.FormSubmission.PerProfileType| metric.
TEST_P(BrowserAutofillManagerProfileMetricsTest,
       FormSubmissionPerProfileTypeMetrics) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});
  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  base::HistogramTester histogram_tester;

  FormSubmitted(form);
  histogram_tester.ExpectBucketCount("Autofill.FormSubmission.PerProfileType",
                                     profile_type_, 1);
  histogram_tester.ExpectTotalCount("Autofill.FormSubmission.PerProfileType",
                                    1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerProfileMetricsTest,
    testing::ValuesIn({profile_metrics::BrowserProfileType::kRegular,
                       profile_metrics::BrowserProfileType::kIncognito,
                       profile_metrics::BrowserProfileType::kGuest}));

// Tests that autocomplete-related metrics are emitted correctly on form
// submission.
TEST_F(BrowserAutofillManagerTest, AutocompleteMetrics) {
  // `kAutocompleteValues` corresponds to empty, valid, garbage and off.
  constexpr base::StringPiece kAutocompleteValues[]{"", "name", "asdf", "off"};
  // The 4 possible combinations of heuristic and server type status:
  // - Neither a fillable heuristic type nor a fillable server type.
  // - Only a fillable server type.
  // - Only a fillable heuristic type.
  // - Both a fillable heuristic type and a fillable server type.
  // NO_SERVER_DATA and UNKNOWN_TYPE are both unfillable types, but
  // NO_SERVER_DATA is ignored in the PredictionCollisionType metric.
  constexpr ServerFieldType kTypeClasses[][2]{
      {UNKNOWN_TYPE, NO_SERVER_DATA},
      {UNKNOWN_TYPE, EMAIL_ADDRESS},
      {ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE},
      {ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS}};

  // Create a form with one field per kAutofillValue x kTypeClass combination.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (const auto& autocomplete : kAutocompleteValues) {
    for (const auto& types : kTypeClasses) {
      FormFieldData field;
      test::CreateTestFormField("", "", "", "text", &field);
      field.autocomplete_attribute = std::string(autocomplete);
      form.fields.push_back(field);
      heuristic_types.push_back(types[0]);
      server_types.push_back(types[1]);
    }
  }
  // Override the types and simulate seeing the form on page load.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Submit the form and verify that all metrics are collected correctly.
  base::HistogramTester histogram_tester;
  FormSubmitted(form);

  // Expect one entry for each possible PredictionStateAutocompleteStatePair.
  // Fields without type predictions and autocomplete attributes are ignored.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.PredictionCollisionState", form.fields.size() - 1);
  for (int i = 0; i < 15; i++) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.PredictionCollisionState", i, 1);
  }

  // The PredictionCollisionType metric is collected for every field with
  // autocomplete=garbage. This amounts 1 vote per `kTypeClass` here.
  // Fields NO_SERVER_DATA are ignored for the .Server metric.
  const std::string kTypeHistogram =
      "Autofill.Autocomplete.PredictionCollisionType.";
  histogram_tester.ExpectTotalCount(kTypeHistogram + "Heuristics", 4);
  histogram_tester.ExpectTotalCount(kTypeHistogram + "Server", 3);
  histogram_tester.ExpectTotalCount(kTypeHistogram + "ServerOrHeuristics", 4);
}

// Test that if a form is mixed content we show a warning instead of any
// suggestions.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedForm) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);

  GetAutofillSuggestions(form, form.fields[0]);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", POPUP_ITEM_ID_MIXED_FORM_MESSAGE));
}

// Test that if a form is mixed content we do not show a warning if the opt out
// polcy is set.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormOptoutPolicy) {
  // Set pref to disabled.
  autofill_client_.GetPrefs()->SetBoolean(::prefs::kMixedFormsWarningsEnabled,
                                          false);

  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we dismiss the mixed form warning if user starts typing.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormUserTyped) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);

  GetAutofillSuggestions(form, form.fields[0]);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", POPUP_ITEM_ID_MIXED_FORM_MESSAGE));

  // Pretend user started typing and make sure we no longer set suggestions.
  form.fields[0].value = u"Michael";
  form.fields[0].properties_mask |= kUserTyped;
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

// Test that we don't treat javascript scheme target URLs as mixed forms.
// Regression test for crbug.com/1135173
TEST_F(BrowserAutofillManagerTest, GetSuggestions_JavascriptUrlTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("javascript:alert('hello');");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we don't treat about:blank target URLs as mixed forms.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_AboutBlankTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the Autofill does not override input field values that were already
// prefilled.
TEST_F(BrowserAutofillManagerTest, PreventOverridingOfPrefilledValues) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "Test City", "text", &field);
  form.fields.push_back(field);
  test::CreateTestSelectField("State", "state", "California",
                              {"Washington", "Tennessee", "California"},
                              {"DC", "TN", "CA"}, 3, &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "Test Country", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "12345678901", "tel",
                            &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Test City");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"Test Country");
  EXPECT_EQ(response_data.fields[4].value, u"12345678901");

  {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    std::vector<std::string> expected_values = {"", "Memphis", "",
                                                "United States", ""};
    bool found = browser_autofill_manager_->GetCachedFormAndField(
        form, form.fields[0], &form_structure, &autofill_field);
    ASSERT_TRUE(found);
    for (size_t i = 0; i < form.fields.size(); ++i) {
      ASSERT_TRUE(form_structure->field(i)->SameFieldAs(form.fields[i]));
      if (!expected_values[i].empty()) {
        EXPECT_TRUE(form_structure->field(i)
                        ->value_not_autofilled_over_existing_value_hash()
                        .has_value());
        EXPECT_FALSE(form_structure->field(i)->is_autofilled);
        EXPECT_EQ(form_structure->field(i)
                      ->value_not_autofilled_over_existing_value_hash(),
                  base::FastHash(expected_values[i]));
      }
    }

    EXPECT_TRUE(form_structure->field(0)->is_autofilled);  // No prefilled value
    EXPECT_TRUE(form_structure->field(2)->is_autofilled);  // Selection field.

    // Prefilled value is same as the value to be autofilled so
    // |value_not_autofilled_over_existing_value_hash| is not set for the field.
    EXPECT_FALSE(form_structure->field(4)->is_autofilled);
    EXPECT_FALSE(form_structure->field(4)
                     ->value_not_autofilled_over_existing_value_hash());
  }

  features.Reset();
  features.InitAndDisableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);

  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Memphis");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"United States");
  EXPECT_EQ(response_data.fields[4].value, u"12345678901");
}

// Tests that the Autofill does override the prefilled field value since the
// field is the initiating field for the Autofill and has a prefilled value
// which is a substring of the autofillable value.
TEST_F(BrowserAutofillManagerTest, AutofillOverridePrefilledValue) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "Test Name", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "Test City", "text", &field);
  form.fields.push_back(field);
  test::CreateTestSelectField("State", "state", "California",
                              {"Washington", "Tennessee", "California"},
                              {"DC", "TN", "CA"}, 3, &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "Test Country", "text",
                            &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // "Elv" is a substring of "Elvis Aaron Presley".
  form.fields[0].value = u"Elv";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendId(std::string(), guid),
                                     &response_page_id, &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Test City");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"Test Country");
}

// Tests that both Autofill popup and TTF are hidden on renderer event.
TEST_F(BrowserAutofillManagerTest, HideAutofillPopupAndTouchToFillOnHidePopup) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kRendererEvent));
  EXPECT_CALL(*touch_to_fill_delegate_, HideTouchToFill);
  browser_autofill_manager_->OnHidePopup();
}

// Tests that only Autofill popup is hidden on editing end, but not TTF.
TEST_F(BrowserAutofillManagerTest, OnDidEndTextFieldEditing) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kEndEditing));
  EXPECT_CALL(*touch_to_fill_delegate_, HideTouchToFill).Times(0);
  browser_autofill_manager_->OnDidEndTextFieldEditing();
}

// Tests that Autofill suggestions are not shown if TTF is eligible and shown.
TEST_F(BrowserAutofillManagerTest, AutofillSuggestionsOrTouchToFill) {
  FormData form;
  CreateTestCreditCardFormData(&form, /*is_https=*/true,
                               /*use_month_type=*/false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[1];
  int query_id = 1;

  // TTF not eligible, Autofill suggestions shown.
  EXPECT_CALL(*touch_to_fill_delegate_, TryToShowTouchToFill(query_id, _, _))
      .Times(0);
  TryToShowTouchToFill(query_id++, form, field, TouchToFillEligible(false));
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // TTF not shown, Autofill suggestions shown.
  EXPECT_CALL(*touch_to_fill_delegate_, TryToShowTouchToFill(query_id, _, _))
      .WillOnce(Return(false));
  TryToShowTouchToFill(query_id++, form, field, TouchToFillEligible(true));
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // TTF eligible and shown, Autofill suggestions not shown
  EXPECT_CALL(*touch_to_fill_delegate_, TryToShowTouchToFill(query_id, _, _))
      .WillOnce(Return(true));
  TryToShowTouchToFill(query_id++, form, field, TouchToFillEligible(true));
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Tests that neither Autofill suggestions nor TTF is triggered if TTF is
// already shown.
TEST_F(BrowserAutofillManagerTest, ShowNothingIfTouchToFillAlreadyShown) {
  FormData form;
  CreateTestCreditCardFormData(&form, /*is_https=*/true,
                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields[1];

  EXPECT_CALL(*touch_to_fill_delegate_, IsShowingTouchToFill)
      .WillOnce(Return(true));
  EXPECT_CALL(*touch_to_fill_delegate_,
              TryToShowTouchToFill(kDefaultPageID, _, _))
      .Times(0);
  TryToShowTouchToFill(kDefaultPageID, form, field, TouchToFillEligible(true));
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Desktop only tests.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class BrowserAutofillManagerTestForVirtualCardOption
    : public BrowserAutofillManagerTest {
 protected:
  BrowserAutofillManagerTestForVirtualCardOption() = default;
  ~BrowserAutofillManagerTestForVirtualCardOption() override = default;

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // The URL should always matche the form URL in
    // CreateTestCreditCardFormData() to have the allowlist work correctly.
    autofill_client_.set_allowed_merchants({"https://myform.com/form.html"});

    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableVirtualCard);

    // Add only one server card so the second suggestion (if any) must be the
    // "Use a virtual card number" option.
    personal_data().ClearCreditCards();
    CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                  /*server_id=*/"a123");
    // TODO(crbug.com/1020740): Replace all the hard-coded expiration year in
    // this file with NextYear().
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.SetNetworkForMaskedCard(kVisaCard);
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    personal_data().AddServerCreditCard(masked_server_card);
  }

  void CreateCompleteFormAndGetSuggestions() {
    FormData form;
    CreateTestCreditCardFormData(&form, /*is_https=*/true,
                                 /*use_month_type=*/false);
    std::vector<FormData> forms(1, form);
    FormsSeen(forms);
    const FormFieldData& field = form.fields[1];  // card number field.
    GetAutofillSuggestions(form, field);
  }

  // Adds a CreditCardCloudTokenData to PersonalDataManager. This needs to be
  // called before suggestions are fetched.
  void CreateCloudTokenDataForDefaultCard() {
    personal_data().ClearCloudTokenData();
    CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
    data1.masked_card_id = "a123";
    personal_data().AddCloudTokenData(data1);
  }

  void VerifyNoVirtualCardSuggestions() {
    external_delegate_->CheckSuggestionCount(kDefaultPageID, 1);
    // Suggestion details need to match the credit card added in the SetUp()
    // above.
    CheckSuggestions(
        kDefaultPageID,
        Suggestion(
            std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
            "Expires on 04/99", kVisaCard,
            browser_autofill_manager_->GetPackedCreditCardID(7)));
  }
};

// Ensures the "Use a virtual card number" option should not be shown when
// experiment is disabled.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableVirtualCard);
  CreateCompleteFormAndGetSuggestions();

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should not be shown when the
// preference for credit card upload is set to disabled.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToCreditCardUploadPrefDisabled) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(false);
  CreateCompleteFormAndGetSuggestions();

  external_delegate_->CheckSuggestionCount(kDefaultPageID, 0);
}

// Ensures the "Use a virtual card number" option should not be shown when
// merchant is not allowlisted.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToMerchantNotAllowlisted) {
  // Adds a different URL in the allowlist.
  autofill_client_.set_allowed_merchants(
      {"https://myform.anotherallowlist.com/form.html"});
  CreateCompleteFormAndGetSuggestions();

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should not be shown when card
// number field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingCardNumberField) {
  // Creates an incomplete form without card number field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  field = form.fields[0];  // cardholder name field.
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckSuggestionCount(kDefaultPageID, 1);
  const std::string visa_label =
      base::JoinString({"Visa  ", test::ObfuscatedCardDigitsAsUTF8("3456"),
                        ", expires on 04/99"},
                       "");
  CheckSuggestions(
      kDefaultPageID,
      Suggestion("Elvis Presley", visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Ensures the "Use a virtual card number" option should not be shown when there
// is no cloud token data for the card.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToNoCloudTokenData) {
  CreateCompleteFormAndGetSuggestions();

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should not be shown when there
// is multiple cloud token data for the card.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToMultipleCloudTokenData) {
  CreateCloudTokenDataForDefaultCard();
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  data2.masked_card_id = "a123";
  personal_data().AddCloudTokenData(data2);
  CreateCompleteFormAndGetSuggestions();

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should not be shown when card
// expiration date field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingExpirationDateField) {
  // Creates an incomplete form without expiration date field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  field = form.fields[1];  // card number field.
  GetAutofillSuggestions(form, field);

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should not be shown when card
// cvc field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingCvcField) {
  // Creates an incomplete form without cvc field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  field = form.fields[1];  // card number field.
  GetAutofillSuggestions(form, field);

  VerifyNoVirtualCardSuggestions();
}

// Ensures the "Use a virtual card number" option should be shown when all
// requirements are met.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldShowVirtualCardOption_OneCard) {
  CreateCloudTokenDataForDefaultCard();
  CreateCompleteFormAndGetSuggestions();

  // Ensures the card suggestion and the virtual card suggestion are shown.
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 2);
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)),
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CLOUD_TOKEN_DROPDOWN_OPTION_LABEL),
                 "", "", PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD));
}

// Ensures the "Use a virtual card number" option should be shown when there are
// multiple cards and at least one card meets requirements.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldShowVirtualCardOption_MultipleCards) {
  CreateCloudTokenDataForDefaultCard();

  // Adds another card which does not meet the requirements (has two cloud
  // tokens).
  CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                /*server_id=*/"a456");
  // TODO(crbug.com/1020740): Replace all the hard-coded expiration year in
  // this file with NextYear().
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4111111111111111",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000008");
  personal_data().AddServerCreditCard(masked_server_card);
  CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
  data1.masked_card_id = "a456";
  personal_data().AddCloudTokenData(data1);
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  data2.masked_card_id = "a456";
  personal_data().AddCloudTokenData(data2);

  CreateCompleteFormAndGetSuggestions();

  // Ensures the card suggestion and the virtual card suggestion are shown.
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 3);
  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("1111"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(8)),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)),
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CLOUD_TOKEN_DROPDOWN_OPTION_LABEL),
                 "", "", PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD));
}
#endif

// Test param indicates if there is an active screen reader.
class OnFocusOnFormFieldTest : public BrowserAutofillManagerTest,
                               public testing::WithParamInterface<bool> {
 protected:
  OnFocusOnFormFieldTest() = default;
  ~OnFocusOnFormFieldTest() override = default;

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    has_active_screen_reader_ = GetParam();
    external_delegate_->set_has_active_screen_reader(has_active_screen_reader_);
  }

  void TearDown() override {
    external_delegate_->set_has_active_screen_reader(false);
    BrowserAutofillManagerTest::TearDown();
  }

  void CheckSuggestionsAvailableIfScreenReaderRunning() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The only existing functions for determining whether ChromeVox is in use
    // are in the src/chrome directory, which cannot be included in components.
    // Thus, if the platform is ChromeOS, we assume that ChromeVox is in use at
    // this point in the code.
    EXPECT_EQ(true,
              external_delegate_->has_suggestions_available_on_field_focus());
#else
    EXPECT_EQ(has_active_screen_reader_,
              external_delegate_->has_suggestions_available_on_field_focus());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  form.name = u"MyForm";
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
  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[0],
                                                    gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();

  // No suggestions should be provided for the second field because of its
  // unrecognized autocomplete attribute.
  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_AutocompleteOffNotRespected) {
  FormData form;
  form.name = u"MyForm";
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

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_Ablation) {
  DisableAutofillViaAblation(scoped_feature_list_, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
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

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
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

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  // In a non-HTTPS context, there will be a warning indicating the page is
  // insecure.
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_Ablation) {
  DisableAutofillViaAblation(scoped_feature_list_, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

INSTANTIATE_TEST_SUITE_P(
    BrowserAutofillManagerTest,
    ProfileMatchingTypesTest,
    testing::Combine(testing::ValuesIn(kProfileMatchingTypesTestCases),
                     testing::Bool()));

INSTANTIATE_TEST_SUITE_P(All, OnFocusOnFormFieldTest, testing::Bool());

// Runs the suite with the feature |kAutofillSupportForMoreStructuredNames|
// enabled and disabled.
INSTANTIATE_TEST_SUITE_P(,
                         BrowserAutofillManagerStructuredProfileTest,
                         testing::Bool());

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "show-all"),
                                         std::make_tuple(1, "show-one")));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "")));
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

// The parameter indicates whether the AutofillKeyboardAccessory feature is
// enabled or disabled.
INSTANTIATE_TEST_SUITE_P(All, CreditCardSuggestionTest, testing::Bool());

struct ShareNicknameTestParam {
  std::string local_nickname;
  std::string server_nickname;
  std::string expected_nickname;
};

const ShareNicknameTestParam kShareNicknameTestParam[] = {
    {"", "", ""},
    {"", "server nickname", "server nickname"},
    {"local nickname", "", "local nickname"},
    {"local nickname", "server nickname", "local nickname"},
};

class BrowserAutofillManagerTestForSharingNickname
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<ShareNicknameTestParam> {
 public:
  BrowserAutofillManagerTestForSharingNickname()
      : local_nickname_(GetParam().local_nickname),
        server_nickname_(GetParam().server_nickname),
        expected_nickname_(GetParam().expected_nickname) {}

  CreditCard GetLocalCard() {
    CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    local_card.set_use_count(3);
    local_card.set_use_date(AutofillClock::Now() - base::Days(1));
    local_card.SetNickname(base::UTF8ToUTF16(local_nickname_));
    local_card.set_guid("00000000-0000-0000-0000-000000000001");
    return local_card;
  }

  CreditCard GetServerCard() {
    CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
    test::SetCreditCardInfo(&full_server_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    full_server_card.SetNickname(base::UTF8ToUTF16(server_nickname_));
    full_server_card.set_guid("00000000-0000-0000-0000-000000000002");
    return full_server_card;
  }

  std::string local_nickname_;
  std::string server_nickname_;
  std::string expected_nickname_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserAutofillManagerTestForSharingNickname,
                         testing::ValuesIn(kShareNicknameTestParam));

TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_DuplicateCards) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().AddCreditCard(local_card);
  personal_data().AddServerCreditCard(GetServerCard());
  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Query by card number field.
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string exp_label = std::string("04/99");
#else
  const std::string exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion((expected_nickname_.empty() ? std::string("Amex")
                                             : expected_nickname_) +
                     "  " + test::ObfuscatedCardDigitsAsUTF8("0005"),
                 exp_label, kAmericanExpressCard,
                 browser_autofill_manager_->GetPackedCreditCardID(2)));
}

TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_UnrelatedCards) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  CreditCard server_card = GetServerCard();
  // Make sure the cards are different by giving a different card number.
  server_card.SetNumber(u"371449635398431");
  personal_data().AddServerCreditCard(server_card);

  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Query by card number field.
  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string exp_label = std::string("04/99");
#else
  const std::string exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      kDefaultPageID,
      Suggestion(
          (local_nickname_.empty() ? std::string("Amex") : local_nickname_) +
              "  " + test::ObfuscatedCardDigitsAsUTF8("0005"),
          exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(1)),
      Suggestion(
          (server_nickname_.empty() ? std::string("Amex") : server_nickname_) +
              "  " + test::ObfuscatedCardDigitsAsUTF8("8431"),
          exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(2)));
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

class BrowserAutofillManagerRefillTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<RefillTestCase> {};

TEST_P(BrowserAutofillManagerRefillTest,
       RefillModifiedCreditCardExpirationDates) {
  RefillTestCase test_case = GetParam();
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillRefillModifiedCreditCardExpirationDates);

  // Set up a CC form with name, cc number and expiration date.
  FormData form;
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration date", "exp_date", "", "text", &field);
  form.fields.push_back(field);

  // Notify BrowserAutofillManager of the form.
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Simulate filling and store the data to be filled in |first_fill_data|.
  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData first_fill_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendId(guid, std::string()),
                                     &response_page_id, &first_fill_data);
  ASSERT_EQ(3u, first_fill_data.fields.size());
  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                    first_fill_data.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    first_fill_data.fields[1]);
  ExpectFilledField("Expiration date", "exp_date", "04/2999", "text",
                    first_fill_data.fields[2]);

  FormData refilled_form;
  if (test_case.triggers_refill) {
    // Prepare intercepting the filling operation to the driver and capture
    // the re-filled form data.
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(testing::SaveArg<2>(&refilled_form),
                        testing::Return(std::vector<FieldGlobalId>{})));
  } else {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _, _)).Times(0);
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
    ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                      refilled_form.fields[0]);
    EXPECT_FALSE(refilled_form.fields[0].force_override);
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                      refilled_form.fields[1]);
    EXPECT_FALSE(refilled_form.fields[1].force_override);
    ExpectFilledField("Expiration date", "exp_date",
                      test_case.refilled_exp_date, "text",
                      refilled_form.fields[2]);
    EXPECT_TRUE(refilled_form.fields[2].force_override);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerRefillTest,
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
