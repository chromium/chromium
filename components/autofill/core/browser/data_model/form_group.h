// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_

#include <string>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AutofillType;

// This class is an interface for collections of form fields, grouped by type.
class FormGroup {
 public:
  virtual ~FormGroup() {}

  // Used to determine the type of a field based on the |text| that a user
  // enters into the field, interpreted in the given |app_locale| if
  // appropriate. The field types can then be reported back to the server.  This
  // method is additive on |matching_types|.
  virtual void GetMatchingTypes(const base::string16& text,
                                const std::string& app_locale,
                                ServerFieldTypeSet* matching_types) const;

  // Returns a set of server field types for which this FormGroup has non-empty
  // data. This method is additive on |non_empty_types|.
  virtual void GetNonEmptyTypes(const std::string& app_locale,
                                ServerFieldTypeSet* non_empty_types) const;

  // Returns the string associated with |type|, without canonicalizing the
  // returned value. For user-visible strings, use GetInfo() instead.
  virtual base::string16 GetRawInfo(ServerFieldType type) const = 0;

  // Sets this FormGroup object's data for |type| to |value|, without
  // canonicalizing the |value|.  For data that has not already been
  // canonicalized, use SetInfo() instead.
  virtual void SetRawInfo(ServerFieldType type,
                          const base::string16& value) = 0;

  // Returns true iff the string associated with |type| is nonempty (without
  // canonicalizing its value).
  bool HasRawInfo(ServerFieldType type) const;

  // Returns the string that should be auto-filled into a text field given the
  // type of that field, localized to the given |app_locale| if appropriate.
  base::string16 GetInfo(ServerFieldType type,
                         const std::string& app_locale) const;
  base::string16 GetInfo(const AutofillType& type,
                         const std::string& app_locale) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  bool SetInfo(ServerFieldType type,
               const base::string16& value,
               const std::string& app_locale);
  bool SetInfo(const AutofillType& type,
               const base::string16& value,
               const std::string& app_locale);

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
  virtual base::string16 GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const;

  // Used to populate this FormGroup object with data. Canonicalizes the data
  // according to the specified |app_locale| prior to storing, if appropriate.
  virtual bool SetInfoImpl(const AutofillType& type,
                           const base::string16& value,
                           const std::string& app_locale);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_FORM_GROUP_H_
