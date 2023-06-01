// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/content/test_content_payment_request_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestWebContentsManagerTest : public testing::Test {
 public:
  PaymentRequestWebContentsManagerTest()
      : web_contents_(web_contents_factory_.CreateWebContents(&context_)) {
    manager_ = PaymentRequestWebContentsManager::GetOrCreateForWebContents(
        *web_contents_);
  }

  content::WebContents* web_contents() { return web_contents_; }

  PaymentRequest* CreateAndReturnPaymentRequest(SPCTransactionMode mode) {
    manager_->SetSPCTransactionMode(mode);

    std::unique_ptr<TestContentPaymentRequestDelegate> delegate =
        std::make_unique<TestContentPaymentRequestDelegate>(
            /*task_executor=*/nullptr, &test_personal_data_manager_);
    delegate->set_frame_routing_id(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId());

    mojo::PendingRemote<payments::mojom::PaymentRequest> remote;
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver =
        remote.InitWithNewPipeAndPassReceiver();

    // PaymentRequest is a DocumentService, whose lifetime is managed by the
    // RenderFrameHost passed into the delegate.
    return new PaymentRequest(std::move(delegate), std::move(receiver));
  }

  // The PaymentRequestWebContentsManager under test.
  raw_ptr<PaymentRequestWebContentsManager, DanglingUntriaged> manager_;

 private:
  // Necessary supporting members to create the testing environment.
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;

  // Used in the creation of PaymentRequests.
  autofill::TestPersonalDataManager test_personal_data_manager_;
};

TEST_F(PaymentRequestWebContentsManagerTest, SPCTransactionMode) {
  // The mode given to the PaymentRequest is exposed on its API.
  PaymentRequest* request1 =
      CreateAndReturnPaymentRequest(SPCTransactionMode::NONE);
  ASSERT_EQ(request1->spc_transaction_mode(), SPCTransactionMode::NONE);
  PaymentRequest* request2 =
      CreateAndReturnPaymentRequest(SPCTransactionMode::AUTOACCEPT);
  ASSERT_EQ(request2->spc_transaction_mode(), SPCTransactionMode::AUTOACCEPT);
  PaymentRequest* request3 =
      CreateAndReturnPaymentRequest(SPCTransactionMode::AUTOREJECT);
  ASSERT_EQ(request3->spc_transaction_mode(), SPCTransactionMode::AUTOREJECT);

  // Check that already-created PaymentRequests were not altered.
  ASSERT_EQ(request1->spc_transaction_mode(), SPCTransactionMode::NONE);
  ASSERT_EQ(request2->spc_transaction_mode(), SPCTransactionMode::AUTOACCEPT);
}

}  // namespace payments
