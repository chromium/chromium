// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
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
      FacilitatedPaymentsApiClientCreator api_client_creator,
      const autofill::Ewallet& ewallet_creation_option);
  ~EwalletAccountLinkingManager() override;

  // Gracefully handles UI teardown and cancels any pending async network
  // callbacks (e.g., GetDetailsForCreatePaymentInstrument) to prevent crashes.
  // Must be explicitly called before destroying the manager or restarting
  // a new account linking flow to ensure any active UI prompts are dismissed.
  void DismissAndCancel();

  // Kicks off the asynchronous account linking network flow.
  void TriggerAccountLinking();

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
  std::optional<AccountLinkingParams> CreateAccountLinkingParams() override;
  void DoOnGetDetailsForCreatePaymentInstrumentResponse(
      bool is_eligible) override;
  base::DictValue GetPayloadForGetDetailsForCreatePaymentInstrument() override;
  std::string_view GetHistogramSuffix() const override;
  base::WeakPtr<NativeAccountLinkingHandler> GetWeakPtr() override;

 private:
  const autofill::Ewallet ewallet_creation_option_;

  base::WeakPtrFactory<EwalletAccountLinkingManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_EWALLET_ACCOUNT_LINKING_MANAGER_H_
