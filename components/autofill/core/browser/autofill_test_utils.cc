// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"

#include <cstdint>
#include <iterator>
#include <string>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_test_api.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/security_interstitials/core/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using base::ASCIIToUTF16;
using FieldPrediction = ::autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;

namespace autofill {

bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b) {
  auto members = [](const FormFieldDataPredictions& p) {
    return std::tie(p.host_form_signature, p.signature, p.heuristic_type,
                    p.server_type, p.overall_type, p.parseable_name, p.section,
                    p.rank, p.rank_in_signature_group, p.rank_in_host_form,
                    p.rank_in_host_form_signature_group);
  };
  return members(a) == members(b);
}

bool operator==(const FormDataPredictions& a, const FormDataPredictions& b) {
  return test::WithoutUnserializedData(a.data).SameFormAs(
             test::WithoutUnserializedData(b.data)) &&
         a.signature == b.signature && a.fields == b.fields;
}

namespace test {

namespace {

std::string GetRandomCardNumber() {
  const size_t length = 16;
  std::string value;
  value.reserve(length);
  for (size_t i = 0; i < length; ++i)
    value.push_back(static_cast<char>(base::RandInt('0', '9')));
  return value;
}

}  // namespace

AutofillTestEnvironment* AutofillTestEnvironment::current_instance_ = nullptr;

AutofillTestEnvironment& AutofillTestEnvironment::GetCurrent(
    const base::Location& location) {
  CHECK(current_instance_)
      << location.ToString() << " "
      << "tried to access the current AutofillTestEnvironment, but none "
         "exists. Add an autofill::test::Autofill(Browser|Unit)TestEnvironment "
         "member to test your test fixture.";
  return *current_instance_;
}

AutofillTestEnvironment::AutofillTestEnvironment(const Options& options) {
  CHECK(!current_instance_) << "An autofill::test::AutofillTestEnvironment has "
                               "already been registered.";
  current_instance_ = this;
  if (options.disable_server_communication) {
    scoped_feature_list_.InitAndDisableFeature(
        features::test::kAutofillServerCommunication);
  }
}

AutofillTestEnvironment::~AutofillTestEnvironment() {
  CHECK_EQ(current_instance_, this);
  current_instance_ = nullptr;
}

LocalFrameToken AutofillTestEnvironment::NextLocalFrameToken() {
  return LocalFrameToken(base::UnguessableToken::CreateForTesting(
      ++local_frame_token_counter_high_, ++local_frame_token_counter_low_));
}

FormRendererId AutofillTestEnvironment::NextFormRendererId() {
  return FormRendererId(++form_renderer_id_counter_);
}

FieldRendererId AutofillTestEnvironment::NextFieldRendererId() {
  return FieldRendererId(++field_renderer_id_counter_);
}

AutofillBrowserTestEnvironment::AutofillBrowserTestEnvironment(
    const Options& options)
    : AutofillTestEnvironment(options) {}

LocalFrameToken MakeLocalFrameToken(RandomizeFrame randomize) {
  if (*randomize) {
    return LocalFrameToken(
        AutofillTestEnvironment::GetCurrent().NextLocalFrameToken());
  } else {
    return LocalFrameToken(
        base::UnguessableToken::CreateForTesting(98765, 43210));
  }
}

FormData WithoutValues(FormData form) {
  for (FormFieldData& field : form.fields)
    field.value.clear();
  return form;
}

FormData AsAutofilled(FormData form, bool is_autofilled) {
  for (FormFieldData& field : form.fields)
    field.is_autofilled = is_autofilled;
  return form;
}

void SetFormGroupValues(FormGroup& form_group,
                        const std::vector<FormGroupValue>& values) {
  for (const auto& value : values) {
    form_group.SetRawInfoWithVerificationStatus(
        value.type, base::UTF8ToUTF16(value.value), value.verification_status);
  }
}

void VerifyFormGroupValues(const FormGroup& form_group,
                           const std::vector<FormGroupValue>& values,
                           bool ignore_status) {
  for (const auto& value : values) {
    SCOPED_TRACE(testing::Message()
                 << "Expected for type "
                 << AutofillType::ServerFieldTypeToString(value.type) << "\n\t"
                 << value.value << " with status "
                 << (ignore_status ? "(ignored)" : "")
                 << value.verification_status << "\nFound:"
                 << "\n\t" << form_group.GetRawInfo(value.type)
                 << " with status "
                 << form_group.GetVerificationStatus(value.type));

    EXPECT_EQ(form_group.GetRawInfo(value.type),
              base::UTF8ToUTF16(value.value));
    if (!ignore_status) {
      EXPECT_EQ(form_group.GetVerificationStatus(value.type),
                value.verification_status);
    }
  }
}

std::unique_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  registry->RegisterBooleanPref(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  registry->RegisterBooleanPref(::prefs::kMixedFormsWarningsEnabled, true);
  registry->RegisterStringPref(prefs::kAutofillStatesDataDir, "");
  return PrefServiceForTesting(registry.get());
}

std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry) {
  prefs::RegisterProfilePrefs(registry);

  PrefServiceFactory factory;
  factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
  return factory.Create(registry);
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field) {
  field->host_frame = MakeLocalFrameToken();
  field->unique_renderer_id = MakeFieldRendererId();
  field->label = ASCIIToUTF16(label);
  field->name = ASCIIToUTF16(name);
  field->value = ASCIIToUTF16(value);
  field->form_control_type = type;
  field->is_focusable = true;
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         const char* autocomplete,
                         FormFieldData* field) {
  CreateTestFormField(label, name, value, type, field);
  field->autocomplete_attribute = autocomplete;
  field->parsed_autocomplete =
      ParseAutocompleteAttribute(autocomplete, field->max_length);
}

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         const char* autocomplete,
                         uint64_t max_length,
                         FormFieldData* field) {
  // First, set the `max_length`, as the `parsed_autocomplete` is set based on
  // this value.
  field->max_length = max_length;
  CreateTestFormField(label, name, value, type, autocomplete, field);
}

void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "select-one", field);

  field->options.clear();
  CHECK_EQ(values.size(), contents.size());
  for (size_t i = 0; i < std::min(values.size(), contents.size()); ++i) {
    field->options.push_back({
        .value = base::UTF8ToUTF16(values[i]),
        .content = base::UTF8ToUTF16(contents[i]),
    });
  }
}

void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const char* autocomplete,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           FormFieldData* field) {
  CreateTestSelectField(label, name, value, values, contents, field);
  field->autocomplete_attribute = autocomplete;
  field->parsed_autocomplete =
      ParseAutocompleteAttribute(autocomplete, field->max_length);
}

void CreateTestSelectField(const std::vector<const char*>& values,
                           FormFieldData* field) {
  CreateTestSelectField("", "", "", values, values, field);
}

void CreateTestDatalistField(const char* label,
                             const char* name,
                             const char* value,
                             const std::vector<const char*>& values,
                             const std::vector<const char*>& labels,
                             FormFieldData* field) {
  // Fill the base attributes.
  CreateTestFormField(label, name, value, "text", field);

  std::vector<std::u16string> values16(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    values16[i] = base::UTF8ToUTF16(values[i]);

  std::vector<std::u16string> label16(labels.size());
  for (size_t i = 0; i < labels.size(); ++i)
    label16[i] = base::UTF8ToUTF16(labels[i]);

  field->datalist_values = values16;
  field->datalist_labels = label16;
}

void CreateTestAddressFormData(FormData* form, const char* unique_id) {
  std::vector<ServerFieldTypeSet> types;
  CreateTestAddressFormData(form, &types, unique_id);
}

void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types,
                               const char* unique_id) {
  form->host_frame = MakeLocalFrameToken();
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  form->button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->is_action_empty = true;
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  types->clear();
  form->submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({NAME_FIRST});
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({NAME_MIDDLE});
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({NAME_LAST, NAME_LAST_SECOND});
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_LINE1});
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_SUBPREMISE, ADDRESS_HOME_LINE2});
  test::CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_CITY});
  test::CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_STATE});
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_ZIP});
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  types->push_back({ADDRESS_HOME_COUNTRY});
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  types->push_back({PHONE_HOME_WHOLE_NUMBER});
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
  types->push_back({EMAIL_ADDRESS});
}

void CreateTestPersonalInformationFormData(FormData* form,
                                           const char* unique_id) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  form->url = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type,
                                  bool split_names,
                                  const char* unique_id) {
  form->unique_renderer_id = MakeFormRendererId();
  form->name = u"MyForm" + ASCIIToUTF16(unique_id ? unique_id : "");
  if (is_https) {
    form->url = GURL("https://myform.com/form.html");
    form->action = GURL("https://myform.com/submit.html");
    form->main_frame_origin =
        url::Origin::Create(GURL("https://myform_root.com/form.html"));
  } else {
    form->url = GURL("http://myform.com/form.html");
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
    test::CreateTestFormField("Last Name on Card", "lastnameoncard", "", "text",
                              &field);
    field.autocomplete_attribute = "cc-family-name";
    form->fields.push_back(field);
    field.autocomplete_attribute = "";
  } else {
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                              &field);
    form->fields.push_back(field);
  } else {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form->fields.push_back(field);
}

void CreateTestIbanFormData(FormData* form_data, const char* value) {
  FormFieldData field;
  test::CreateTestFormField("IBAN Value:", "iban_value", value, "text", &field);
  form_data->fields.push_back(field);
}

FormData WithoutUnserializedData(FormData form) {
  form.url = {};
  form.main_frame_origin = {};
  form.host_frame = {};
  for (FormFieldData& field : form.fields)
    field = WithoutUnserializedData(std::move(field));
  return form;
}

FormFieldData WithoutUnserializedData(FormFieldData field) {
  field.host_frame = {};
  return field;
}

inline void check_and_set(
    FormGroup* profile,
    ServerFieldType type,
    const char* value,
    VerificationStatus status = VerificationStatus::kObserved) {
  if (value) {
    profile->SetRawInfoWithVerificationStatus(type, base::UTF8ToUTF16(value),
                                              status);
  }
}

AutofillProfile GetFullValidProfileForCanada() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "Alice", "", "Wonderland", "alice@wonderland.ca",
                 "Fiction", "666 Notre-Dame Ouest", "Apt 8", "Montreal", "QC",
                 "H3B 2T9", "CA", "15141112233");
  return profile;
}

AutofillProfile GetFullProfile() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "johndoe@hades.com",
                 "Underworld", "666 Erebus St.", "Apt 8", "Elysium", "CA",
                 "91111", "US", "16502111111");
  return profile;
}

AutofillProfile GetFullProfile2() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "Jane", "A.", "Smith", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "13105557889");
  return profile;
}

AutofillProfile GetFullCanadianProfile() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "Wayne", "", "Gretzky", "wayne@hockey.com", "NHL",
                 "123 Hockey rd.", "Apt 8", "Moncton", "New Brunswick",
                 "E1A 0A6", "CA", "15068531212");
  return profile;
}

AutofillProfile GetIncompleteProfile1() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "John", "H.", "Doe", "jsmith@example.com", "ACME",
                 "123 Main Street", "Unit 1", "Greensdale", "MI", "48838", "US",
                 "");
  return profile;
}

AutofillProfile GetIncompleteProfile2() {
  AutofillProfile profile(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kEmptyOrigin);
  SetProfileInfo(&profile, "", "", "", "jsmith@example.com", "", "", "", "", "",
                 "", "", "");
  return profile;
}

AutofillProfile GetVerifiedProfile() {
  AutofillProfile profile(GetFullProfile());
  profile.set_origin(kSettingsOrigin);
  return profile;
}

AutofillProfile GetServerProfile() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id1");
  // Note: server profiles don't have email addresses and only have full names.
  SetProfileInfo(&profile, "", "", "", "", "Google, Inc.", "123 Fake St.",
                 "Apt. 42", "Mountain View", "California", "94043", "US",
                 "1.800.555.1234");

  profile.SetInfo(NAME_FULL, u"John K. Doe", "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"CEDEX");
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Santa Clara");

  profile.set_language_code("en");
  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(54321));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

AutofillProfile GetServerProfile2() {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "id2");
  // Note: server profiles don't have email addresses.
  SetProfileInfo(&profile, "", "", "", "", "Main, Inc.", "4323 Wrong St.",
                 "Apt. 1032", "Sunnyvale", "California", "10011", "US",
                 "+1 514-123-1234");

  profile.SetInfo(NAME_FULL, u"Jim S. Bristow", "en");
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"XEDEC");
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Santa Monica");

  profile.set_language_code("en");
  profile.set_use_count(14);
  profile.set_use_date(base::Time::FromTimeT(98765));

  profile.GenerateServerProfileIdentifier();

  return profile;
}

void SetProfileCategory(
    AutofillProfile& profile,
    autofill_metrics::AutofillProfileSourceCategory category) {
  switch (category) {
    case autofill_metrics::AutofillProfileSourceCategory::kLocalOrSyncable:
      profile.set_source_for_testing(AutofillProfile::Source::kLocalOrSyncable);
      break;
    case autofill_metrics::AutofillProfileSourceCategory::kAccountChrome:
    case autofill_metrics::AutofillProfileSourceCategory::kAccountNonChrome:
      profile.set_source_for_testing(AutofillProfile::Source::kAccount);
      // Any value that is not kInitialCreatorOrModifierChrome works.
      const int kInitialCreatorOrModifierNonChrome =
          AutofillProfile::kInitialCreatorOrModifierChrome + 1;
      profile.set_initial_creator_id(
          category == autofill_metrics::AutofillProfileSourceCategory::
                          kAccountChrome
              ? AutofillProfile::kInitialCreatorOrModifierChrome
              : kInitialCreatorOrModifierNonChrome);
      break;
  }
}

std::string GetStrippedValue(const char* value) {
  std::u16string stripped_value;
  base::RemoveChars(base::UTF8ToUTF16(value), base::kWhitespaceUTF16,
                    &stripped_value);
  return base::UTF16ToUTF8(stripped_value);
}

IBAN GetIBAN() {
  IBAN iban(base::Uuid::GenerateRandomV4().AsLowercaseString());
  iban.set_value(base::UTF8ToUTF16(std::string(kIbanValue)));
  iban.set_nickname(u"Nickname for Iban");
  return iban;
}

IBAN GetIBAN2() {
  IBAN iban;
  iban.set_value(base::UTF8ToUTF16(std::string(kIbanValue_1)));
  iban.set_nickname(u"My doctor's IBAN");
  return iban;
}

IBAN GetIBANWithoutNickname() {
  IBAN iban;
  iban.set_value(base::UTF8ToUTF16(std::string(kIbanValue_2)));
  return iban;
}

CreditCard GetCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetCreditCard2() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Someone Else", "378282246310005" /* AmEx */,
                    NextMonth().c_str(), TenYearsFromNow().c_str(), "1");
  return credit_card;
}

CreditCard GetExpiredCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "Test User", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), LastYear().c_str(), "1");
  return credit_card;
}

CreditCard GetIncompleteCreditCard() {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kEmptyOrigin);
  SetCreditCardInfo(&credit_card, "", "4111111111111111" /* Visa */,
                    NextMonth().c_str(), NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetVerifiedCreditCard() {
  CreditCard credit_card(GetCreditCard());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetVerifiedCreditCard2() {
  CreditCard credit_card(GetCreditCard2());
  credit_card.set_origin(kSettingsOrigin);
  return credit_card;
}

CreditCard GetMaskedServerCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  credit_card.set_instrument_id(1);
  return credit_card;
}

CreditCard GetMaskedServerCardWithLegacyId() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardWithNonLegacyId() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, 1);
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kMasterCard);
  return credit_card;
}

CreditCard GetMaskedServerCardVisa() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  return credit_card;
}

CreditCard GetMaskedServerCardAmex() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card, "Justin Thyme", "8431" /* Amex */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kAmericanExpressCard);
  return credit_card;
}

CreditCard GetMaskedServerCardWithNickname() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card, "Test user", "1111" /* Visa */,
                          NextMonth().c_str(), NextYear().c_str(), "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  credit_card.SetNickname(u"Test nickname");
  return credit_card;
}

CreditCard GetMaskedServerCardEnrolledIntoVirtualCardNumber() {
  CreditCard credit_card = GetMaskedServerCard();
  credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  return credit_card;
}

CreditCard GetFullServerCard() {
  CreditCard credit_card(CreditCard::FULL_SERVER_CARD, "c123");
  test::SetCreditCardInfo(&credit_card, "Full Carter",
                          "4111111111111111" /* Visa */, NextMonth().c_str(),
                          NextYear().c_str(), "1");
  return credit_card;
}

CreditCard GetVirtualCard() {
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Lorem Ipsum",
                          "5555555555554444",  // Mastercard
                          "10", test::NextYear().c_str(), "1");
  credit_card.set_record_type(CreditCard::RecordType::VIRTUAL_CARD);
  credit_card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  CreditCardTestApi(&credit_card).set_network_for_virtual_card(kMasterCard);
  return credit_card;
}

CreditCard GetRandomCreditCard(CreditCard::RecordType record_type) {
  static const char* const kNetworks[] = {
      kAmericanExpressCard,
      kDinersCard,
      kDiscoverCard,
      kEloCard,
      kGenericCard,
      kJCBCard,
      kMasterCard,
      kMirCard,
      kUnionPay,
      kVisaCard,
  };
  constexpr size_t kNumNetworks = sizeof(kNetworks) / sizeof(kNetworks[0]);
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);

  CreditCard credit_card =
      (record_type == CreditCard::LOCAL_CARD)
          ? CreditCard(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                       kEmptyOrigin)
          : CreditCard(
                record_type,
                base::Uuid::GenerateRandomV4().AsLowercaseString().substr(24));
  test::SetCreditCardInfo(
      &credit_card, "Justin Thyme", GetRandomCardNumber().c_str(),
      base::StringPrintf("%d", base::RandInt(1, 12)).c_str(),
      base::StringPrintf("%d", now.year + base::RandInt(1, 4)).c_str(), "1");
  if (record_type == CreditCard::MASKED_SERVER_CARD) {
    credit_card.SetNetworkForMaskedCard(
        kNetworks[base::RandInt(0, kNumNetworks - 1)]);
  }

  return credit_card;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData1() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data1_id";
  data.suffix = u"1111";
  data.exp_month = 1;
  base::StringToInt(NextYear(), &data.exp_year);
  data.card_art_url = "fake url 1";
  data.instrument_token = "fake token 1";
  return data;
}

CreditCardCloudTokenData GetCreditCardCloudTokenData2() {
  CreditCardCloudTokenData data;
  data.masked_card_id = "data2_id";
  data.suffix = u"2222";
  data.exp_month = 2;
  base::StringToInt(NextYear(), &data.exp_year);
  data.exp_year += 1;
  data.card_art_url = "fake url 2";
  data.instrument_token = "fake token 2";
  return data;
}

AutofillOfferData GetCardLinkedOfferData1(int64_t offer_id) {
  // Sets the expiry to be 45 days later.
  base::Time expiry = AutofillClock::Now() + base::Days(45);
  GURL offer_details_url = GURL("http://www.example1.com");
  std::vector<GURL> merchant_origins{offer_details_url};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "Get 5% off your purchase";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Check out with this card to activate";
  std::string offer_reward_amount = "5%";
  std::vector<int64_t> eligible_instrument_id{111111};

  return AutofillOfferData::GPayCardLinkedOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      eligible_instrument_id, offer_reward_amount);
}

AutofillOfferData GetCardLinkedOfferData2(int64_t offer_id) {
  // Sets the expiry to be 40 days later.
  base::Time expiry = AutofillClock::Now() + base::Days(40);
  GURL offer_details_url = GURL("http://www.example2.com");
  std::vector<GURL> merchant_origins{offer_details_url};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "Get $10 off your purchase";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Check out with this card to activate";
  std::string offer_reward_amount = "$10";
  std::vector<int64_t> eligible_instrument_id{222222};

  return AutofillOfferData::GPayCardLinkedOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      eligible_instrument_id, offer_reward_amount);
}

AutofillOfferData GetPromoCodeOfferData(GURL origin,
                                        bool is_expired,
                                        int64_t offer_id) {
  // Sets the expiry to be later if not expired, or earlier if expired.
  base::Time expiry = is_expired ? AutofillClock::Now() - base::Days(1)
                                 : AutofillClock::Now() + base::Days(35);
  std::vector<GURL> merchant_origins{origin};
  DisplayStrings display_strings;
  display_strings.value_prop_text = "5% off on shoes. Up to $50.";
  display_strings.see_details_text = "See details";
  display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";
  std::string promo_code = "5PCTOFFSHOES";
  GURL offer_details_url = GURL("https://pay.google.com");

  return AutofillOfferData::GPayPromoCodeOffer(
      offer_id, expiry, merchant_origins, offer_details_url, display_strings,
      promo_code);
}

VirtualCardUsageData GetVirtualCardUsageData1() {
  return VirtualCardUsageData(
      VirtualCardUsageData::UsageDataId(
          "VirtualCardUsageData|12345|google|https://www.google.com"),
      VirtualCardUsageData::InstrumentId(12345),
      VirtualCardUsageData::VirtualCardLastFour(u"1234"),
      url::Origin::Create(GURL("https://www.google.com")));
}

VirtualCardUsageData GetVirtualCardUsageData2() {
  return VirtualCardUsageData(
      VirtualCardUsageData::UsageDataId(
          "VirtualCardUsageData|23456|google|https://www.pay.google.com"),
      VirtualCardUsageData::InstrumentId(23456),
      VirtualCardUsageData::VirtualCardLastFour(u"2345"),
      url::Origin::Create(GURL("https://www.pay.google.com")));
}

std::vector<CardUnmaskChallengeOption> GetCardUnmaskChallengeOptions(
    const std::vector<CardUnmaskChallengeOptionType>& types) {
  std::vector<CardUnmaskChallengeOption> challenge_options;
  for (CardUnmaskChallengeOptionType type : types) {
    switch (type) {
      case CardUnmaskChallengeOptionType::kSmsOtp:
        challenge_options.emplace_back(CardUnmaskChallengeOption(
            CardUnmaskChallengeOption::ChallengeOptionId("123"), type,
            /*challenge_info=*/u"xxx-xxx-3547",
            /*challenge_input_length=*/6U));
        break;
      case CardUnmaskChallengeOptionType::kCvc:
        challenge_options.emplace_back(CardUnmaskChallengeOption(
            CardUnmaskChallengeOption::ChallengeOptionId("234"), type,
            /*challenge_info=*/
            u"3 digit security code on the back of your card",
            /*challenge_input_length=*/3U,
            /*cvc_position=*/CvcPosition::kBackOfCard));
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
        challenge_options.emplace_back(
            CardUnmaskChallengeOption::ChallengeOptionId("345"), type,
            /*challenge_info=*/u"a******b@google.com",
            /*challenge_input_length=*/6U);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return challenge_options;
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    VerificationStatus status) {
  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_DEPENDENT_LOCALITY, dependent_locality,
                status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize,
                    VerificationStatus status) {
  check_and_set(profile, NAME_FIRST, first_name, status);
  check_and_set(profile, NAME_MIDDLE, middle_name, status);
  check_and_set(profile, NAME_LAST, last_name, status);
  check_and_set(profile, EMAIL_ADDRESS, email, status);
  check_and_set(profile, COMPANY_NAME, company, status);
  check_and_set(profile, ADDRESS_HOME_LINE1, address1, status);
  check_and_set(profile, ADDRESS_HOME_LINE2, address2, status);
  check_and_set(profile, ADDRESS_HOME_CITY, city, status);
  check_and_set(profile, ADDRESS_HOME_STATE, state, status);
  check_and_set(profile, ADDRESS_HOME_ZIP, zipcode, status);
  check_and_set(profile, ADDRESS_HOME_COUNTRY, country, status);
  check_and_set(profile, PHONE_HOME_WHOLE_NUMBER, phone, status);
  if (finalize)
    profile->FinalizeAfterImport();
}

void SetProfileInfoWithGuid(AutofillProfile* profile,
                            const char* guid,
                            const char* first_name,
                            const char* middle_name,
                            const char* last_name,
                            const char* email,
                            const char* company,
                            const char* address1,
                            const char* address2,
                            const char* city,
                            const char* state,
                            const char* zipcode,
                            const char* country,
                            const char* phone,
                            bool finalize,
                            VerificationStatus status) {
  if (guid)
    profile->set_guid(guid);
  SetProfileInfo(profile, first_name, middle_name, last_name, email, company,
                 address1, address2, city, state, zipcode, country, phone,
                 finalize, status);
}

void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id) {
  check_and_set(credit_card, CREDIT_CARD_NAME_FULL, name_on_card);
  check_and_set(credit_card, CREDIT_CARD_NUMBER, card_number);
  check_and_set(credit_card, CREDIT_CARD_EXP_MONTH, expiration_month);
  check_and_set(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR, expiration_year);
  credit_card->set_billing_address_id(billing_address_id);
}

void DisableSystemServices(PrefService* prefs) {
  // Use a mock Keychain rather than the OS one to store credit card data.
  OSCryptMocker::SetUp();
}

void ReenableSystemServices() {
  OSCryptMocker::TearDown();
}

void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards) {
  std::vector<CreditCard> as_masked_cards = cards;
  for (CreditCard& card : as_masked_cards) {
    card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    card.SetNumber(card.LastFourDigits());
    card.SetNetworkForMaskedCard(card.network());
    card.set_instrument_id(card.instrument_id());
  }
  table->SetServerCreditCards(as_masked_cards);

  for (const CreditCard& card : cards) {
    if (card.record_type() != CreditCard::FULL_SERVER_CARD)
      continue;
    ASSERT_TRUE(table->UnmaskServerCreditCard(card, card.number()));
  }
}

void InitializePossibleTypesAndValidities(
    std::vector<ServerFieldTypeSet>& possible_field_types,
    std::vector<ServerFieldTypeValidityStatesMap>&
        possible_field_types_validities,
    const std::vector<ServerFieldType>& possible_types,
    const std::vector<AutofillDataModel::ValidityState>& validity_states) {
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types_validities.push_back(ServerFieldTypeValidityStatesMap());

  if (validity_states.empty()) {
    for (const auto& possible_type : possible_types) {
      possible_field_types.back().insert(possible_type);
      possible_field_types_validities.back()[possible_type].push_back(
          AutofillProfile::UNVALIDATED);
    }
    return;
  }

  ASSERT_FALSE(possible_types.empty());
  ASSERT_TRUE((possible_types.size() == validity_states.size()) ||
              (possible_types.size() == 1 && validity_states.size() > 1));

  ServerFieldType possible_type = possible_types[0];
  for (unsigned i = 0; i < validity_states.size(); ++i) {
    if (possible_types.size() == validity_states.size()) {
      possible_type = possible_types[i];
    }
    possible_field_types.back().insert(possible_type);
    possible_field_types_validities.back()[possible_type].push_back(
        validity_states[i]);
  }
}

void BasicFillUploadField(AutofillUploadContents::Field* field,
                          unsigned signature,
                          const char* name,
                          const char* control_type,
                          const char* autocomplete) {
  field->set_signature(signature);
  if (name)
    field->set_name(name);
  if (control_type)
    field->set_type(control_type);
  if (autocomplete)
    field->set_autocomplete(autocomplete);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     unsigned validity_state) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);

  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  type_validities->add_validity(validity_state);
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     const std::vector<unsigned>& autofill_types,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  for (unsigned i = 0; i < autofill_types.size(); ++i) {
    field->add_autofill_type(autofill_types[i]);

    auto* type_validities = field->add_autofill_type_validities();
    type_validities->set_type(autofill_types[i]);
    if (i < validity_states.size()) {
      type_validities->add_validity(validity_states[i]);
    } else {
      type_validities->add_validity(0);
    }
  }
}

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     const std::vector<unsigned>& validity_states) {
  BasicFillUploadField(field, signature, name, control_type, autocomplete);

  field->add_autofill_type(autofill_type);
  auto* type_validities = field->add_autofill_type_validities();
  type_validities->set_type(autofill_type);
  for (unsigned i = 0; i < validity_states.size(); ++i)
    type_validities->add_validity(validity_states[i]);
}

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  FormData form;
  FormFieldData field;
  form.host_frame = MakeLocalFrameToken();
  form.unique_renderer_id = MakeFormRendererId();
  field.host_frame = MakeLocalFrameToken();
  field.unique_renderer_id = MakeFieldRendererId();
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::RectF bounds(100.f, 100.f);
  autofill_external_delegate->OnQuery(form, field, bounds);

  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(u"Test suggestion"));
  autofill_external_delegate->OnSuggestionsReturned(
      field.global_id(), suggestions, AutoselectFirstSuggestion(false));
}

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str,
                                       int obfuscation_length) {
  return base::UTF16ToUTF8(internal::GetObfuscatedStringForCardDigits(
      base::ASCIIToUTF16(str), obfuscation_length));
}

std::string NextMonth() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::StringPrintf("%02d", now.month % 12 + 1);
}
std::string LastYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year - 1);
}
std::string NextYear() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 1);
}
std::string TenYearsFromNow() {
  base::Time::Exploded now;
  AutofillClock::Now().LocalExplode(&now);
  return base::NumberToString(now.year + 10);
}

std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form) {
  std::vector<FormSignature> signatures;
  signatures.push_back(form.form_signature());
  return signatures;
}

std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<FormStructure*>& forms) {
  std::vector<FormSignature> all_signatures;
  for (const FormStructure* form : forms)
    all_signatures.push_back(form->form_signature());
  return all_signatures;
}

FieldPrediction CreateFieldPrediction(ServerFieldType type,
                                      FieldPrediction::Source source) {
  FieldPrediction field_prediction;
  field_prediction.set_type(type);
  field_prediction.set_source(source);
  if (source == FieldPrediction::SOURCE_OVERRIDE)
    field_prediction.set_override(true);
  return field_prediction;
}

FieldPrediction CreateFieldPrediction(ServerFieldType type, bool is_override) {
  if (is_override) {
    return CreateFieldPrediction(type, FieldPrediction::SOURCE_OVERRIDE);
  }
  if (type == NO_SERVER_DATA) {
    return CreateFieldPrediction(type, FieldPrediction::SOURCE_UNSPECIFIED);
  }
  return CreateFieldPrediction(
      type, GroupTypeOfServerFieldType(type) == FieldTypeGroup::kPasswordField
                ? FieldPrediction::SOURCE_PASSWORDS_DEFAULT
                : FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
}

void AddFieldPredictionToForm(
    const FormFieldData& field_data,
    ServerFieldType field_type,
    AutofillQueryResponse_FormSuggestion* form_suggestion,
    bool is_override) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  *field_suggestion->add_predictions() =
      CreateFieldPrediction(field_type, is_override);
}

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<ServerFieldType>& field_types,
    AutofillQueryResponse_FormSuggestion* form_suggestion) {
  std::vector<FieldPrediction> field_predictions;
  field_predictions.reserve(field_types.size());
  base::ranges::transform(field_types, std::back_inserter(field_predictions),
                          [](ServerFieldType field_type) {
                            return CreateFieldPrediction(field_type);
                          });
  return AddFieldPredictionsToForm(field_data, field_predictions,
                                   form_suggestion);
}

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<FieldPrediction>& field_predictions,
    AutofillQueryResponse_FormSuggestion* form_suggestion) {
  auto* field_suggestion = form_suggestion->add_field_suggestions();
  field_suggestion->set_field_signature(
      CalculateFieldSignatureForField(field_data).value());
  for (const auto& prediction : field_predictions) {
    *field_suggestion->add_predictions() = prediction;
  }
}

Suggestion CreateAutofillSuggestion(int frontend_id,
                                    const std::u16string& main_text_value,
                                    const Suggestion::Payload& payload) {
  Suggestion suggestion;
  suggestion.frontend_id = frontend_id;
  suggestion.main_text.value = main_text_value;
  suggestion.payload = payload;
  return suggestion;
}

}  // namespace test
}  // namespace autofill
