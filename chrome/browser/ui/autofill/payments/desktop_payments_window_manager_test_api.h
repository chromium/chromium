// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_DESKTOP_PAYMENTS_WINDOW_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "ui/gfx/geometry/rect.h"

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

  void CreatePopup(const GURL& url, gfx::Rect popup_size) {
    window_manager_->CreatePopup(url, std::move(popup_size));
  }

  void OnVcn3dsAuthenticationResponseReceived(
      PaymentsAutofillClient::PaymentsRpcResult result,
      const UnmaskResponseDetails& response_details) {
    window_manager_->OnVcn3dsAuthenticationResponseReceived(result,
                                                            response_details);
  }

  void OnVcn3dsAuthenticationProgressDialogCancelled() {
    window_manager_->OnVcn3dsAuthenticationProgressDialogCancelled();
  }

  const std::optional<PaymentsWindowManager::Vcn3dsContext>&
  GetVcn3dsContext() {
    CHECK(window_manager_->flow_state_);
    return window_manager_->flow_state_->vcn_3ds_context;
  }

  const std::optional<DesktopPaymentsWindowManager::FlowState>& GetFlowState() {
    return window_manager_->flow_state_;
  }

  void SetPopupClosedClosure(base::RepeatingClosure popup_closed_closure) {
    window_manager_->popup_closed_closure_for_testing_ = popup_closed_closure;
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
