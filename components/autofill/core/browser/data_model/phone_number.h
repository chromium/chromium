// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/form_group.h"
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
// If no country code is submitted (as a separate PHONE_HOME_COUNTRY_CODE field
// or as part of a PHONE_HOME_WHOLE_NUMBER or PHONE_HOME_CITY_AND_NUMBER) at
// form submission time, no attempt is made to save one. As a consequence, we
// cannot fill country code fields nor international phone number fields with
// the country code. See b/322330285.
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
  explicit PhoneNumber(const AutofillProfile* profile);
  PhoneNumber(const PhoneNumber& number);
  ~PhoneNumber() override;

  PhoneNumber& operator=(const PhoneNumber& number);
  bool operator==(const PhoneNumber& other) const;

  void set_profile(const AutofillProfile* profile) { profile_ = profile; }

  // FormGroup implementation:
  void GetMatchingTypesWithProfileSources(
      const std::u16string& text,
      const std::string& app_locale,
      FieldTypeSet* matching_types,
      PossibleProfileValueSources* profile_value_sources) const override;
  std::u16string GetRawInfo(FieldType type) const override;
  void SetRawInfoWithVerificationStatus(FieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  // The class used to combine home phone parts into a whole number.
  class PhoneCombineHelper {
   public:
    PhoneCombineHelper();
    ~PhoneCombineHelper();

    // Processes the `value` accordingly given a phone number `field_type`.
    void SetInfo(FieldType field_type, const std::u16string& value);

    // Parses the number built up from pieces stored via SetInfo() according to
    // the specified |profile|'s country code, falling back to the given
    // |app_locale| if the |profile| has no associated country code.  Returns
    // true if parsing was successful, false otherwise.
    bool ParseNumber(const AutofillProfile& profile,
                     const std::string& app_locale,
                     std::u16string* value) const;

    // Returns true if both |phone_| and |whole_number_| are empty.
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
      const std::string& app_locale,
      AutofillProfile& profile);

 private:
  // FormGroup:
  void GetSupportedTypes(FieldTypeSet* supported_types) const override;
  std::u16string GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;
  bool SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                         const std::u16string& value,
                                         const std::string& app_locale,
                                         VerificationStatus status) override;

  // Updates the cached parsed number if the profile's region has changed
  // since the last time the cache was updated.
  void UpdateCacheIfNeeded(const std::string& app_locale) const;

  // The phone number.
  std::u16string number_;
  // Profile which stores the region used as hint when normalizing the number.
  raw_ptr<const AutofillProfile> profile_;

  // Cached number.
  mutable i18n::PhoneObject cached_parsed_phone_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_
