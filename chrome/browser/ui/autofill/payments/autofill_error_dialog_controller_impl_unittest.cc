// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
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
  controller()->Show(
      AutofillErrorDialogController::VIRTUAL_CARD_TEMPORARY_ERROR);

  // Verify that the metric for shown is incremented.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ErrorDialogShown",
      AutofillErrorDialogController::VIRTUAL_CARD_TEMPORARY_ERROR, 1);
}

}  // namespace autofill
