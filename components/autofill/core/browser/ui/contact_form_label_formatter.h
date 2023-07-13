// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CONTACT_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CONTACT_FORM_LABEL_FORMATTER_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// containing name and phone or email fields.
class ContactFormLabelFormatter : public LabelFormatter {
 public:
  ContactFormLabelFormatter(const std::vector<AutofillProfile*>& profiles,
                            const std::string& app_locale,
                            ServerFieldType focused_field_type,
                            uint32_t groups,
                            const ServerFieldTypeSet& field_types);

  ~ContactFormLabelFormatter() override;

  std::u16string GetLabelForProfile(
      const AutofillProfile& profile,
      FieldTypeGroup focused_group) const override;

 private:
  // Returns |profile|'s email address if |profile| has a valid email address
  // and if this formatter's associated form has an email field.
  std::u16string MaybeGetEmail(const AutofillProfile& profile) const;

  // Returns |profile|'s phone number if |profile| has a phone number and if
  // this formatter's associated form has a phone field.
  std::u16string MaybeGetPhone(const AutofillProfile& profile) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_CONTACT_FORM_LABEL_FORMATTER_H_
