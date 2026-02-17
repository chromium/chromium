// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/security_interstitials/core/insecure_form_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormData& form) {
  return !client.IsContextSecure() ||
         (form.action().is_valid() && form.action().SchemeIs("http"));
}

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormStructure& form) {
  return !client.IsContextSecure() ||
         (form.target_url().is_valid() && form.target_url().SchemeIs("http"));
}

bool IsFormMixedContent(const AutofillClient& client, const FormData& form) {
  return client.IsContextSecure() && form.action().is_valid() &&
         security_interstitials::IsInsecureFormAction(form.action());
}

bool IsFormStructurePerfectlyFilled(const FormStructure& form) {
  return std::ranges::none_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->all_modifiers().contains(FieldModifier::kUser) &&
               field->last_modifier() != FieldModifier::kAutofill;
      });
}

}  // namespace autofill
