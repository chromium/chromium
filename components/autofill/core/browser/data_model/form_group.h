// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_

#include <string>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

enum class VerificationStatus;

class AutofillType;

// This class is an interface for collections of form fields, grouped by type.
class FormGroup {
 public:
  virtual ~FormGroup() = default;

  // Used to determine the type of a field based on the |text| that a user
  // enters into the field, interpreted in the given |app_locale| if
  // appropriate. The field types can then be reported back to the server.  This
  // method is additive on |matching_types|.
  virtual void GetMatchingTypes(const std::u16string& text,
                                const std::string& app_locale,
                                ServerFieldTypeSet* matching_types) const;

  // Returns a set of server field types for which this FormGroup has non-empty
  // data. This method is additive on |non_empty_types|.
  virtual void GetNonEmptyTypes(const std::string& app_locale,
                                ServerFieldTypeSet* non_empty_types) const;

  // Returns a set of server field types for which this FormGroup has non-empty
  // raw data. This method is additive on `non_empty_types`.
  virtual void GetNonEmptyRawTypes(ServerFieldTypeSet* non_empty_types) const;

  // Returns the string associated with |type|, without canonicalizing the
  // returned value. For user-visible strings, use GetInfo() instead.
  virtual std::u16string GetRawInfo(ServerFieldType type) const = 0;

  // Same as |GetRawInfo()|, but as an integer. This is only supported for types
  // that stores their data as an integer internally to avoid unnecessary
  // conversions.
  virtual int GetRawInfoAsInt(ServerFieldType type) const;

  // Sets this FormGroup object's data for |type| to |value|, without
  // canonicalizing the |value|.  For data that has not already been
  // canonicalized, use SetInfo() instead.
  // Accepts a verification status.
  virtual void SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                const std::u16string& value,
                                                VerificationStatus status) = 0;

  // Convenience wrapper to allow passing the |value| as an integer.
  virtual void SetRawInfoAsIntWithVerificationStatus(ServerFieldType type,
                                                     int value,
                                                     VerificationStatus status);

  // Convenience wrapper to allow passing the |status| as an integer.
  void SetRawInfoWithVerificationStatusInt(ServerFieldType type,
                                           const std::u16string& value,
                                           int status);

  // Convenience wrapper to add
  // |VerificationStatus::kNoStatus| to
  // |SetRawInfoWithVerificationStatus|.
  void SetRawInfo(ServerFieldType type, const std::u16string& value);

  // Same as |SetRawInfo()| without a verification status, but with an integer.
  void SetRawInfoAsInt(ServerFieldType type, int value);

  // Returns true iff the string associated with |type| is nonempty (without
  // canonicalizing its value).
  bool HasRawInfo(ServerFieldType type) const;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  std::u16string GetInfo(ServerFieldType type,
                         const std::string& app_locale) const;
  std::u16string GetInfo(const AutofillType& type,
                         const std::string& app_locale) const;

  // Returns the verification status associated with the type.
  // Returns kNoStatus if the type does not support a verification status.
  virtual VerificationStatus GetVerificationStatus(ServerFieldType type) const;
  VerificationStatus GetVerificationStatus(const AutofillType& type) const;

  // Convenience wrappers to retrieve the Verification status in integer
  // representation.
  int GetVerificationStatusInt(ServerFieldType type) const;
  int GetVerificationStatusInt(const AutofillType& type) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  bool SetInfo(ServerFieldType type,
               const std::u16string& value,
               const std::string& app_locale);
  bool SetInfo(const AutofillType& type,
               const std::u16string& value,
               const std::string& app_locale);

  // Same as |SetInfo| but supports a verification status.
  bool SetInfoWithVerificationStatus(ServerFieldType type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status);

  bool SetInfoWithVerificationStatus(const AutofillType& type,
                                     const std::u16string& value,
                                     const std::string& app_locale,
                                     const VerificationStatus status);

  // Returns true iff the string associated with |type| is nonempty.
  bool HasInfo(ServerFieldType type) const;
  bool HasInfo(const AutofillType& type) const;

 protected:
  // AutofillProfile needs to call into GetSupportedTypes() for objects of
  // non-AutofillProfile type, for which mere inheritance is insufficient.
  friend class AutofillProfile;

  // Returns a set of server field types for which this FormGroup can store
  // data. This method is additive on |supported_types|.
  virtual void GetSupportedTypes(ServerFieldTypeSet* supported_types) const = 0;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  virtual std::u16string GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  virtual bool SetInfoWithVerificationStatusImpl(
      const AutofillType& type,
      const std::u16string& value,
      const std::string& app_locale,
      const VerificationStatus status);

  // Used to retrieve the verification status of a value associated with |type|.
  virtual VerificationStatus GetVerificationStatusImpl(
      ServerFieldType type) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
