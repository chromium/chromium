// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_TOUCH_TO_FILL_TOUCH_TO_FILL_AUTOFILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_TOUCH_TO_FILL_TOUCH_TO_FILL_AUTOFILL_DELEGATE_H_

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// An interface for interaction with the bottom sheet UI controller, which is
// `TouchToFillAutofillController` on Android. The delegate will supply the
// data to show and will be notified of events by the controller.
class TouchToFillAutofillDelegate {
 public:
  virtual ~TouchToFillAutofillDelegate() = default;

  // Checks whether the TTF Autofill surface is eligible for the given web form
  // data.
  virtual bool IntendsToShowTouchToFill(FormGlobalId form_id,
                                        FieldGlobalId field_id) = 0;

  // Attempts to show the TTF Autofill surface for the given web form data.
  virtual bool TryToShowTouchToFill(const FormData& form,
                                    const FormFieldData& field) = 0;

  // Returns whether the TTF Autofill surface is currently being shown.
  virtual bool IsShowingTouchToFill() = 0;

  // Hides the TTF Autofill surface if one is shown.
  virtual void HideTouchToFill() = 0;

  // Called when the TTF Autofill surface is shown.
  virtual void OnShow() = 0;

  // Called when the user acknowledges the TTF Autofill notice.
  virtual void OnNoticeAcknowledged() = 0;

  // Called when the TTF Autofill surface is dismissed.
  virtual void OnDismissed() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_TOUCH_TO_FILL_TOUCH_TO_FILL_AUTOFILL_DELEGATE_H_
