// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Handles the creation of Suggestions' disambiguating labels.
class LabelFormatter {
 public:
  LabelFormatter(const std::vector<AutofillProfile*>& profiles,
                 const std::string& app_locale,
                 ServerFieldType focused_field_type,
                 uint32_t groups,
                 const std::vector<ServerFieldType>& field_types);
  virtual ~LabelFormatter();

  // Returns the bitmask indicating which FieldTypeGroups are represented in
  // this formatter's associated form.
  uint32_t groups() const { return groups_; }

  // Returns a collection of labels formed by extracting useful disambiguating
  // information from |profiles_|.
  std::vector<base::string16> GetLabels() const;

  // Creates a form-specific LabelFormatter according to |field_types|. This
  // formatter has the ability to build labels with disambiguating information
  // from the given |profiles|.
  static std::unique_ptr<LabelFormatter> Create(
      const std::vector<AutofillProfile*>& profiles,
      const std::string& app_locale,
      ServerFieldType focused_field_type,
      const std::vector<ServerFieldType>& field_types);

 protected:
  // Returns a label to show the user. The elements of the label and their
  // ordering depend on the kind of LabelFormatter, the data in |profile|,
  // |focused_group|, and |focused_field_type_|.
  virtual base::string16 GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const = 0;

  // Returns the FieldTypeGroup with which |focused_field_type_| is associated.
  // Billing field types are mapped to their corresponding home address field
  // types. For example, if focused_field_type_ is ADDRESS_BILLING_ZIP, then
  // the resulting FieldTypeGroup is ADDRESS_HOME instead of ADDRESS_BILLING.
  FieldTypeGroup GetFocusedNonBillingGroup() const;

  const std::string& app_locale() const { return app_locale_; }

  ServerFieldType focused_field_type() const { return focused_field_type_; }

  const std::vector<ServerFieldType>& field_types_for_labels() const {
    return field_types_for_labels_;
  }

 private:
  // The collection of profiles for which to build labels. Storing this
  // collection ensures that the profiles for which this formatter is created
  // are the profiles for which the labels are constructed.
  //
  // It is safe to store a reference here because the LabelFormatter is
  // destroyed when the suggestions for which the labels are constructed are
  // returned.
  const std::vector<AutofillProfile*>& profiles_;

  // The locale for which to generate labels. This reflects the language and
  // country for which the application is translated, e.g. en-AU for Australian
  // English.
  std::string app_locale_;

  // The type of field on which the user is focused, e.g. NAME_FIRST.
  ServerFieldType focused_field_type_;

  // The bitmask indicating which FieldTypeGroups are represented in this
  // formatter's associated form.
  uint32_t groups_;

  // The collection of field types that can be used to make labels. It includes
  // only types related to names, addresses, email addresses, and phone
  // numbers. It excludes types related to countries.
  std::vector<ServerFieldType> field_types_for_labels_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_LABEL_FORMATTER_H_
