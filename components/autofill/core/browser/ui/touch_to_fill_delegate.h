// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card.h"

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_

namespace autofill {

class AutofillDriver;

// An interface for interaction with the bottom sheet UI controller, which is
// `TouchToFillCreditCardController` on Android. The delegate will supply the
// data to show and will be notified of events by the controller.
class TouchToFillDelegate {
 public:
  virtual ~TouchToFillDelegate() = default;

  // TODO(crbug.com/1247698): Define the API.
  virtual AutofillDriver* GetDriver() = 0;

  virtual bool ShouldShowScanCreditCard() = 0;
  virtual void ScanCreditCard() = 0;
  virtual void OnCreditCardScanned(const CreditCard& card) = 0;
  virtual void ShowCreditCardSettings() = 0;
  virtual void SuggestionSelected(std::string unique_id) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
