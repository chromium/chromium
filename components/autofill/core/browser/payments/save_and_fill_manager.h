// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_

namespace autofill::payments {

// Interface for managing the Save and Fill dialog flow.
class SaveAndFillManager {
 public:
  SaveAndFillManager() = default;
  SaveAndFillManager(const SaveAndFillManager& other) = delete;
  SaveAndFillManager& operator=(const SaveAndFillManager& other) = delete;
  virtual ~SaveAndFillManager() = default;

  virtual void OnDidAcceptCreditCardSaveAndFillSuggestion() = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SAVE_AND_FILL_MANAGER_H_
