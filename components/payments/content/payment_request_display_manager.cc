// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_display_manager.h"

#include "base/check.h"
#include "components/payments/content/content_payment_request_delegate.h"

namespace payments {

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

PaymentRequestDisplayManager::~PaymentRequestDisplayManager() {}

std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::TryShow(
    base::WeakPtr<ContentPaymentRequestDelegate> delegate) {
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle;
  if (!current_handle_ && delegate) {
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
