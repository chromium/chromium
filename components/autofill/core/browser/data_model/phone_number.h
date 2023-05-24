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
  void GetMatchingTypes(const std::u16string& text,
                        const std::string& app_locale,
                        ServerFieldTypeSet* matching_types) const override;
  std::u16string GetRawInfo(ServerFieldType type) const override;
  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  // The class used to combine home phone parts into a whole number.
  class PhoneCombineHelper {
   public:
    PhoneCombineHelper();
    ~PhoneCombineHelper();

    // If |type| is a phone field type, saves the |value| accordingly and
    // returns true.  For all other field types returns false.
    bool SetInfo(const AutofillType& type, const std::u16string& value);

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

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;
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
