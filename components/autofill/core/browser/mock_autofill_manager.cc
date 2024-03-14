// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/mock_autofill_manager.h"

namespace autofill {

MockAutofillManager::MockAutofillManager(AutofillDriver* driver)
    : AutofillManager(driver) {}

MockAutofillManager::~MockAutofillManager() = default;

base::WeakPtr<AutofillManager> MockAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
