// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_view_delegate.h"
#include "components/autofill/core/browser/autofill_metrics.h"

namespace autofill {

namespace {

// Controller for the credit card scanner UI. The controller deletes itself
// after the view is dismissed.
class Controller : public CreditCardScannerViewDelegate,
                   public base::SupportsWeakPtr<Controller> {
 public:
  Controller(content::WebContents* web_contents,
             const AutofillClient::CreditCardScanCallback& callback)
      : view_(CreditCardScannerView::Create(AsWeakPtr(), web_contents)),
        callback_(callback) {
    DCHECK(view_);
  }

  // Shows the UI to scan the credit card.
  void Show() {
    show_time_ = base::TimeTicks::Now();
    view_->Show();
  }

 private:
  ~Controller() override {}

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
    callback_.Run(card);
    delete this;
  }

  // The view for the credit card scanner.
  std::unique_ptr<CreditCardScannerView> view_;

  // The callback to be invoked when scanning completes successfully.
  AutofillClient::CreditCardScanCallback callback_;

  // The time when the UI was shown.
  base::TimeTicks show_time_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
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
    const AutofillClient::CreditCardScanCallback& callback) {
  (new Controller(web_contents, callback))->Show();
}

}  // namespace autofill
