// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

class BrowserAutofillManager;

// Delegate for in-browser Touch To Fill (TTF) surface display and selection.
// Currently TTF surface is eligible only for credit card forms.
//
// It is supposed to be owned by the given |BrowserAutofillManager|, and
// interact with it and its |AutofillClient|.
// TODO(crbug.com/1324900): Consider using more descriptive name.
class TouchToFillDelegateImpl : public TouchToFillDelegate {
 public:
  explicit TouchToFillDelegateImpl(BrowserAutofillManager* manager);
  TouchToFillDelegateImpl(const TouchToFillDelegateImpl&) = delete;
  TouchToFillDelegateImpl& operator=(const TouchToFillDelegateImpl&) = delete;
  ~TouchToFillDelegateImpl() override;

  // Checks whether TTF is eligible for the given web form data. On success
  // triggers the corresponding surface and returns |true|.
  virtual bool TryToShowTouchToFill(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field);

  // Hides the TTF surface if one is shown.
  virtual void HideTouchToFill();

 private:
  base::WeakPtr<TouchToFillDelegateImpl> GetWeakPtr();

  const raw_ptr<BrowserAutofillManager> manager_;

  base::WeakPtrFactory<TouchToFillDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_IMPL_H_
