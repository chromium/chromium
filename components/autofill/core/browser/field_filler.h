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
      const FormFieldData& field_data,
      const std::u16string& cvc,
      mojom::ActionPersistence action_persistence,
      std::string* failure_to_fill);

 private:
  const std::string app_locale_;
  // Weak, should outlive this object. May be null.
  raw_ptr<AddressNormalizer> address_normalizer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLER_H_
