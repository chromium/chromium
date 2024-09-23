// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace autofill::payments {

// The interface for the consent dialog that appears before Chrome initiates a
// payments window flow. If the dialog is accepted, Chrome will trigger the
// pop-up to navigate to the specific flow that is ongoing.
class PaymentsWindowUserConsentDialog {
 public:
  virtual ~PaymentsWindowUserConsentDialog() = default;

  // Dismisses the view that is currently presented to the user.
  virtual void Dismiss() = 0;

  virtual base::WeakPtr<PaymentsWindowUserConsentDialog> GetWeakPtr() = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_H_
