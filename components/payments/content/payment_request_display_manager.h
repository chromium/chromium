// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_frame_host.h"
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

// Enum of possible outcomes from a call to
// PaymentRequestDisplayManager::TryShow, used for logging purposes.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PaymentRequestTryShowOutcome)
enum class PaymentRequestTryShowOutcome {
  kAbleToShow = 0,
  kCannotShowUnknownReason = 1,
  kCannotShowDelegateWasNull = 2,
  kCannotShowExistingPaymentRequestSameTab = 3,
  kCannotShowExistingPaymentRequestDifferentTab = 4,
  kMaxValue = kCannotShowExistingPaymentRequestDifferentTab,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:PaymentRequestTryShowOutcome)

// This KeyedService is responsible for displaying and hiding Payment Request
// UI. It ensures that only one Payment Request is showing per profile.
class PaymentRequestDisplayManager : public KeyedService {
 public:
  class DisplayHandle {
   public:
    DisplayHandle(base::WeakPtr<PaymentRequestDisplayManager> display_manager,
                  base::WeakPtr<ContentPaymentRequestDelegate> delegate);

    DisplayHandle(const DisplayHandle&) = delete;
    DisplayHandle& operator=(const DisplayHandle&) = delete;

    ~DisplayHandle();
    void Show(base::WeakPtr<PaymentRequest> request);
    void Retry();
    // Attempt to display |url| inside the Payment Request dialog and run
    // |callback| after navigation is completed, passing true/false to indicate
    // success/failure.
    void DisplayPaymentHandlerWindow(const GURL& url,
                                     PaymentHandlerOpenWindowCallback callback);

    // Returns true after Show() was called.
    bool was_shown() const { return was_shown_; }

    base::WeakPtr<ContentPaymentRequestDelegate> delegate() {
      return delegate_;
    }

    base::WeakPtr<DisplayHandle> GetWeakPtr();

   private:
    base::WeakPtr<PaymentRequestDisplayManager> display_manager_;
    base::WeakPtr<ContentPaymentRequestDelegate> delegate_;
    bool was_shown_ = false;

    base::WeakPtrFactory<DisplayHandle> weak_ptr_factory_{this};
  };

  PaymentRequestDisplayManager();

  PaymentRequestDisplayManager(const PaymentRequestDisplayManager&) = delete;
  PaymentRequestDisplayManager& operator=(const PaymentRequestDisplayManager&) =
      delete;

  ~PaymentRequestDisplayManager() override;

  // If no PaymentRequest is currently showing, returns a unique_ptr to a
  // display handle that can be used to display the PaymentRequest dialog. The
  // UI is considered open until the handle object is deleted. |callback| is
  // called with true if the window is finished opening successfully, false if
  // opening it failed.
  std::unique_ptr<DisplayHandle> TryShow(
      base::WeakPtr<ContentPaymentRequestDelegate> delegate);
  void ShowPaymentHandlerWindow(const GURL& url,
                                PaymentHandlerOpenWindowCallback callback);

  base::WeakPtr<PaymentRequestDisplayManager> GetWeakPtr();

 private:
  void set_current_handle(base::WeakPtr<DisplayHandle> handle) {
    current_handle_ = handle;
  }

  base::WeakPtr<DisplayHandle> current_handle_;

  base::WeakPtrFactory<PaymentRequestDisplayManager> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DISPLAY_MANAGER_H_
