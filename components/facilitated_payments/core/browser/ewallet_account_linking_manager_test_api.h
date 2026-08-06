// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_TEST_API_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_TEST_API_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

namespace payments::facilitated {

class EwalletAccountLinkingManagerTestApi {
 public:
  explicit EwalletAccountLinkingManagerTestApi(
      EwalletAccountLinkingManager* manager)
      : manager_(manager) {}

  void DoOnClientTokenReceived(const std::vector<uint8_t>& client_token) {
    manager_->DoOnClientTokenReceived(client_token);
  }

  void DoOnGetDetailsForCreatePaymentInstrumentResponse(bool is_eligible) {
    manager_->DoOnGetDetailsForCreatePaymentInstrumentResponse(is_eligible);
  }

  std::optional<AccountLinkingParams> CreateAccountLinkingParams() {
    return manager_->CreateAccountLinkingParams();
  }

  void DoOnAccountLinkingResult(AccountLinkingResult result) {
    manager_->DoOnAccountLinkingResult(result);
  }

  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() {
    return manager_->GetPayloadForGetDetailsForCreatePaymentInstrument();
  }

  std::string_view GetHistogramSuffix() const {
    return manager_->GetHistogramSuffix();
  }

  base::WeakPtr<NativeAccountLinkingHandler> GetWeakPtr() {
    return manager_->GetWeakPtr();
  }

 private:
  const raw_ptr<EwalletAccountLinkingManager> manager_;
};

inline EwalletAccountLinkingManagerTestApi test_api(
    EwalletAccountLinkingManager& manager) {
  return EwalletAccountLinkingManagerTestApi(&manager);
}

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_TEST_API_H_
