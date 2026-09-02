// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_PHONE_NUMBER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_PHONE_NUMBER_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"

namespace autofill {

class AutofillProfile;

// A form group that stores phone number information.
//
// The behavior of PhoneNumber is quite complex because of different
// representations of phone numbers (national and international formats) and the
// number of field types. See components/autofill/core/browser/field_types.h for
// an introduction to the semantic field types.
//
// The PhoneNumber/PhoneImportAndGetTest.TestSettingAndParsing unittests may be
// best to see the exact behavior of learning phone numbers from submitted forms
// and filling phone numbers into new forms.
//
// Phone numbers of form submissions are validated by libphonenumber for
// plausibility before getting saved (in the context of the country, which is
// the first of 1) country in the form, 2) country of GeoIP, 3) country of
// locale). Phone numbers from form submissions are stored in a formatted way
// unless they were submitted in a PHONE_HOME_WHOLE_NUMBER field and already
// contained formatting characters (whitespaces, parentheses, slashes, hyphens,
// ...).
//
// At filling time, the stored number is interpreted and, if successful, the
// relevant pieces are returned. The values used for filling consist only of
// [+0123456789]. Whitespaces, parentheses, slashes, hyphens, ... are stripped.
// International numbers filled as PHONE_HOME_WHOLE_NUMBER start with a + in all
// countries but the US, where the + is dropped.
class PhoneNumber : public FormGroup {
 public:
  // See `AutofillProfile::kDatabaseStoredTypes` for a documentation of the
  // purpose of this constant.
  static constexpr FieldTypeSet kDatabaseStoredTypes{PHONE_HOME_WHOLE_NUMBER};

  explicit PhoneNumber(const AutofillProfile* profile);
  PhoneNumber(const PhoneNumber& number);
  PhoneNumber(PhoneNumber&& number) noexcept;
  ~PhoneNumber() override;

  PhoneNumber& operator=(const PhoneNumber& number);
  PhoneNumber& operator=(PhoneNumber&& number) noexcept;
  bool operator==(const PhoneNumber& other) const;

  void set_profile(const AutofillProfile* profile) { profile_ = profile; }

  // FormGroup implementation:
  void GetMatchingTypes(std::u16string_view text,
                        std::string_view app_locale,
                        FieldTypeSet* matching_types) const override;
  using FormGroup::GetInfo;
  std::u16string GetInfo(const AutofillType& type,
                         std::string_view app_locale) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        std::u16string_view value,
                                        VerificationStatus status) override;
  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     std::u16string_view value,
                                     std::string_view app_locale,
                                     const VerificationStatus status) override;
  VerificationStatus GetVerificationStatus(FieldType type) const override;

  // The class used to combine home phone parts into a whole number.
  class PhoneCombineHelper {
   public:
    static PhoneCombineHelper FromObservedValues(
        const base::flat_map<FieldType, std::u16string>& observed_values);

    PhoneCombineHelper();
    PhoneCombineHelper(const PhoneCombineHelper&);
    PhoneCombineHelper(PhoneCombineHelper&&);
    PhoneCombineHelper& operator=(const PhoneCombineHelper&);
    PhoneCombineHelper& operator=(PhoneCombineHelper&&);
    ~PhoneCombineHelper();

    // Processes the `value` accordingly given a phone number `field_type`.
    void SetInfo(FieldType field_type, std::u16string_view value);

    // Parses the number built up from pieces stored via `SetInfo()` according
    // to the given two-letter region code and returns it as a string in
    // international format if it is a valid phone number. The region code
    // should be one of the values returned by `autofill::GetCountryCodes()`.
    std::optional<std::u16string> ParseNumber(const std::string& region) const;

    // Parses the combined phone number and returns the region to which it
    // belongs. The region code is the two-letter string according to ISO-3166
    // that represents the country of the phone number.
    std::optional<std::u16string> GetRegionCode() const;

    // Returns true if both `phone_` and `whole_number_` are empty.
    bool IsEmpty() const;

   private:
    std::u16string country_;
    std::u16string city_;
    std::u16string phone_;
    std::u16string whole_number_;
  };

  // Imports the `combined_phone` number into `profile`, interpreting it from
  // the perspective of the the country stored in `profile` or (if that's empty)
  // `app_locale`.
  // Returns whether the phonenumber was successfully parsed and stored.
  static bool ImportPhoneNumberToProfile(
      const PhoneNumber::PhoneCombineHelper& combined_phone,
      std::string_view app_locale,
      AutofillProfile& profile);

 private:
  // FormGroup:
  FieldTypeSet GetSupportedTypes() const override;

  // Updates the cached parsed number if the profile's region has changed
  // since the last time the cache was updated.
  void UpdateCacheIfNeeded(std::string_view app_locale) const;

  // The phone number.
  std::u16string number_;
  // Profile which stores the region used as hint when normalizing the number.
  raw_ptr<const AutofillProfile> profile_;

  // Cached number.
  mutable i18n::PhoneObject cached_parsed_phone_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_PHONE_NUMBER_H_
