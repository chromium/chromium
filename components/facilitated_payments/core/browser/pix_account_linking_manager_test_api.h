// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_

#include <vector>

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
  void DismissPrompt() { manager_->DismissPrompt(); }
  void OnAccepted() { manager_->OnAccepted(); }
  void OnDeclined() { manager_->OnDeclined(); }
  void OnUiScreenEvent(UiEvent ui_event_type) {
    manager_->OnUiScreenEvent(ui_event_type);
  }

  // Network and backend response handlers.
  void DoOnClientTokenReceived(const std::vector<uint8_t>& client_token) {
    manager_->DoOnClientTokenReceived(client_token);
  }
  void DoOnAccountLinkingResult(AccountLinkingResult result) {
    manager_->DoOnAccountLinkingResult(result);
  }
  void OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      bool is_eligible_for_pix_account_linking,
      const std::vector<uint8_t>& action_token) {
    manager_->OnGetDetailsForCreatePaymentInstrumentResponseReceived(
        start_time, result, is_eligible_for_pix_account_linking, action_token);
  }
  void Reset() { manager_->Reset(); }

  // Getters and inspection accessors.
  std::string_view GetHistogramSuffix() const {
    return manager_->GetHistogramSuffix();
  }
  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() {
    return manager_->GetPayloadForGetDetailsForCreatePaymentInstrument();
  }
  const std::vector<uint8_t>& client_token() const {
    return manager_->client_token_;
  }

 private:
  const raw_ref<PixAccountLinkingManager> manager_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_TEST_API_H_
