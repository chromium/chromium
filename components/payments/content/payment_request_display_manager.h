// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace payments {

class ContentPaymentRequestDelegate;
class PaymentRequest;

// The callback type for functions that need to signal back to the ServiceWorker
// when a window was/failed to open following an openWindow call. The parameter
// indicates whether the call was successful or not.
using PaymentHandlerOpenWindowCallback =
    base::OnceCallback<void(bool /* success */,
                            int /* render_process_id */,
                            int /* render_frame_id */)>;

// This KeyedService is responsible for displaying and hiding Payment Request
// UI. It ensures that only one Payment Request is showing per profile.
class PaymentRequestDisplayManager : public KeyedService {
 public:
  class DisplayHandle {
   public:
    DisplayHandle(PaymentRequestDisplayManager* display_manager,
                  ContentPaymentRequestDelegate* delegate);
    ~DisplayHandle();
    void Show(base::WeakPtr<PaymentRequest> request);
    void Retry();
    // Attempt to display |url| inside the Payment Request dialog and run
    // |callback| after navigation is completed, passing true/false to indicate
    // success/failure.
    void DisplayPaymentHandlerWindow(const GURL& url,
                                     PaymentHandlerOpenWindowCallback callback);

   private:
    PaymentRequestDisplayManager* display_manager_;
    ContentPaymentRequestDelegate* delegate_;
    DISALLOW_COPY_AND_ASSIGN(DisplayHandle);
  };

  PaymentRequestDisplayManager();
  ~PaymentRequestDisplayManager() override;

  // If no PaymentRequest is currently showing, returns a unique_ptr to a
  // display handle that can be used to display the PaymentRequest dialog. The
  // UI is considered open until the handle object is deleted. |callback| is
  // called with true if the window is finished opening successfully, false if
  // opening it failed.
  std::unique_ptr<DisplayHandle> TryShow(
      ContentPaymentRequestDelegate* delegate);
  void ShowPaymentHandlerWindow(const GURL& url,
                                PaymentHandlerOpenWindowCallback callback);

 private:
  void set_current_handle(DisplayHandle* handle) { current_handle_ = handle; }

  DisplayHandle* current_handle_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDisplayManager);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_
