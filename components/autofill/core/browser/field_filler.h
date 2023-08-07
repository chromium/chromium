// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

class AddressNormalizer;
class AutofillField;

// Helper class to put user content in fields, to eventually send to the
// renderer.
class FieldFiller {
 public:
  FieldFiller(const std::string& app_locale,
              AddressNormalizer* address_normalizer);
  ~FieldFiller();

  // Based on |field.Type()|, returns value that is supposed to be filled in the
  // |field_data|.
  std::u16string GetValueForFilling(
      const AutofillField& field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      FormFieldData* field_data,
      const std::u16string& cvc,
      mojom::AutofillActionPersistence action_persistence,
      std::string* failure_to_fill);

  // Set |field_data|'s value to the right value in |profile_or_credit_card|.
  // Uses |field| to determine which field type should be filled, and
  // |app_locale_| as hint when filling exceptional cases like phone number
  // values. If |forced_fill_values| contains a string for the field to be
  // filled, this value will be used unconditionally.
  // If |action| indicates that the value will be used for the
  // autofill preview (aka. suggestion) state, the data to be filled may be
  // obfuscated.
  //
  // Returns |true| if the field has been filled, false otherwise. This is
  // independent of whether the field was filled or autofilled before. If
  // |failure_to_fill| is not null, errors are reported to that string.
  bool FillFormField(
      const AutofillField& field,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const std::map<FieldGlobalId, std::u16string>& forced_fill_values,
      FormFieldData* field_data,
      const std::u16string& cvc,
      mojom::AutofillActionPersistence action_persistence,
      std::string* failure_to_fill = nullptr);

  // Returns the phone number value for the given `field_data`. The returned
  // value might be `number`, or `city_and_number`, or could possibly be a
  // meaningful subset `number`, if that's appropriate for the field.
  static std::u16string GetPhoneNumberValueForInput(
      const FormFieldData& field_data,
      const std::u16string& number,
      const std::u16string& city_and_number);

  // Returns the index of the shortest entry in the given select field of which
  // |value| is a substring. Returns -1 if no such entry exists.
  static int FindShortestSubstringMatchInSelect(const std::u16string& value,
                                                bool ignore_whitespace,
                                                const FormFieldData* field);

 private:
  const std::string app_locale_;
  // Weak, should outlive this object. May be null.
  raw_ptr<AddressNormalizer> address_normalizer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLER_H_
