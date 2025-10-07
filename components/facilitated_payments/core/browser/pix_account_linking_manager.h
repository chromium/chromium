// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "url/origin.h"

namespace payments::facilitated {

class FacilitatedPaymentsClient;
// A cross-platform interface that manages the Pix account linking flow. It is
// owned by `FacilitatedPaymentsClient`. There is 1 instance of this class per
// tab. Its lifecycle is same as that of `FacilitatedPaymentsClient`.

// The Pix account linking prompt is shown after the user has paid on their bank
// app and returned to Chrome. Some merchants show the order status causing page
// navigations. To overcome such cases, the manager should be associated with
// the tab, and not a single frame.
class PixAccountLinkingManager {
 public:
  explicit PixAccountLinkingManager(FacilitatedPaymentsClient* client);
  virtual ~PixAccountLinkingManager();

  // Initialize the Pix account linking flow. Virtual so it can be overridden in
  // tests.
  virtual void MaybeShowPixAccountLinkingPrompt(
      const url::Origin& pix_payment_page_origin);

 private:
  friend class PixAccountLinkingManagerTestApi;

  void Reset();

  // Sets the UI event listener, sets the internal UI state, and triggers
  // showing the Pix account linking prompt if the user is eligible.
  void ShowPixAccountLinkingPromptIfEligible();

  // Shows the Pix account linking prompt to user after the predefined wait
  // time.
  void ShowPixAccountLinkingPromptAfterDelay();

  // Sets the internal UI state and triggers dismissal.
  void DismissPrompt();

  void OnAccepted();

  void OnDeclined();

  // Called by the view to communicate UI events.
  void OnUiScreenEvent(UiEvent ui_event_type);

  // Callback for when the payments request to check Pix account linking
  // eligibility is completed.
  void OnGetDetailsForCreatePaymentInstrumentResponseReceived(
      base::TimeTicks start_time,
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult result,
      bool is_eligible_for_pix_account_linking);

  // Owner.
  const raw_ref<FacilitatedPaymentsClient> client_;

  // Optional bool to indicate whether the user is eligible for Pix account
  // linking based on the response from payments backend. This field is set to
  // optional to be able to differentiate between the case where the server
  // response is not received yet.
  std::optional<bool> is_eligible_for_pix_account_linking_ = std::nullopt;

  // True when the Pix account linking prompt is being shown to the user. Used
  // to catch instances where the prompt is unexpectedly closed.
  bool is_prompt_showing_ = false;

  // The origin of the Pix payment page that triggered the account linking flow.
  url::Origin pix_payment_page_origin_;

  base::WeakPtrFactory<PixAccountLinkingManager> weak_ptr_factory_{this};
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_PIX_ACCOUNT_LINKING_MANAGER_H_
