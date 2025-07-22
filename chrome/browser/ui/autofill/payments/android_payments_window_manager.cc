// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"

#include "base/check_deref.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"
#include "components/autofill/core/browser/payments/payments_window_manager_util.h"
#include "url/gurl.h"

namespace autofill::payments {

AndroidPaymentsWindowManager::AndroidPaymentsWindowManager(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AndroidPaymentsWindowManager::~AndroidPaymentsWindowManager() = default;

void AndroidPaymentsWindowManager::InitBnplFlow(BnplContext context) {
  CHECK(!flow_state_.has_value());
  flow_state_ = FlowState();

  flow_state_->flow_type = FlowType::kBnpl;
  flow_state_->bnpl_context = std::move(context);
  CreateTab(flow_state_->bnpl_context->initial_url);
  autofill_metrics::LogBnplPopupWindowShown(
      flow_state_->bnpl_context->issuer_id);
}

void AndroidPaymentsWindowManager::InitVcn3dsAuthentication(
    Vcn3dsContext context) {
  NOTIMPLEMENTED();
}

void AndroidPaymentsWindowManager::WebContentsDestroyed() {
  CHECK(flow_state_.has_value());
  switch (flow_state_->flow_type) {
    case FlowType::kBnpl:
      TriggerCompletionCallbackAndLogMetricsForBnpl(
          std::move(flow_state_.value()));
      break;
    case FlowType::kVcn3ds:
    case FlowType::kNoFlow:
      NOTREACHED();
  }
  flow_state_.reset();
}

void AndroidPaymentsWindowManager::OnDidFinishNavigationForBnpl(
    const GURL& url) {
  CHECK(flow_state_.has_value());
  CHECK_EQ(flow_state_->flow_type, FlowType::kBnpl);

  flow_state_->most_recent_url_navigation = url;
  BnplPopupStatus status =
      ParseUrlForBnpl(flow_state_->most_recent_url_navigation,
                      flow_state_->bnpl_context.value());
  if (status != BnplPopupStatus::kNotFinished) {
    // TODO(crbug.com/430582871): Once PaymentsWindowManagerBridge::Close is
    // implemented, call it here.
    return;
  }
}

void AndroidPaymentsWindowManager::CreateTab(const GURL& url) {
  CHECK(flow_state_.has_value());

  // TODO((crbug.com/430582871): Once
  // PaymentsWindowManagerBridge::OpenInEphemeralTab is implemented, call it
  // here.

  switch (flow_state_->flow_type) {
    case FlowType::kBnpl:
      flow_state_->bnpl_popup_shown_timestamp = base::TimeTicks::Now();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace autofill::payments
