// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_display_manager.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/payments/content/mock_content_payment_request_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {

using PaymentRequestDisplayManagerTest = content::RenderViewHostTestHarness;

TEST_F(PaymentRequestDisplayManagerTest, TryShow_Success) {
  base::HistogramTester histogram_tester;
  PaymentRequestDisplayManager display_manager;
  MockContentPaymentRequestDelegate mock_delegate;

  EXPECT_TRUE(display_manager.TryShow(mock_delegate.GetContentWeakPtr()));
  histogram_tester.ExpectUniqueSample("PaymentRequest.Show.TryShowOutcome",
                                      PaymentRequestTryShowOutcome::kAbleToShow,
                                      /*expected_bucket_count=*/1);
}

TEST_F(PaymentRequestDisplayManagerTest,
       TryShow_ExistingPaymentRequestSameTab) {
  PaymentRequestDisplayManager display_manager;
  std::unique_ptr<content::WebContents> web_contents(CreateTestWebContents());
  MockContentPaymentRequestDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetRenderFrameHost)
      .WillRepeatedly(testing::Return(web_contents->GetPrimaryMainFrame()));

  // Set up an ongoing PaymentRequest.
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> existing_handle =
      display_manager.TryShow(mock_delegate.GetContentWeakPtr());
  ASSERT_TRUE(existing_handle);

  // Now trigger another PaymentRequest using the same delegate (and thus the
  // same tab). This should be rejected and have the corresponding histogram
  // sample recorded.
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(display_manager.TryShow(mock_delegate.GetContentWeakPtr()));
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Show.TryShowOutcome",
      PaymentRequestTryShowOutcome::kCannotShowExistingPaymentRequestSameTab,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentRequestDisplayManagerTest,
       TryShow_ExistingPaymentRequestDifferentTab) {
  PaymentRequestDisplayManager display_manager;

  // Set up an ongoing PaymentRequest.
  MockContentPaymentRequestDelegate mock_delegate_1;
  std::unique_ptr<content::WebContents> web_contents_1(CreateTestWebContents());
  EXPECT_CALL(mock_delegate_1, GetRenderFrameHost)
      .WillRepeatedly(testing::Return(web_contents_1->GetPrimaryMainFrame()));
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> existing_handle =
      display_manager.TryShow(mock_delegate_1.GetContentWeakPtr());
  ASSERT_TRUE(existing_handle);

  // Now trigger another PaymentRequest in a different tab. This should be
  // rejected and have the corresponding histogram sample recorded.
  base::HistogramTester histogram_tester;
  MockContentPaymentRequestDelegate mock_delegate_2;
  std::unique_ptr<content::WebContents> web_contents_2(CreateTestWebContents());
  EXPECT_CALL(mock_delegate_2, GetRenderFrameHost)
      .WillRepeatedly(testing::Return(web_contents_2->GetPrimaryMainFrame()));
  EXPECT_FALSE(display_manager.TryShow(mock_delegate_2.GetContentWeakPtr()));
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Show.TryShowOutcome",
      PaymentRequestTryShowOutcome::
          kCannotShowExistingPaymentRequestDifferentTab,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentRequestDisplayManagerTest, TryShow_NullDelegate) {
  PaymentRequestDisplayManager display_manager;

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(display_manager.TryShow(nullptr));
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Show.TryShowOutcome",
      PaymentRequestTryShowOutcome::kCannotShowDelegateWasNull,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentRequestDisplayManagerTest, TryShow_ExistingDelegateBecameNull) {
  PaymentRequestDisplayManager display_manager;

  // Set up an ongoing PaymentRequest.
  auto mock_delegate_1 = std::make_unique<MockContentPaymentRequestDelegate>();
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> existing_handle =
      display_manager.TryShow(mock_delegate_1->GetContentWeakPtr());
  ASSERT_TRUE(existing_handle);

  // Reset the first delegate. This isn't expected to happen in the wild, but
  // the code should be able to handle it.
  mock_delegate_1.reset();

  // Now trigger another PaymentRequest using a new delegate. This should be
  // rejected as there is an existing show() going on, even though the delegate
  // has been cleared. A histogram sample should also be specifically
  // recorded for this situation.
  base::HistogramTester histogram_tester;
  MockContentPaymentRequestDelegate mock_delegate_2;
  EXPECT_FALSE(display_manager.TryShow(mock_delegate_2.GetContentWeakPtr()));
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.Show.TryShowOutcome",
      PaymentRequestTryShowOutcome::kCannotShowUnknownReason,
      /*expected_bucket_count=*/1);
}

TEST_F(PaymentRequestDisplayManagerTest,
       ShowPaymentHandlerWindow_NoInvokedApp) {
  PaymentRequestDisplayManager display_manager;
  MockContentPaymentRequestDelegate mock_delegate;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle =
      display_manager.TryShow(mock_delegate.GetContentWeakPtr());
  ASSERT_TRUE(handle);

  base::MockCallback<PaymentHandlerOpenWindowCallback> callback;
  EXPECT_CALL(callback, Run(/*success=*/false, /*render_process_id=*/0,
                            /*render_frame_id=*/0));

  handle->DisplayPaymentHandlerWindow(
      GURL("https://example.com"), /*preferred_web_contents_size=*/std::nullopt,
      callback.Get());
}

TEST_F(PaymentRequestDisplayManagerTest,
       ShowPaymentHandlerWindow_MatchingOrigin) {
  PaymentRequestDisplayManager display_manager;
  MockContentPaymentRequestDelegate mock_delegate;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle =
      display_manager.TryShow(mock_delegate.GetContentWeakPtr());
  ASSERT_TRUE(handle);

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  handle->SetPaymentHandlerOrigin(origin);

  base::MockCallback<PaymentHandlerOpenWindowCallback> callback;
  EXPECT_CALL(mock_delegate,
              EmbedPaymentHandlerWindow(GURL("https://example.com/pay"),
                                        testing::Eq(std::nullopt), testing::_))
      .WillOnce([](const GURL& url,
                   const std::optional<gfx::Size>& preferred_web_contents_size,
                   PaymentHandlerOpenWindowCallback cb) {
        std::move(cb).Run(/*success=*/true, /*render_process_id=*/1,
                          /*render_frame_id=*/2);
      });
  EXPECT_CALL(callback, Run(/*success=*/true, /*render_process_id=*/1,
                            /*render_frame_id=*/2));

  handle->DisplayPaymentHandlerWindow(
      GURL("https://example.com/pay"),
      /*preferred_web_contents_size=*/std::nullopt, callback.Get());
}

TEST_F(PaymentRequestDisplayManagerTest,
       ShowPaymentHandlerWindow_ForwardsWindowSize) {
  PaymentRequestDisplayManager display_manager;
  MockContentPaymentRequestDelegate mock_delegate;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle =
      display_manager.TryShow(mock_delegate.GetContentWeakPtr());
  ASSERT_TRUE(handle);

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  handle->SetPaymentHandlerOrigin(origin);

  base::MockCallback<PaymentHandlerOpenWindowCallback> callback;
  EXPECT_CALL(
      mock_delegate,
      EmbedPaymentHandlerWindow(
          GURL("https://example.com/pay"),
          testing::Eq(std::make_optional(gfx::Size(640, 650))), testing::_))
      .WillOnce([](const GURL& url,
                   const std::optional<gfx::Size>& preferred_web_contents_size,
                   PaymentHandlerOpenWindowCallback cb) {
        std::move(cb).Run(/*success=*/true, /*render_process_id=*/1,
                          /*render_frame_id=*/2);
      });
  EXPECT_CALL(callback, Run(/*success=*/true, /*render_process_id=*/1,
                            /*render_frame_id=*/2));

  handle->DisplayPaymentHandlerWindow(GURL("https://example.com/pay"),
                                      gfx::Size(640, 650), callback.Get());
}

TEST_F(PaymentRequestDisplayManagerTest,
       ShowPaymentHandlerWindow_MismatchingOrigin) {
  PaymentRequestDisplayManager display_manager;
  MockContentPaymentRequestDelegate mock_delegate;
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle =
      display_manager.TryShow(mock_delegate.GetContentWeakPtr());
  ASSERT_TRUE(handle);

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  handle->SetPaymentHandlerOrigin(origin);

  base::MockCallback<PaymentHandlerOpenWindowCallback> callback;
  EXPECT_CALL(callback, Run(/*success=*/false, /*render_process_id=*/0,
                            /*render_frame_id=*/0));

  handle->DisplayPaymentHandlerWindow(
      GURL("https://not-example.com/pay"),
      /*preferred_web_contents_size=*/std::nullopt, callback.Get());
}

}  // namespace payments
