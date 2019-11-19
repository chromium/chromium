// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_EMAIL_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_EMAIL_FORM_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// with name, address, and email fields and without phone fields.
class AddressEmailFormLabelFormatter : public LabelFormatter {
 public:
  AddressEmailFormLabelFormatter(
      const std::vector<AutofillProfile*>& profiles,
      const std::string& app_locale,
      ServerFieldType focused_field_type,
      uint32_t groups,
      const std::vector<ServerFieldType>& field_types);

  ~AddressEmailFormLabelFormatter() override;

  base::string16 GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const override;

 private:
  // Returns a label to show the user when |focused_field_type_| is a type
  // other than a non-street-address field type. For example,
  // |focused_field_type_| could be last name, home street address, or email
  // address.
  base::string16 GetLabelForProfileOnFocusedNameEmailOrStreetAddress(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const;

  // True if this formatter's associated form has a street address field. A
  // form may have an address-related field, e.g. zip code, without having a
  // street address field. If a form does not include a street address field,
  // street addresses should not appear in labels.
  bool form_has_street_address_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_EMAIL_FORM_LABEL_FORMATTER_H_
