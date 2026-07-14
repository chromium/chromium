// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/facilitated_payments/core/browser/account_linking_result.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/native_account_linking_handler.h"

namespace payments::facilitated {

// TODO: b/520063014 - Add strike database logic when
// NativeAccountLinkingHandler supports it.
class EwalletAccountLinkingManager : public NativeAccountLinkingHandler {
 public:
  EwalletAccountLinkingManager(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator);
  ~EwalletAccountLinkingManager() override;

  EwalletAccountLinkingManager(const EwalletAccountLinkingManager&) = delete;
  EwalletAccountLinkingManager& operator=(const EwalletAccountLinkingManager&) =
      delete;

 private:
  friend class EwalletAccountLinkingManagerTestApi;

 protected:
  // NativeAccountLinkingHandler:
  void DoOnClientTokenReceived(
      const std::vector<uint8_t>& client_token) override;
  void DoOnAccountLinkingResult(AccountLinkingResult result) override;
  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() override;
  std::string_view GetHistogramSuffix() const override;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_
