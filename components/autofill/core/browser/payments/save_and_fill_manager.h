// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_

#include "base/memory/raw_ref.h"

namespace autofill::payments {

class PaymentsAutofillClient;

// Owned by PaymentsAutofillClient. There is one instance of this class per Web
// Contents. This class manages the flow for the Save and Fill dialog.
class SaveAndFillManager {
 public:
  explicit SaveAndFillManager(PaymentsAutofillClient* payments_autofill_client);
  SaveAndFillManager(const SaveAndFillManager& other) = delete;
  SaveAndFillManager& operator=(const SaveAndFillManager& other) = delete;
  ~SaveAndFillManager();

 private:
  // The associated payments autofill client.
  const raw_ref<PaymentsAutofillClient> payments_autofill_client_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
