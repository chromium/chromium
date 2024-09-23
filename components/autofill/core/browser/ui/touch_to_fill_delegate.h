// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_

namespace autofill {

class AutofillManager;
class FormStructure;

// An interface for interaction with the bottom sheet UI controller, which is
// `TouchToFillPaymentMethodController` on Android. The delegate will supply
// the data to show and will be notified of events by the controller.
class TouchToFillDelegate {
 public:
  virtual ~TouchToFillDelegate() = default;

  virtual AutofillManager* GetManager() = 0;

  virtual bool IntendsToShowTouchToFill(FormGlobalId form_id,
                                        FieldGlobalId field_id,
                                        const FormData& form) = 0;

  // Checks whether TTF is eligible for the given web form data and, if
  // successful, triggers the corresponding surface and returns |true|.
  virtual bool TryToShowTouchToFill(const FormData& form,
                                    const FormFieldData& field) = 0;

  // Returns whether the TTF surface is currently being shown.
  virtual bool IsShowingTouchToFill() = 0;

  // Hides the TTF surface if one is shown.
  virtual void HideTouchToFill() = 0;

  // Resets the delegate to its starting state (e.g. on navigation).
  virtual void Reset() = 0;

  virtual bool ShouldShowScanCreditCard() = 0;
  virtual void ScanCreditCard() = 0;
  virtual void OnCreditCardScanned(const CreditCard& card) = 0;
  virtual void ShowPaymentMethodSettings() = 0;
  virtual void CreditCardSuggestionSelected(std::string unique_id,
                                            bool is_virtual) = 0;
  // Called when an IBAN suggestion was selected.
  // An Iban::Guid is passed in case of a locally stored IBAN and an
  // Iban::InstrumentId for server IBANs.
  virtual void IbanSuggestionSelected(
      absl::variant<Iban::Guid, Iban::InstrumentId> backend_id) = 0;
  virtual void OnDismissed(bool dismissed_by_user) = 0;

  virtual void LogMetricsAfterSubmission(
      const FormStructure& submitted_form) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
