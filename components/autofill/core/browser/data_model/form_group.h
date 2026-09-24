// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class LogBuffer;

// This class is an interface for collections of form fields, grouped by type.
class FormGroup {
 public:
  virtual ~FormGroup() = default;

  // Used to determine the type of a field based on the `text` that a user
  // enters into the field, interpreted in the given `app_locale` if
  // appropriate. The field types can then be reported back to the server.  This
  // method is additive on `matching_types`.
  virtual void GetMatchingTypes(std::u16string_view text,
                                std::string_view app_locale,
                                FieldTypeSet* matching_types) const;

  // Returns a set of server field types for which this FormGroup has non-empty
  // data. This method is additive on `non_empty_types`.
  virtual void GetNonEmptyTypes(std::string_view app_locale,
                                FieldTypeSet* non_empty_types) const;

  // Returns the string associated with `type`, without canonicalizing the
  // returned value. For user-visible strings, use GetInfo() instead.
  virtual std::u16string GetRawInfo(FieldType type) const = 0;

  // Sets this FormGroup object's data for `type` to `value`, without
  // canonicalizing the `value`.  For data that has not already been
  // canonicalized, use SetInfo() instead.
  // Accepts a verification status.
  virtual void SetRawInfoWithVerificationStatus(FieldType type,
                                                std::u16string_view value,
                                                VerificationStatus status) = 0;

  // Convenience wrapper to add `VerificationStatus::kNoStatus` to
  // `SetRawInfoWithVerificationStatus`.
  void SetRawInfo(FieldType type, std::u16string_view value);

  // Returns true iff the string associated with `type` is nonempty (without
  // canonicalizing its value).
  bool HasRawInfo(FieldType type) const;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given `app_locale` if appropriate.
  virtual std::u16string GetInfo(FieldType type,
                                 std::string_view app_locale) const = 0;

  // Returns the verification status associated with the type.
  // Returns kNoStatus if the type does not support a verification status.
  virtual VerificationStatus GetVerificationStatus(FieldType type) const = 0;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified `app_locale` prior to storing, if appropriate.
  bool SetInfo(FieldType type,
               std::u16string_view value,
               std::string_view app_locale);

  // Same as `SetInfo` but supports a verification status.
  virtual bool SetInfoWithVerificationStatus(
      FieldType type,
      std::u16string_view value,
      std::string_view app_locale,
      const VerificationStatus status) = 0;

  // Returns true iff the string associated with `type` is nonempty.
  bool HasInfo(FieldType type) const;

  // Returns the set of `FieldType`s for which `SetInfo()` and friends may be
  // called.
  virtual FieldTypeSet GetSupportedTypes() const = 0;
};

LogBuffer& operator<<(LogBuffer& buffer, const FormGroup& form_group);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
