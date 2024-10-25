// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"

namespace payments::facilitated {

class EwalletManagerTestApi {
 public:
  explicit EwalletManagerTestApi(EwalletManager* ewallet_manager)
      : ewallet_manager_(CHECK_DEREF(ewallet_manager)) {}
  EwalletManagerTestApi(const EwalletManagerTestApi&) = delete;
  EwalletManagerTestApi& operator=(const EwalletManagerTestApi&) = delete;
  ~EwalletManagerTestApi() = default;

  FacilitatedPaymentsApiClient* GetApiClient() {
    return ewallet_manager_->GetApiClient();
  }

  void OnApiAvailabilityReceived(bool is_api_available) {
    ewallet_manager_->OnApiAvailabilityReceived(is_api_available);
  }

 private:
  const raw_ref<EwalletManager> ewallet_manager_;
};

inline EwalletManagerTestApi test_api(EwalletManager& manager) {
  return EwalletManagerTestApi(&manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_MANAGER_TEST_API_H_
