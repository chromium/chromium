// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_TEST_API_H_

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"

namespace autofill::payments {

class PaymentsWindowBridge;

class AndroidPaymentsWindowManagerTestApi {
 public:
  explicit AndroidPaymentsWindowManagerTestApi(
      AndroidPaymentsWindowManager* window_manager)
      : window_manager_(CHECK_DEREF(window_manager)) {}
  AndroidPaymentsWindowManagerTestApi(
      const AndroidPaymentsWindowManagerTestApi&) = delete;
  AndroidPaymentsWindowManagerTestApi& operator=(
      const AndroidPaymentsWindowManagerTestApi&) = delete;
  ~AndroidPaymentsWindowManagerTestApi() = default;

  bool NoOngoingFlow() { return !window_manager_->flow_state_.has_value(); }

  GURL GetMostRecentUrlNavigation() {
    CHECK(window_manager_->flow_state_);
    return window_manager_->flow_state_->most_recent_url_navigation;
  }

  const std::optional<PaymentsWindowManager::BnplContext>& GetBnplContext() {
    CHECK(window_manager_->flow_state_);
    return window_manager_->flow_state_->bnpl_context;
  }

  void SetPaymentsWindowBridge(
      std::unique_ptr<PaymentsWindowBridge> payments_window_bridge) {
    window_manager_->payments_window_bridge_ =
        std::move(payments_window_bridge);
  }

  PaymentsWindowBridge& GetPaymentsWindowBridge() {
    return *window_manager_->payments_window_bridge_;
  }

 private:
  const raw_ref<AndroidPaymentsWindowManager> window_manager_;
};

inline AndroidPaymentsWindowManagerTestApi test_api(
    AndroidPaymentsWindowManager& manager) {
  return AndroidPaymentsWindowManagerTestApi(&manager);
}

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_PAYMENTS_WINDOW_MANAGER_TEST_API_H_
