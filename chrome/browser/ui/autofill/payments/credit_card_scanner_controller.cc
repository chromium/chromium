// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view_delegate.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

namespace {

// Controller for the credit card scanner UI. The controller deletes itself
// after the view is dismissed.
class Controller final : public CreditCardScannerViewDelegate {
 public:
  Controller(content::WebContents* web_contents,
             payments::PaymentsAutofillClient::CreditCardScanCallback callback)
      : callback_(std::move(callback)) {
    view_ = CreditCardScannerView::Create(weak_ptr_factory_.GetWeakPtr(),
                                          web_contents);
    DCHECK(view_);
  }
  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  // Shows the UI to scan the credit card.
  void Show() {
    show_time_ = base::TimeTicks::Now();
    view_->Show();
  }

 private:
  ~Controller() override = default;

  // CreditCardScannerViewDelegate implementation.
  void ScanCancelled() override {
    AutofillMetrics::LogScanCreditCardCompleted(
        base::TimeTicks::Now() - show_time_, false);
    delete this;
  }

  // CreditCardScannerViewDelegate implementation.
  void ScanCompleted(const CreditCard& card) override {
    AutofillMetrics::LogScanCreditCardCompleted(
        base::TimeTicks::Now() - show_time_, true);
    std::move(callback_).Run(card);
    delete this;
  }

  // The view for the credit card scanner.
  std::unique_ptr<CreditCardScannerView> view_;

  // The callback to be invoked when scanning completes successfully.
  payments::PaymentsAutofillClient::CreditCardScanCallback callback_;

  // The time when the UI was shown.
  base::TimeTicks show_time_;

  base::WeakPtrFactory<Controller> weak_ptr_factory_{this};
};

}  // namespace

// static
bool CreditCardScannerController::HasCreditCardScanFeature() {
  static const bool kCanShow = CreditCardScannerView::CanShow();
  return kCanShow;
}

// static
void CreditCardScannerController::ScanCreditCard(
    content::WebContents* web_contents,
    payments::PaymentsAutofillClient::CreditCardScanCallback callback) {
  (new Controller(web_contents, std::move(callback)))->Show();
}

}  // namespace autofill
