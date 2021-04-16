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
    ContentPaymentRequestDelegate* delegate)
    : display_manager_(display_manager), delegate_(delegate) {
  display_manager_->set_current_handle(this);
}

PaymentRequestDisplayManager::DisplayHandle::~DisplayHandle() {
  display_manager_->set_current_handle(nullptr);
}

void PaymentRequestDisplayManager::DisplayHandle::Show(
    base::WeakPtr<PaymentRequest> request) {
  DCHECK(request);
  DCHECK(delegate_);
  delegate_->ShowDialog(request);
}

void PaymentRequestDisplayManager::DisplayHandle::Retry() {
  DCHECK(delegate_);
  delegate_->RetryDialog();
}

void PaymentRequestDisplayManager::DisplayHandle::DisplayPaymentHandlerWindow(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  DCHECK(delegate_);
  delegate_->EmbedPaymentHandlerWindow(url, std::move(callback));
}

PaymentRequestDisplayManager::PaymentRequestDisplayManager()
    : current_handle_(nullptr) {}

PaymentRequestDisplayManager::~PaymentRequestDisplayManager() {}

std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::TryShow(ContentPaymentRequestDelegate* delegate) {
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle = nullptr;
  if (!current_handle_) {
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
