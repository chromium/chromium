// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_

#include <string>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

enum class VerificationStatus;
class PossibleProfileValueSources;

class AutofillType;

// This class is an interface for collections of form fields, grouped by type.
class FormGroup {
 public:
  virtual ~FormGroup() = default;

  // Used to determine the type of a field based on the |text| that a user
  // enters into the field, interpreted in the given |app_locale| if
  // appropriate. The field types can then be reported back to the server.  This
  // method is additive on |matching_types|.
  virtual void GetMatchingTypesWithProfileSources(
      const std::u16string& text,
      const std::string& app_locale,
      FieldTypeSet* matching_types,
      PossibleProfileValueSources* profile_value_sources) const;

  // Returns a set of server field types for which this FormGroup has non-empty
  // data. This method is additive on |non_empty_types|.
  virtual void GetNonEmptyTypes(const std::string& app_locale,
                                FieldTypeSet* non_empty_types) const;

  // Returns the string associated with |type|, without canonicalizing the
  // returned value. For user-visible strings, use GetInfo() instead.
  virtual std::u16string GetRawInfo(FieldType type) const = 0;

  // Sets this FormGroup object's data for |type| to |value|, without
  // canonicalizing the |value|.  For data that has not already been
  // canonicalized, use SetInfo() instead.
  // Accepts a verification status.
  virtual void SetRawInfoWithVerificationStatus(FieldType type,
                                                const std::u16string& value,
                                                VerificationStatus status) = 0;

  // Convenience wrapper to allow passing the |status| as an integer.
  void SetRawInfoWithVerificationStatusInt(FieldType type,
                                           const std::u16string& value,
                                           int status);

  // Convenience wrapper to add
  // |VerificationStatus::kNoStatus| to
  // |SetRawInfoWithVerificationStatus|.
  void SetRawInfo(FieldType type, const std::u16string& value);

  // Returns true iff the string associated with |type| is nonempty (without
  // canonicalizing its value).
  bool HasRawInfo(FieldType type) const;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  std::u16string GetInfo(FieldType type, const std::string& app_locale) const;
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const;

  // Returns the verification status associated with the type.
  // Returns kNoStatus if the type does not support a verification status.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  virtual VerificationStatus GetVerificationStatus(FieldType type) const;
  VerificationStatus GetVerificationStatus(const AutofillType& type) const;

  // Convenience wrappers to retrieve the Verification status in integer
  // representation.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  int GetVerificationStatusInt(FieldType type) const;
  int GetVerificationStatusInt(const AutofillType& type) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  bool SetInfo(FieldType type,
               const std::u16string& value,
               const std::string& app_locale);
  bool SetInfo(const AutofillType& type,
               const std::u16string& value,
               const std::string& app_locale);

  // Same as |SetInfo| but supports a verification status.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  bool SetInfoWithVerificationStatus(FieldType type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status);

  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status);

  // Returns true iff the string associated with |type| is nonempty.
  // TODO(crbug.com/40264633): Remove the `AutofillType` version.
  bool HasInfo(FieldType type) const;
  bool HasInfo(const AutofillType& type) const;

 protected:
  // AutofillProfile needs to call into GetSupportedTypes() for objects of
  // non-AutofillProfile type, for which mere inheritance is insufficient.
  friend class AutofillProfile;

  // Returns a set of server field types for which this FormGroup can store
  // data. This method is additive on |supported_types|.
  virtual void GetSupportedTypes(FieldTypeSet* supported_types) const = 0;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  // TODO(crbug.com/40264633): Pass `FieldType` instead of `AutofillType`.
  virtual std::u16string GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  // TODO(crbug.com/40264633): Pass `FieldType` instead of `AutofillType`.
  virtual bool SetInfoWithVerificationStatusImpl(
      const AutofillType& type,
      const std::u16string& value,
      const std::string& app_locale,
      const VerificationStatus status);

  // Used to retrieve the verification status of a value associated with |type|.
  virtual VerificationStatus GetVerificationStatusImpl(FieldType type) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
