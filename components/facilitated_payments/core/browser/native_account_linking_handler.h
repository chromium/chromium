// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/account_linking_params.h"
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

  // Called when the user accepts the account linking prompt.
  virtual void OnAccepted();

  // Called when the user declines the account linking prompt.
  virtual void OnDeclined();

  // Called if the user dismisses the prompt (e.g., swiping down or tapping the
  // scrim).
  virtual void OnDismissed();

  // Dismisses the prompt UI.
  virtual void DismissPrompt();

 protected:
  // Starts fetching the client token from GMSCore.
  void FetchClientToken();
  // Virtual hook for subclasses to provide specific prompt configuration data.
  // Return std::nullopt to prevent the generic prompt from showing (useful
  // for subclasses that manage their own UI flows entirely).
  virtual std::optional<AccountLinkingParams> CreateAccountLinkingParams() = 0;

  // Concrete helper to show the prompt. Fetches params and binds base
  // callbacks.
  void ShowAccountLinkingPrompt();
  // Virtual hook to handle subclass-specific timing/logic on token reception.
  virtual void DoOnClientTokenReceived(
      const std::vector<uint8_t>& client_token) = 0;

  // Virtual hook to handle subclass-specific logic when GDCPI response is
  // received.
  virtual void DoOnGetDetailsForCreatePaymentInstrumentResponse(
      bool is_eligible) = 0;

  // Virtual hooks for subclass-specific prompt acceptance and decline
  // side-effects.
  virtual void DoOnAccepted() {}
  virtual void DoOnDeclined() {}

  // Virtual hook to handle subclass-specific UI updates on completion.
  virtual void DoOnAccountLinkingResult(AccountLinkingResult result) = 0;

  // Virtual hook to provide the FOP-specific backend payload.
  virtual base::DictValue
  GetPayloadForGetDetailsForCreatePaymentInstrument() = 0;

  // Virtual hook to get the FOP-specific prefix/suffix for histogram names.
  virtual std::string_view GetHistogramSuffix() const = 0;

  // Initiates the GDCPI network call to check eligibility and/or retrieve
  // the action token using the client token.
  void InitiateAccountLinkingNetworkCall(
      const std::vector<uint8_t>& client_token);

  // Invokes the native GMSCore InstrumentManager (Bender screens).
  void InvokeInstrumentManager(CoreAccountInfo primary_account,
                               const std::vector<uint8_t>& action_token);


  // Non-virtual helper to handle standard linking completion logic. Calls the
  // DoOnAccountLinkingResult virtual method.
  void OnAccountLinkingResult(AccountLinkingResult result);

  FacilitatedPaymentsClient* client() { return &*client_; }

  // Instantiates/retrieves the FacilitatedPaymentsApiClient.
  FacilitatedPaymentsApiClient* GetApiClient();

  // Track if the prompt UI is showing. Subclasses are responsible for updating
  // this state when they show the prompt.
  bool is_prompt_showing_ = false;

 private:
  // Callback invoked when the client token is received.
  // Handles latency/validity checks and calls DoOnClientTokenReceived.
  void OnClientTokenReceived(base::TimeTicks start_time,
                             std::vector<uint8_t> client_token);

  // Callback for when the network request completes.
  void OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult rpc_result,
      bool is_eligible,
      const std::vector<uint8_t>& action_token);

  const raw_ref<FacilitatedPaymentsClient> client_;

  // Creator callback for the GMSCore API client.
  FacilitatedPaymentsApiClientCreator api_client_creator_;

  // The GMSCore API client.
  std::unique_ptr<FacilitatedPaymentsApiClient> api_client_;

  // Cached action token used for invoking the instrument manager GMSCore API.
  std::vector<uint8_t> action_token_;
  // Subclasses must provide a WeakPtr to the base class since a WeakPtrFactory
  // can only exist on the leaf descendant class.
  virtual base::WeakPtr<NativeAccountLinkingHandler> GetWeakPtr() = 0;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_NATIVE_ACCOUNT_LINKING_HANDLER_H_
