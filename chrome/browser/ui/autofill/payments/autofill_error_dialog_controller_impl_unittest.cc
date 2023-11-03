// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Param of the AutofillErrorDialogControllerImplTest:
// -- bool server_did_return_decline_details;
class AutofillErrorDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  AutofillErrorDialogControllerImplTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    controller_ =
        std::make_unique<AutofillErrorDialogControllerImpl>(web_contents());
  }

  void TearDown() override {
    // Reset explicitly to avoid a dangling pointer to the `WebContents` inside
    // of the controller. This mirrors the behavior in production
    // code in which `ChromeAutofillClient` owns the controller and is destroyed
    // prior to the destruction of the respective `WebContents`.
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  AutofillErrorDialogControllerImpl* controller() { return controller_.get(); }

 private:
  std::unique_ptr<AutofillErrorDialogControllerImpl> controller_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillErrorDialogControllerImplTest,
                         testing::Bool());

TEST_P(AutofillErrorDialogControllerImplTest, MetricsTest) {
  base::HistogramTester histogram_tester;
  bool server_did_return_decline_details = GetParam();
  AutofillErrorDialogContext context;
  context.type = AutofillErrorDialogType::kVirtualCardTemporaryError;
  if (server_did_return_decline_details) {
    context.server_returned_title = "test_server_returned_title";
    context.server_returned_description = "test_server_returned_description";
  }

  controller()->Show(context);

  if (server_did_return_decline_details) {
    EXPECT_EQ(controller()->GetTitle(),
              base::UTF8ToUTF16(*context.server_returned_title));
    EXPECT_EQ(controller()->GetDescription(),
              base::UTF8ToUTF16(*context.server_returned_description));
  }

  // Verify that the metric for shown is incremented.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ErrorDialogShown",
      AutofillErrorDialogType::kVirtualCardTemporaryError, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.ErrorDialogShown.WithServerText",
      AutofillErrorDialogType::kVirtualCardTemporaryError,
      /*expected_count=*/server_did_return_decline_details ? 1 : 0);
}

}  // namespace autofill
