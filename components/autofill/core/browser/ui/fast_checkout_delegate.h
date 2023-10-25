// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_DELEGATE_H_

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {
class AutofillManager;

// Delegate for in-browser Fast Checkout (FC) surface display and selection.
// Currently FC surface is eligible only for particular forms on click on
// an empty focusable text input field.
//
// It is supposed to be owned by the given `BrowserAutofillManager`, and
// interact with its `FastCheckoutClient`.
class FastCheckoutDelegate {
 public:
  virtual ~FastCheckoutDelegate() = default;

  // Checks whether FastCheckout is eligible for the given web form data. On
  // success triggers the corresponding surface and returns `true`.
  virtual bool TryToShowFastCheckout(
      const FormData& form,
      const FormFieldData& field,
      base::WeakPtr<AutofillManager> autofill_manager) = 0;

  // Returns whether the FC surface is supported to trigger on a particular form
  // and field. Only if this is true, the client will show the view.
  virtual bool IntendsToShowFastCheckout(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id,
      const autofill::FormData& form_data) const = 0;

  // Returns whether the FC surface is currently being shown.
  virtual bool IsShowingFastCheckoutUI() const = 0;

  // Hides the FC surface if one is shown.
  virtual void HideFastCheckout(bool allow_further_runs) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_FAST_CHECKOUT_DELEGATE_H_
