// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include <algorithm>
#include <memory>

#include "base/check_deref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/security_interstitials/core/insecure_form_util.h"

namespace autofill {

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

bool ShouldRecordFillingHistory(FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kCreditCard:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kOneTimePassword:
      return true;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPasskey:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kAtMemory:
      return false;
  }
  NOTREACHED();
}

}  // namespace autofill
