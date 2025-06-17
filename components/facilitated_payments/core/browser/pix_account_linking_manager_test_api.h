// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"

namespace payments::facilitated {

class PixAccountLinkingManagerTestApi {
 public:
  explicit PixAccountLinkingManagerTestApi(PixAccountLinkingManager* manager)
      : manager_(CHECK_DEREF(manager)) {}
  PixAccountLinkingManagerTestApi(const PixAccountLinkingManagerTestApi&) =
      delete;
  PixAccountLinkingManagerTestApi& operator=(
      const PixAccountLinkingManagerTestApi&) = delete;
  ~PixAccountLinkingManagerTestApi() = default;

  // Calls the underlying PixAccountLinkingManager's private methods.
  void OnAccepted() { manager_->OnAccepted(); }
  void OnDeclined() { manager_->OnDeclined(); }
  void OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      bool is_eligible_for_pix_account_linking) {
    manager_->OnGetDetailsForCreatePaymentInstrumentResponseReceived(
        result, is_eligible_for_pix_account_linking);
  }

  // Getters for PixAccountLinkingManager's private members.
  std::optional<bool> is_eligible_for_pix_account_linking() {
    return manager_->is_eligible_for_pix_account_linking_;
  }

 private:
  const raw_ref<PixAccountLinkingManager> manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_
