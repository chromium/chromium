// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"

namespace payments::facilitated {

class FacilitatedPaymentsClient;

// An abstract base class to coordinate the native account linking workflows
// for Pix and eWallets. It deduplicates interactions with GMSCore and the
// Payments backend.
class NativeAccountLinkingHandler {
 public:
  NativeAccountLinkingHandler(
      FacilitatedPaymentsClient* client,
      FacilitatedPaymentsApiClientCreator api_client_creator);
  virtual ~NativeAccountLinkingHandler();

  NativeAccountLinkingHandler(const NativeAccountLinkingHandler&) = delete;
  NativeAccountLinkingHandler& operator=(const NativeAccountLinkingHandler&) =
      delete;

  // Starts fetching the client token from GMSCore.
  void FetchClientToken();

  // Non-virtual callback invoked when the client token is received.
  // Handles latency/validity checks and calls DoOnClientTokenReceived.
  void OnClientTokenReceived(base::TimeTicks start_time,
                             std::vector<uint8_t> client_token);

  // Non-virtual helper to handle standard linking completion logic. Calls the
  // DoOnAccountLinkingResult virtual method.
  void OnAccountLinkingResult(bool result);

 protected:
  // Virtual hook to handle subclass-specific timing/logic on token reception.
  virtual void DoOnClientTokenReceived(
      const std::vector<uint8_t>& client_token) = 0;

  // Virtual hook to handle subclass-specific UI updates on completion.
  virtual void DoOnAccountLinkingResult(bool result) = 0;

  // Virtual hook to provide the FOP-specific backend payload.
  virtual base::DictValue
  GetPayloadForGetDetailsForCreatePaymentInstrument() = 0;

  // Virtual hook to get the FOP-specific prefix/suffix for histogram names.
  virtual std::string_view GetHistogramSuffix() const = 0;

  FacilitatedPaymentsClient* client() { return &*client_; }

  // Instantiates/retrieves the FacilitatedPaymentsApiClient.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Initiates the GDCPI network call to check eligibility and/or retrieve
  // the action token using the client token.
  void InitiateAccountLinkingNetworkCall(
      const std::vector<uint8_t>& client_token);

  // Invokes the native GMSCore InstrumentManager (Bender screens).
  void InvokeInstrumentManager(const std::vector<uint8_t>& action_token);

  // Dismisses the prompt UI.
  void DismissPrompt();

  // Track if the prompt UI is showing. Subclasses are responsible for updating
  // this state when they show the prompt.
  bool is_prompt_showing_ = false;

 private:
  // Callback for when the network request completes.
  // TODO(crbug.com/417330610): Add action_token parameter to this callback once
  // the callback signature in FacilitatedPaymentsNetworkInterface is updated to
  // return action_token.
  void OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult rpc_result,
      bool is_eligible);

  const raw_ref<FacilitatedPaymentsClient> client_;

  // Creator callback for the GMSCore API client.
  FacilitatedPaymentsApiClientCreator api_client_creator_;

  // The GMSCore API client.
  std::unique_ptr<FacilitatedPaymentsApiClient> api_client_;

  base::WeakPtrFactory<NativeAccountLinkingHandler> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_
