// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillErrorDialogControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillErrorDialogControllerImplTest() = default;

  AutofillErrorDialogControllerImpl* controller() {
    if (!controller_)
      controller_ = new AutofillErrorDialogControllerImpl(web_contents());
    return controller_;
  }

 private:
  raw_ptr<AutofillErrorDialogControllerImpl> controller_ = nullptr;
};

TEST_F(AutofillErrorDialogControllerImplTest, MetricsTest) {
  base::HistogramTester histogram_tester;
  AutofillErrorDialogContext context;
  context.type = AutofillErrorDialogType::kVirtualCardTemporaryError;
  controller()->Show(context);

  // Verify that the metric for shown is incremented.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ErrorDialogShown",
      AutofillErrorDialogType::kVirtualCardTemporaryError, 1);
}

}  // namespace autofill
