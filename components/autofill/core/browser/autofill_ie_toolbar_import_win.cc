// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ie_toolbar_import_win.h"

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/win/registry.h"
#include "components/autofill/core/browser/crypto/rc4_decryptor.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/os_crypt/os_crypt.h"

using base::win::RegKey;

namespace autofill {

// Forward declaration. This function is not in unnamed namespace as it
// is referenced in the unittest.
bool ImportCurrentUserProfiles(const std::string& app_locale,
                               std::vector<AutofillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards);
namespace {

const wchar_t* const kProfileKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Profiles";
const wchar_t* const kCreditCardKey =
    L"Software\\Google\\Google Toolbar\\4.0\\Autofill\\Credit Cards";
const wchar_t* const kPasswordHashValue = L"password_hash";
const wchar_t* const kSaltValue = L"salt";

// This string is stored along with saved addresses and credit cards in the
// WebDB, and hence should not be modified, so that it remains consistent over
// time.
const char kIEToolbarImportOrigin[] = "Imported from Internet Explorer";

// This is RC4 decryption for Toolbar credit card data. This is necessary
// because it is not standard, so Crypto API cannot be used.
std::wstring DecryptCCNumber(const std::wstring& data) {
  const wchar_t* kEmptyKey =
    L"\x3605\xCEE5\xCE49\x44F7\xCF4E\xF6CC\x604B\xFCBE\xC70A\x08FD";
  const size_t kMacLen = 10;

  if (data.length() <= kMacLen)
    return std::wstring();

  RC4Decryptor rc4_algorithm(kEmptyKey);
  return rc4_algorithm.Run(data.substr(kMacLen));
}

bool IsEmptySalt(std::wstring const& salt) {
  // Empty salt in IE Toolbar is \x1\x2...\x14
  if (salt.length() != 20)
    return false;
  for (size_t i = 0; i < salt.length(); ++i) {
    if (salt[i] != i + 1)
      return false;
  }
  return true;
}

base::string16 ReadAndDecryptValue(const RegKey& key,
                                   const wchar_t* value_name) {
  DWORD data_type = REG_BINARY;
  DWORD data_size = 0;
  LONG result = key.ReadValue(value_name, nullptr, &data_size, &data_type);
  if ((result != ERROR_SUCCESS) || !data_size || data_type != REG_BINARY)
    return base::string16();
  std::string data;
  data.resize(data_size);
  result = key.ReadValue(value_name, &(data[0]), &data_size, &data_type);
  if (result == ERROR_SUCCESS) {
    std::string out_data;
    if (OSCrypt::DecryptString(data, &out_data)) {
      // The actual data is in UTF16 already.
      if (!(out_data.size() & 1) && (out_data.size() > 2) &&
          !out_data[out_data.size() - 1] && !out_data[out_data.size() - 2]) {
        return base::string16(
            reinterpret_cast<const wchar_t *>(out_data.c_str()));
      }
    }
  }
  return base::string16();
}

struct {
  ServerFieldType field_type;
  const wchar_t *reg_value_name;
} profile_reg_values[] = {
    {NAME_FIRST, L"name_first"},
    {NAME_MIDDLE, L"name_middle"},
    {NAME_LAST, L"name_last"},
    {NAME_SUFFIX, L"name_suffix"},
    {EMAIL_ADDRESS, L"email"},
    {COMPANY_NAME, L"company_name"},
    {PHONE_HOME_NUMBER, L"phone_home_number"},
    {PHONE_HOME_CITY_CODE, L"phone_home_city_code"},
    {PHONE_HOME_COUNTRY_CODE, L"phone_home_country_code"},
    {ADDRESS_HOME_LINE1, L"address_home_line1"},
    {ADDRESS_HOME_LINE2, L"address_home_line2"},
    {ADDRESS_HOME_CITY, L"address_home_city"},
    {ADDRESS_HOME_STATE, L"address_home_state"},
    {ADDRESS_HOME_ZIP, L"address_home_zip"},
    {ADDRESS_HOME_COUNTRY, L"address_home_country"},
    {ADDRESS_BILLING_LINE1, L"address_billing_line1"},
    {ADDRESS_BILLING_LINE2, L"address_billing_line2"},
    {ADDRESS_BILLING_CITY, L"address_billing_city"},
    {ADDRESS_BILLING_STATE, L"address_billing_state"},
    {ADDRESS_BILLING_ZIP, L"address_billing_zip"},
    {ADDRESS_BILLING_COUNTRY, L"address_billing_country"},
    {CREDIT_CARD_NAME_FULL, L"credit_card_name_full"},
    {CREDIT_CARD_NUMBER, L"credit_card_number"},
    {CREDIT_CARD_EXP_MONTH, L"credit_card_exp_month"},
    {CREDIT_CARD_EXP_4_DIGIT_YEAR, L"credit_card_exp_4_digit_year"},
    {CREDIT_CARD_TYPE, L"credit_card_type"},
    // We do not import verification code.
};

typedef std::map<std::wstring, ServerFieldType> RegToFieldMap;

// Imports address or credit card data from the given registry |key| into the
// given |form_group|, with the help of |reg_to_field|.  When importing address
// data, writes the phone data into |phone|; otherwise, |phone| should be null.
// Returns true if any fields were set, false otherwise.
bool ImportSingleFormGroup(const RegKey& key,
                           const RegToFieldMap& reg_to_field,
                           const std::string& app_locale,
                           FormGroup* form_group,
                           PhoneNumber::PhoneCombineHelper* phone) {
  if (!key.Valid())
    return false;

  bool has_non_empty_fields = false;

  for (uint32_t i = 0; i < key.GetValueCount(); ++i) {
    std::wstring value_name;
    if (key.GetValueNameAt(i, &value_name) != ERROR_SUCCESS)
      continue;

    RegToFieldMap::const_iterator it = reg_to_field.find(value_name);
    if (it == reg_to_field.end())
      continue;  // This field is not imported.

    base::string16 field_value = ReadAndDecryptValue(key, value_name.c_str());
    if (!field_value.empty()) {
      if (it->second == CREDIT_CARD_NUMBER)
        field_value = DecryptCCNumber(field_value);

      // Phone numbers are stored piece-by-piece, and then reconstructed from
      // the pieces.  The rest of the fields are set "as is".
      if (!phone || !phone->SetInfo(AutofillType(it->second), field_value)) {
        has_non_empty_fields = true;
        form_group->SetInfo(AutofillType(it->second), field_value, app_locale);
      }
    }
  }

  return has_non_empty_fields;
}

// Imports address data from the given registry |key| into the given |profile|,
// with the help of |reg_to_field|.  Returns true if any fields were set, false
// otherwise.
bool ImportSingleProfile(const std::string& app_locale,
                         const RegKey& key,
                         const RegToFieldMap& reg_to_field,
                         AutofillProfile* profile) {
  PhoneNumber::PhoneCombineHelper phone;
  bool has_non_empty_fields =
      ImportSingleFormGroup(key, reg_to_field, app_locale, profile, &phone);

  // Now re-construct the phones if needed.
  base::string16 constructed_number;
  if (phone.ParseNumber(*profile, app_locale, &constructed_number)) {
    has_non_empty_fields = true;
    profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, constructed_number);
  }

  return has_non_empty_fields;
}

// Imports profiles from the IE toolbar and stores them. Asynchronous
// if PersonalDataManager has not been loaded yet. Deletes itself on completion.
class AutofillImporter : public PersonalDataManagerObserver {
 public:
  explicit AutofillImporter(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {
      personal_data_manager_->AddObserver(this);
  }

  bool ImportProfiles() {
    if (!ImportCurrentUserProfiles(personal_data_manager_->app_locale(),
                                   &profiles_,
                                   &credit_cards_)) {
      delete this;
      return false;
    }
    if (personal_data_manager_->IsDataLoaded())
      OnPersonalDataChanged();
    return true;
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    for (const AutofillProfile& it : profiles_)
      personal_data_manager_->AddProfile(it);
    for (const CreditCard& it : credit_cards_)
      personal_data_manager_->AddCreditCard(it);
    delete this;
  }

 private:
  ~AutofillImporter() override { personal_data_manager_->RemoveObserver(this); }

  PersonalDataManager* personal_data_manager_;
  std::vector<AutofillProfile> profiles_;
  std::vector<CreditCard> credit_cards_;
};

}  // namespace

// Imports Autofill profiles and credit cards from IE Toolbar if present and not
// password protected. Returns true if data is successfully retrieved. False if
// there is no data, data is password protected or error occurred.
bool ImportCurrentUserProfiles(const std::string& app_locale,
                               std::vector<AutofillProfile>* profiles,
                               std::vector<CreditCard>* credit_cards) {
  DCHECK(profiles);
  DCHECK(credit_cards);

  // Create a map of possible fields for a quick access.
  RegToFieldMap reg_to_field;
  for (const auto& profile_reg_value : profile_reg_values) {
    reg_to_field[std::wstring(profile_reg_value.reg_value_name)] =
        profile_reg_value.field_type;
  }

  base::win::RegistryKeyIterator iterator_profiles(HKEY_CURRENT_USER,
                                                   kProfileKey);
  for (; iterator_profiles.Valid(); ++iterator_profiles) {
    std::wstring key_name(kProfileKey);
    key_name.append(L"\\");
    key_name.append(iterator_profiles.Name());
    RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
    AutofillProfile profile;
    profile.set_origin(kIEToolbarImportOrigin);
    if (ImportSingleProfile(app_locale, key, reg_to_field, &profile)) {
      // Combine phones into whole phone #.
      profiles->push_back(profile);
    }
  }
  base::string16 password_hash;
  base::string16 salt;
  RegKey cc_key(HKEY_CURRENT_USER, kCreditCardKey, KEY_READ);
  if (cc_key.Valid()) {
    password_hash = ReadAndDecryptValue(cc_key, kPasswordHashValue);
    salt = ReadAndDecryptValue(cc_key, kSaltValue);
  }

  // We import CC profiles only if they are not password protected.
  if (password_hash.empty() && IsEmptySalt(salt)) {
    base::win::RegistryKeyIterator iterator_cc(HKEY_CURRENT_USER,
                                               kCreditCardKey);
    for (; iterator_cc.Valid(); ++iterator_cc) {
      std::wstring key_name(kCreditCardKey);
      key_name.append(L"\\");
      key_name.append(iterator_cc.Name());
      RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ);
      CreditCard credit_card;
      credit_card.set_origin(kIEToolbarImportOrigin);
      if (ImportSingleFormGroup(key, reg_to_field, app_locale, &credit_card,
                                nullptr)) {
        base::string16 cc_number = credit_card.GetRawInfo(CREDIT_CARD_NUMBER);
        if (!cc_number.empty())
          credit_cards->push_back(credit_card);
      }
    }
  }
  return (profiles->size() + credit_cards->size()) > 0;
}

bool ImportAutofillDataWin(PersonalDataManager* pdm) {
  // In incognito mode we do not have PDM - and we should not import anything.
  if (!pdm)
    return false;
  AutofillImporter* importer = new AutofillImporter(pdm);
  // importer will self delete.
  return importer->ImportProfiles();
}

}  // namespace autofill
