// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"

namespace autofill {

class AutofillProfile;

// A form group that stores phone number information.
class PhoneNumber : public FormGroup {
 public:
  explicit PhoneNumber(const AutofillProfile* profile);
  PhoneNumber(const PhoneNumber& number);
  ~PhoneNumber() override;

  PhoneNumber& operator=(const PhoneNumber& number);
  bool operator==(const PhoneNumber& other) const;
  bool operator!=(const PhoneNumber& other) const { return !operator==(other); }

  void set_profile(const AutofillProfile* profile) { profile_ = profile; }

  // FormGroup implementation:
  void GetMatchingTypes(const base::string16& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;
  base::string16 GetRawInfo(ServerFieldType type) const override;
  void SetRawInfo(ServerFieldType type, const base::string16& value) override;

  // Size and offset of the prefix and suffix portions of phone numbers.
  static const size_t kPrefixOffset = 0;
  static const size_t kPrefixLength = 3;
  static const size_t kSuffixOffset = 3;
  static const size_t kSuffixLength = 4;

  // The class used to combine home phone parts into a whole number.
  class PhoneCombineHelper {
   public:
    PhoneCombineHelper();
    ~PhoneCombineHelper();

    // If |type| is a phone field type, saves the |value| accordingly and
    // returns true.  For all other field types returs false.
    bool SetInfo(const AutofillType& type, const base::string16& value);

    // Parses the number built up from pieces stored via SetInfo() according to
    // the specified |profile|'s country code, falling back to the given
    // |app_locale| if the |profile| has no associated country code.  Returns
    // true if parsing was successful, false otherwise.
    bool ParseNumber(const AutofillProfile& profile,
                     const std::string& app_locale,
                     base::string16* value);

    // Returns true if both |phone_| and |whole_number_| are empty.
    bool IsEmpty() const;

   private:
    base::string16 country_;
    base::string16 city_;
    base::string16 phone_;
    base::string16 whole_number_;
  };

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
  base::string16 GetInfoImpl(const AutofillType& type,
                             const std::string& app_locale) const override;
  bool SetInfoImpl(const AutofillType& type,
                   const base::string16& value,
                   const std::string& app_locale) override;

  // Updates the cached parsed number if the profile's region has changed
  // since the last time the cache was updated.
  void UpdateCacheIfNeeded(const std::string& app_locale) const;

  // The phone number.
  base::string16 number_;
  // Profile which stores the region used as hint when normalizing the number.
  const AutofillProfile* profile_;  // WEAK

  // Cached number.
  mutable i18n::PhoneObject cached_parsed_phone_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PHONE_NUMBER_H_
