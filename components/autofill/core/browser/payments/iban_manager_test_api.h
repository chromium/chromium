// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_TEST_API_H_

#include "components/autofill/core/browser/payments/iban_manager.h"

namespace autofill {

class IbanManagerTestApi {
 public:
  explicit IbanManagerTestApi(IbanManager& iban_manager)
      : iban_manager_(iban_manager) {}

  void set_most_recent_suggestions_shown_field_global_id(
      FieldGlobalId field_global_id) {
    iban_manager_->uma_recorder_
        .most_recent_suggestions_shown_field_global_id_ = field_global_id;
  }

 private:
  const raw_ref<IbanManager> iban_manager_;
};

inline IbanManagerTestApi test_api(IbanManager& iban_manager) {
  return IbanManagerTestApi(iban_manager);
}

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_MANAGER_TEST_API_H_
