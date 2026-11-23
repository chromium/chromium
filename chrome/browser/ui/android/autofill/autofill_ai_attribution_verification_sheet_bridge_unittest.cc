// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_attribution_verification_sheet_bridge.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/android/autofill/autofill_ai_attribution_verification_sheet_bridge_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

using Decision = AutofillClient::AutofillAiVerificationDecision;
using DecisionCallback = AutofillClient::AutofillAiVerificationDecisionCallback;
using ::testing::InSequence;

class AutofillAiAttributionVerificationSheetBridgeTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    bridge_ = std::make_unique<AutofillAiAttributionVerificationSheetBridge>(
        web_contents());
  }

  void TearDown() override {
    bridge_.reset();
    window_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<AutofillAiAttributionVerificationSheetBridge> bridge_;
};

TEST_F(AutofillAiAttributionVerificationSheetBridgeTest,
       RequestShowContentWithoutWindowResolvesDismiss) {
  window_.reset();
  base::MockCallback<DecisionCallback> decision_cb;
  EXPECT_CALL(decision_cb, Run(Decision::kDismiss));

  bridge_->RequestShowContent(decision_cb.Get());
}

TEST_F(AutofillAiAttributionVerificationSheetBridgeTest,
       RequestShowContentWithoutBottomSheetControllerResolvesDismiss) {
  // The test `WindowAndroid` has no `BottomSheetController` attached, so the
  // Java bridge synchronously notifies dismissal back to native.
  base::MockCallback<DecisionCallback> decision_cb;
  EXPECT_CALL(decision_cb, Run(Decision::kDismiss));

  bridge_->RequestShowContent(decision_cb.Get());
}

TEST_F(AutofillAiAttributionVerificationSheetBridgeTest,
       OnDismissedRunsCallbackOnce) {
  base::MockCallback<DecisionCallback> decision_cb;
  EXPECT_CALL(decision_cb, Run(Decision::kDismiss)).Times(1);

  test_api(*bridge_).SetDecisionCallback(decision_cb.Get());
  bridge_->OnDismissed();
  bridge_->OnDismissed();
}

TEST_F(AutofillAiAttributionVerificationSheetBridgeTest,
       RequestShowContentSupersedesPendingCallback) {
  base::MockCallback<DecisionCallback> first_cb;
  base::MockCallback<DecisionCallback> second_cb;
  {
    InSequence s;
    EXPECT_CALL(first_cb, Run(Decision::kDismiss));
    EXPECT_CALL(second_cb, Run(Decision::kDismiss));
  }

  test_api(*bridge_).SetDecisionCallback(first_cb.Get());
  bridge_->RequestShowContent(second_cb.Get());
}

TEST_F(AutofillAiAttributionVerificationSheetBridgeTest,
       DestructorResolvesPendingCallbackWithDismiss) {
  base::MockCallback<DecisionCallback> decision_cb;
  EXPECT_CALL(decision_cb, Run(Decision::kDismiss));

  test_api(*bridge_).SetDecisionCallback(decision_cb.Get());
  bridge_.reset();
}

}  // namespace
}  // namespace autofill
