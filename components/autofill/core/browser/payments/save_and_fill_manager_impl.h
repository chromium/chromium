// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"

namespace autofill::payments {

class PaymentsAutofillClient;

// Owned by PaymentsAutofillClient. There is one instance of this class per Web
// Contents. This class manages the flow for the Save and Fill dialog.
class SaveAndFillManagerImpl : public SaveAndFillManager {
 public:
  explicit SaveAndFillManagerImpl(
      PaymentsAutofillClient* payments_autofill_client);
  SaveAndFillManagerImpl(const SaveAndFillManagerImpl& other) = delete;
  SaveAndFillManagerImpl& operator=(const SaveAndFillManagerImpl& other) =
      delete;
  ~SaveAndFillManagerImpl() override;

  void OnDidAcceptCreditCardSaveAndFillSuggestion() override;

 private:
  const raw_ref<PaymentsAutofillClient> payments_autofill_client_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_IMPL_H_
