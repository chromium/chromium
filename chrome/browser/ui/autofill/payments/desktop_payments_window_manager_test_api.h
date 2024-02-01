// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"

namespace autofill::payments {

class DesktopPaymentsWindowManagerTestApi {
 public:
  explicit DesktopPaymentsWindowManagerTestApi(
      DesktopPaymentsWindowManager* window_manager)
      : window_manager_(CHECK_DEREF(window_manager)) {}
  DesktopPaymentsWindowManagerTestApi(
      const DesktopPaymentsWindowManagerTestApi&) = delete;
  DesktopPaymentsWindowManagerTestApi& operator=(
      const DesktopPaymentsWindowManagerTestApi&) = delete;
  ~DesktopPaymentsWindowManagerTestApi() = default;

  void CreatePopup(const GURL& url) { window_manager_->CreatePopup(url); }

  std::optional<PaymentsWindowManager::Vcn3dsContext> GetVcn3dsContext() {
    return window_manager_->vcn_3ds_context_;
  }

 private:
  const raw_ref<DesktopPaymentsWindowManager> window_manager_;
};

inline DesktopPaymentsWindowManagerTestApi test_api(
    DesktopPaymentsWindowManager& manager) {
  return DesktopPaymentsWindowManagerTestApi(&manager);
}

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_TEST_API_H_
