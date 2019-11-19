// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOBILE_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOBILE_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// on mobile platforms.
//
// In constructing a label, a MobileLabelFormatter may consider the following:
//
// 1. The type of form, e.g. a form with only name and phone fields or a form
// with name, address, phone, and email fields
// 2. The field on which the user is focused, e.g. a first name field
// 3. The non-focused form fields whose corresponding profile data is not the
// same across |profiles_|, if any
// 4. The relative usefulness of form field data that could be shown in labels
//
// The usefulness, from greatest to least, is as follows:
// address > phone > email > name
//
// There are two address categories: street address and non street address.
// Street address example: 44 Lakeview Ln
// Non street address examples: London, 02113, CA, Rio Comprido, and Germany
class MobileLabelFormatter : public LabelFormatter {
 public:
  MobileLabelFormatter(const std::vector<AutofillProfile*>& profiles,
                       const std::string& app_locale,
                       ServerFieldType focused_field_type,
                       uint32_t groups,
                       const std::vector<ServerFieldType>& field_types);

  ~MobileLabelFormatter() override;

  base::string16 GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const override;

 private:
  // Returns a label for the kAutofillUseMobileLabelDisambiguation feature when
  // the ShowOne variant is enabled.
  //
  // The label has at most one piece of data, e.g. a phone number. For address
  // data, note that a street address, e.g. 120 Oak Rd #2, and a non street
  // address, e.g. Palo Alto, CA 94303, are each considered one piece of data.
  //
  // It is possible for the label to be an empty string. For example, suppose
  // (A) a user has two profiles with slightly different names, e.g. Joe and
  // Joseph, (B) the profile with Joe as its first name lacks an email address
  // and (C) this user is interacting with a form that has first name, last
  // name, and email address fields. If this user clicks on the first name
  // field, then the suggestion with Joe has an empty string as its label.
  base::string16 GetLabelForShowOneVariant(const AutofillProfile& profile,
                                           FieldTypeGroup focused_group) const;

  // Returns a label for the kAutofillUseMobileLabelDisambiguation feature when
  // the ShowAll variant is enabled.
  //
  // The label may contain multiple pieces of data, e.g. a street address, a
  // phone number, and an email address. It contains only data whose values are
  // not the same across |profiles|.
  //
  // As explained in the comment for GetLabelForShowOneVariant, it is possible
  // for the label to be an empty string.
  base::string16 GetLabelForShowAllVariant(const AutofillProfile& profile,
                                           FieldTypeGroup focused_group) const;

  // Returns a label with the most useful piece of data according to the
  // ordering described in this class' description.
  //
  // It is possible for the label to be an empty string. This can happen when
  // |profile| is missing data corresponding to a field, e.g. a profile without
  // a phone number.
  base::string16 GetDefaultLabel(const AutofillProfile& profile,
                                 FieldTypeGroup focused_group) const;

  // Returns true if the label should be an address part, e.g. 4 Oak Rd or
  // Boston, MA 02116.
  bool ShowLabelAddress(FieldTypeGroup focused_group) const;

  // True if the field (A) appears in the form, (B) is not the focused field,
  // and (C) does not have the same data for all |profiles_|.
  //
  // If a field is focused, then its corresponding bool should be false because
  // the focused field's data is shown in a suggestion's value. Repeating this
  // data in a suggestion's label is not helpful to users.
  bool could_show_email_;
  bool could_show_name_;
  bool could_show_non_street_address_;
  bool could_show_phone_;
  bool could_show_street_address_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOBILE_LABEL_FORMATTER_H_
