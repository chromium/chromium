// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BIRTHDATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BIRTHDATE_H_

#include <string>

#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill {

// A form group that stores birthdate information.
class Birthdate : public FormGroup {
 public:
  Birthdate() = default;
  Birthdate(const Birthdate& other) = default;
  Birthdate& operator=(const Birthdate& other) = default;

  friend bool operator==(const Birthdate& a, const Birthdate& b);
  friend bool operator!=(const Birthdate& a, const Birthdate& b) {
    return !(a == b);
  }

  // Convenience accessor to the day, month and 4 digit year components.
  static ServerFieldTypeSet GetRawComponents() {
    return {BIRTHDATE_DAY, BIRTHDATE_MONTH, BIRTHDATE_4_DIGIT_YEAR};
  }

  // FormGroup:
  std::u16string GetRawInfo(ServerFieldType type) const override;

  // All |GetRawComponents()| are stored as integers and directly accessible.
  int GetRawInfoAsInt(ServerFieldType type) const override;

  void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                        const std::u16string& value,
                                        VerificationStatus status) override;

  void SetRawInfoAsIntWithVerificationStatus(
      ServerFieldType type,
      int value,
      VerificationStatus status) override;

 private:
  // FormGroup:
  void GetSupportedTypes(ServerFieldTypeSet* supported_types) const override;

  // Zero represents an unset value.
  int day_ = 0;
  int month_ = 0;
  // 4 digits.
  int year_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_BIRTHDATE_H_
