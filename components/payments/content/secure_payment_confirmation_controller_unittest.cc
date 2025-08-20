// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

#include <list>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_event_waiter.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/payments/content/test_content_payment_request_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class SecurePaymentConfirmationControllerTest
    : public testing::Test,
      public PaymentRequest::ObserverForTest {
 public:
  enum Event : int {
    CONNECTION_TERMINATED,
  };

  SecurePaymentConfirmationControllerTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {
    auto delegate = std::make_unique<TestContentPaymentRequestDelegate>(
        /*task_executor=*/nullptr, &personal_data_manager_);
    delegate->set_frame_routing_id(
        web_contents_->GetPrimaryMainFrame()->GetGlobalId());
    mojo::PendingRemote<payments::mojom::PaymentRequest> remote;
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    // PaymentRequest deletes itself through a callback after the controller is
    // done with it so a raw pointer is used here instead of a unique_ptr.
    raw_ptr<PaymentRequest> payment_request =
        new PaymentRequest(std::move(delegate), std::move(receiver));
    payment_request->set_observer_for_test(GetWeakPtr());

    controller_ = std::make_unique<SecurePaymentConfirmationController>(
        payment_request->GetWeakPtr());
  }

  void CreateEventWaiter(std::list<Event> events) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<Event>>(events);
  }

  void WaitForEvents() { ASSERT_TRUE(event_waiter_->Wait()); }

  SecurePaymentConfirmationController* controller() {
    return controller_.get();
  }

  base::WeakPtr<SecurePaymentConfirmationControllerTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  // PaymentRequest::ObserverForTest:
  void OnCanMakePaymentCalled() override {}
  void OnCanMakePaymentReturned() override {}
  void OnHasEnrolledInstrumentCalled() override {}
  void OnHasEnrolledInstrumentReturned() override {}
  void OnNotSupportedError() override {}
  void OnConnectionTerminated() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(Event::CONNECTION_TERMINATED);
    }
  }
  void OnPayCalled() override {}
  void OnAbortCalled() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  autofill::TestPersonalDataManager personal_data_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  std::unique_ptr<autofill::EventWaiter<Event>> event_waiter_;
  std::unique_ptr<SecurePaymentConfirmationController> controller_;

  base::WeakPtrFactory<SecurePaymentConfirmationControllerTest>
      weak_ptr_factory_{this};
};

TEST_F(SecurePaymentConfirmationControllerTest, Metrics_OnCancel) {
  base::HistogramTester histogram_tester;

  CreateEventWaiter({Event::CONNECTION_TERMINATED});
  controller()->OnCancel();
  WaitForEvents();

  histogram_tester.ExpectUniqueSample(
      "SecurePaymentRequest.Transaction.Outcome",
      SecurePaymentRequestOutcome::kAnotherWay,
      /*expected_bucket_count=*/1);
}

TEST_F(SecurePaymentConfirmationControllerTest, Metrics_OnOptOut) {
  base::HistogramTester histogram_tester;

  CreateEventWaiter({Event::CONNECTION_TERMINATED});
  controller()->OnOptOut();
  WaitForEvents();

  histogram_tester.ExpectUniqueSample(
      "SecurePaymentRequest.Transaction.Outcome",
      SecurePaymentRequestOutcome::kOptOut,
      /*expected_bucket_count=*/1);
}

TEST_F(SecurePaymentConfirmationControllerTest, Metrics_OnConfirm) {
  base::HistogramTester histogram_tester;

  CreateEventWaiter({Event::CONNECTION_TERMINATED});
  controller()->OnConfirm();
  WaitForEvents();

  histogram_tester.ExpectUniqueSample(
      "SecurePaymentRequest.Transaction.Outcome",
      SecurePaymentRequestOutcome::kAccept,
      /*expected_bucket_count=*/1);
}

}  // namespace payments
