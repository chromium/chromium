// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"

#include <memory>

#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/ui/android/autofill/payments/payments_window_bridge.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/payments_window_manager_util.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace autofill::payments {

AndroidPaymentsWindowManager::AndroidPaymentsWindowManager(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {
  payments_window_bridge_ =
      std::make_unique<PaymentsWindowBridge>(/*payments_window_delegate=*/this);
}

AndroidPaymentsWindowManager::~AndroidPaymentsWindowManager() = default;

void AndroidPaymentsWindowManager::InitBnplFlow(BnplContext context) {
  CHECK(!flow_state_.has_value());
  flow_state_ = FlowState();

  flow_state_->flow_type = FlowType::kBnpl;
  flow_state_->bnpl_context = std::move(context);
  CreateTab(flow_state_->bnpl_context->initial_url,
            BnplIssuerIdToDisplayName(flow_state_->bnpl_context->issuer_id));
  autofill_metrics::LogBnplPopupWindowShown(
      flow_state_->bnpl_context->issuer_id);
}

void AndroidPaymentsWindowManager::InitVcn3dsAuthentication(
    Vcn3dsContext context) {
  NOTIMPLEMENTED();
}

void AndroidPaymentsWindowManager::OnWebContentsObservationStarted(
    content::WebContents& web_contents) {
  if (ContentAutofillClient* client =
          ContentAutofillClient::FromWebContents(&web_contents)) {
    if (payments::PaymentsAutofillClient* payments_client =
            client->GetPaymentsAutofillClient()) {
      payments_client->DisablePaymentsAutofill();
    }
  }
}

void AndroidPaymentsWindowManager::WebContentsDestroyed() {
  // If `flow_state_` is not present, then completion flow has already been
  // handled.
  if (!flow_state_.has_value()) {
    return;
  }

  // On Android, the PaymentsAutofillClient can only be a
  // ChromePaymentsAutofillClient.
  ChromePaymentsAutofillClient* payments_autofill_client =
      static_cast<ChromePaymentsAutofillClient*>(
          client_->GetPaymentsAutofillClient());

  switch (flow_state_->flow_type) {
    case FlowType::kBnpl:
      // This should only be reached if the user directly closed the ephemeral
      // tab before navigating to the success/failure URL.
      CHECK_EQ(BnplPopupStatus::kNotFinished,
               ParseUrlForBnpl(flow_state_->most_recent_url_navigation,
                               flow_state_->bnpl_context.value()));
      TriggerCompletionCallbackAndLogMetricsForBnpl(
          std::move(flow_state_.value()));

      // Clear any leftover state from the TouchToFillPaymentMethodController in
      // case there is a bottom sheet that is currently hidden but not fully
      // dismissed.
      if (payments_autofill_client &&
          payments_autofill_client->GetTouchToFillPaymentMethodController()) {
        payments_autofill_client->GetTouchToFillPaymentMethodController()
            ->OnDismissed(/*env=*/nullptr, /*dismissed_by_user=*/true,
                          /*should_reshow=*/true);
      }
      break;
    case FlowType::kVcn3ds:
    case FlowType::kNoFlow:
      NOTREACHED();
  }
  flow_state_.reset();
}

void AndroidPaymentsWindowManager::OnDidFinishNavigationForBnpl(
    const GURL& url) {
  // An extra navigation (e.g., a JS redirect) may trigger immediately after the
  // completion URL is reached but before the ephemeral tab fully closes. If the
  // flow state has already been reset, the tab is closing, and nothing needs to
  // be done here.
  if (!flow_state_.has_value()) {
    return;
  }

  CHECK_EQ(flow_state_->flow_type, FlowType::kBnpl);

  flow_state_->most_recent_url_navigation = url;
  BnplPopupStatus status =
      ParseUrlForBnpl(flow_state_->most_recent_url_navigation,
                      flow_state_->bnpl_context.value());
  if (status == BnplPopupStatus::kNotFinished) {
    return;
  }

  // Run the completion callback to add the subsequent UI, such as the progress
  // or error screen, to the bottom sheet queue. This ensures the next screen is
  // shown seamlessly after the current ephemeral tab is dismissed.
  TriggerCompletionCallbackAndLogMetricsForBnpl(std::move(flow_state_.value()));
  flow_state_.reset();

  payments_window_bridge_->CloseEphemeralTab();
}

void AndroidPaymentsWindowManager::CreateTab(const GURL& url,
                                             const std::u16string& title) {
  CHECK(flow_state_.has_value());
  payments_window_bridge_->OpenEphemeralTab(url, title,
                                            client_->GetWebContents());
  switch (flow_state_->flow_type) {
    case FlowType::kBnpl:
      flow_state_->bnpl_popup_shown_timestamp = base::TimeTicks::Now();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace autofill::payments
