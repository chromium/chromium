// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/buildflag.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_ui_type.h"
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

  void DeleteController() { controller_.reset(); }

  void InitializeController(base::OnceClosure cancel_callback) {
    controller_ = std::make_unique<AutofillProgressDialogControllerImpl>(
        AutofillProgressUiType::kVirtualCardUnmaskProgressUi,
        std::move(cancel_callback));
#if BUILDFLAG(IS_IOS)
    controller_->ShowDialog(base::BindOnce(
        &AutofillProgressDialogControllerImplTest::CreateDialogView,
        base::Unretained(this)));
#else
    controller_->ShowDialog(
        base::BindOnce([]() -> std::unique_ptr<AutofillProgressDialogView> {
          return std::make_unique<TestAutofillProgressDialogView>();
        }));
#endif
  }

 private:
  std::unique_ptr<AutofillProgressDialogView> view_;
  std::unique_ptr<AutofillProgressDialogControllerImpl> controller_;
};

// Tests that a Use-After-Free (UAF) is prevented when the controller is
// destroyed synchronously during the success callback. A UAF can occur if
// `OnDismissed()` accesses members after the callback deletes the controller.
TEST_F(AutofillProgressDialogControllerImplTest,
       OnDismissed_Success_SafeSelfDestruction) {
  base::MockCallback<base::OnceClosure> cancel_callback;
  InitializeController(cancel_callback.Get());

  base::OnceClosure no_interactive_auth_callback =
      base::BindLambdaForTesting([&]() { DeleteController(); });

  controller()->DismissDialog(/*show_confirmation_before_closing=*/false,
                              std::move(no_interactive_auth_callback));

  controller()->OnDismissed(/*is_canceled_by_user=*/false);

  EXPECT_EQ(controller(), nullptr);
}

// Tests the cancellation path of the UAF fix. Ensures that if the controller
// is destroyed synchronously during the cancel callback, the `WeakPtr` safely
// prevents `OnDismissed()` from accessing freed memory.
TEST_F(AutofillProgressDialogControllerImplTest,
       OnDismissed_Canceled_SafeSelfDestruction) {
  InitializeController(
      base::BindLambdaForTesting([&]() { DeleteController(); }));

  controller()->OnDismissed(/*is_canceled_by_user=*/true);

  EXPECT_EQ(controller(), nullptr);
}

}  // namespace autofill
