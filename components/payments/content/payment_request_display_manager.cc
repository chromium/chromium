// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_display_manager.h"

#include "base/check.h"
#include "components/payments/content/content_payment_request_delegate.h"

namespace payments {

class PaymentRequest;

PaymentRequestDisplayManager::DisplayHandle::DisplayHandle(
    PaymentRequestDisplayManager* display_manager,
    base::WeakPtr<ContentPaymentRequestDelegate> delegate)
    : display_manager_(display_manager), delegate_(delegate) {
  display_manager_->set_current_handle(this);
}

PaymentRequestDisplayManager::DisplayHandle::~DisplayHandle() {
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

PaymentRequestDisplayManager::PaymentRequestDisplayManager()
    : current_handle_(nullptr) {}

PaymentRequestDisplayManager::~PaymentRequestDisplayManager() {}

std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::TryShow(
    base::WeakPtr<ContentPaymentRequestDelegate> delegate) {
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle;
  if (!current_handle_ && delegate) {
    handle = std::make_unique<PaymentRequestDisplayManager::DisplayHandle>(
        this, delegate);
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

}  // namespace payments
