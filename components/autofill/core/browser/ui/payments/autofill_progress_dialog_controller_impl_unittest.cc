// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestAutofillProgressDialogView : public AutofillProgressDialogView {
 public:
  TestAutofillProgressDialogView() = default;
  TestAutofillProgressDialogView(const TestAutofillProgressDialogView&) =
      delete;
  TestAutofillProgressDialogView& operator=(
      const TestAutofillProgressDialogView&) = delete;
  ~TestAutofillProgressDialogView() override = default;

  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override {}
  void InvalidateControllerForCallbacks() override {}
  base::WeakPtr<AutofillProgressDialogView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestAutofillProgressDialogView> weak_ptr_factory_{this};
};

class AutofillProgressDialogControllerImplTest : public testing::Test {
 public:

  base::WeakPtr<AutofillProgressDialogView> CreateDialogView() {
    if (!view_) {
      view_ = std::make_unique<TestAutofillProgressDialogView>();
    }
    return view_->GetWeakPtr();
  }

  AutofillProgressDialogControllerImpl* controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<AutofillProgressDialogView> view_;
  std::unique_ptr<AutofillProgressDialogControllerImpl> controller_;
};

}  // namespace autofill
