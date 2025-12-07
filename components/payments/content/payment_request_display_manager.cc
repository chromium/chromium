// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_display_manager.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/payments/content/content_payment_request_delegate.h"

namespace payments {

namespace {
// Helper for PaymentRequestDisplayManager::TryShow, to determine the outcome
// given a request to show a `new_delegate` and the `current_handle` for any
// PaymentRequest that is currently showing. If no PaymentRequest is currently
// showing, `current_handle` is nullptr.
PaymentRequestTryShowOutcome GetTryShowOutcome(
    base::WeakPtr<ContentPaymentRequestDelegate> new_delegate,
    base::WeakPtr<PaymentRequestDisplayManager::DisplayHandle> current_handle) {
  if (!new_delegate) {
    return PaymentRequestTryShowOutcome::kCannotShowDelegateWasNull;
  }

  if (!current_handle) {
    return PaymentRequestTryShowOutcome::kAbleToShow;
  }

  // At this point it is not possible to show a new PaymentRequest as there is
  // an existing handle, however the function returns various outcomes for
  // metric tracking.

  base::WeakPtr<ContentPaymentRequestDelegate> current_delegate =
      current_handle->delegate();
  if (!current_delegate || !current_delegate->GetRenderFrameHost() ||
      !new_delegate->GetRenderFrameHost()) {
    // It is possible for the current delegate or either of the RenderFrameHosts
    // to have become null before the current_handle is cleared (for example, if
    // one of the pages is in the middle of navigating away). Such scenarios
    // should be rare, so they are collected under a single outcome.
    return PaymentRequestTryShowOutcome::kCannotShowUnknownReason;
  }

  content::RenderFrameHost* current_main_frame =
      current_delegate->GetRenderFrameHost()->GetMainFrame();
  content::RenderFrameHost* new_main_frame =
      new_delegate->GetRenderFrameHost()->GetMainFrame();
  return (current_main_frame == new_main_frame)
             ? PaymentRequestTryShowOutcome::
                   kCannotShowExistingPaymentRequestSameTab
             : PaymentRequestTryShowOutcome::
                   kCannotShowExistingPaymentRequestDifferentTab;
}

}  // namespace

class PaymentRequest;

PaymentRequestDisplayManager::DisplayHandle::DisplayHandle(
    base::WeakPtr<PaymentRequestDisplayManager> display_manager,
    base::WeakPtr<ContentPaymentRequestDelegate> delegate)
    : display_manager_(display_manager), delegate_(delegate) {
  if (display_manager_)
    display_manager_->set_current_handle(GetWeakPtr());
}

PaymentRequestDisplayManager::DisplayHandle::~DisplayHandle() {
  if (display_manager_)
    display_manager_->set_current_handle(nullptr);
}

void PaymentRequestDisplayManager::DisplayHandle::Show(
    base::WeakPtr<PaymentRequest> request) {
  DCHECK(request);
  was_shown_ = true;
  if (delegate_)
    delegate_->ShowDialog(request);
}

void PaymentRequestDisplayManager::DisplayHandle::Retry() {
  if (delegate_)
    delegate_->RetryDialog();
}

void PaymentRequestDisplayManager::DisplayHandle::DisplayPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  if (delegate_)
    delegate_->EmbedPaymentHandlerWindow(url, std::move(callback));
}

base::WeakPtr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::DisplayHandle::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PaymentRequestDisplayManager::PaymentRequestDisplayManager()
    : current_handle_(nullptr) {}

PaymentRequestDisplayManager::~PaymentRequestDisplayManager() = default;

std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::TryShow(
    base::WeakPtr<ContentPaymentRequestDelegate> delegate) {
  // Measure PaymentRequest's ability to show to determine the impact of
  // one-PaymentRequest-per-profile; see crbug.com/41427529
  PaymentRequestTryShowOutcome outcome =
      GetTryShowOutcome(delegate, current_handle_);
  base::UmaHistogramEnumeration("PaymentRequest.Show.TryShowOutcome", outcome);

  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle;
  if (outcome == PaymentRequestTryShowOutcome::kAbleToShow) {
    handle = std::make_unique<PaymentRequestDisplayManager::DisplayHandle>(
        GetWeakPtr(), delegate);
  }

  return handle;
}

void PaymentRequestDisplayManager::ShowPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  if (current_handle_) {
    current_handle_->DisplayPaymentHandlerWindow(url, std::move(callback));
  } else {
    std::move(callback).Run(false, 0, 0);
  }
}

base::WeakPtr<PaymentRequestDisplayManager>
PaymentRequestDisplayManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
