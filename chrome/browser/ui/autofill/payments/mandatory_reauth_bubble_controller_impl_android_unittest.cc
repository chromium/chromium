// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

namespace autofill {
namespace {

class MandatoryReauthBubbleControllerImplAndroidTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AddChild(web_contents()->GetNativeView());
    MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    window_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void InvokeJavaControllerBridgeOnClosed(
      const base::android::JavaRef<jobject>& java_bridge,
      PaymentsUiClosedReason reason) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jclass> bridge_class =
        base::android::ScopedJavaLocalRef<jclass>::Adopt(
            env, env->GetObjectClass(java_bridge.obj()));
    jmethodID on_closed =
        env->GetMethodID(bridge_class.obj(), "onClosed", "(I)V");
    ASSERT_NE(on_closed, nullptr);
    env->CallVoidMethod(java_bridge.obj(), on_closed,
                        static_cast<jint>(reason));
    ASSERT_FALSE(base::android::ClearException(env));
  }

  MandatoryReauthBubbleControllerImpl* controller() {
    return MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents());
  }

 private:
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
};

// Regression test for crbug.com/573980017. When `CreateAndShow()` returns null
// (for example because `BottomSheetController` queued the sheet behind another
// closing sheet), destroying `WebContents` must clear the Java controller
// bridge's native pointer so a delayed `onClosed()` is a safe no-op.
TEST_F(MandatoryReauthBubbleControllerImplAndroidTest,
       JavaBridgeIgnoresCloseAfterWebContentsDestroyedOnFailedShow) {
  base::MockOnceClosure cancel_callback;
  EXPECT_CALL(cancel_callback, Run).Times(0);

  // Retain the controller's singleton Java bridge before `SetupAndShowBubble()`
  // (which passes `GetJavaControllerBridge()` to Java and returns null in unit
  // tests without a `BottomSheetController`) to simulate a queued Java
  // `MandatoryReauthOptInBottomSheetCoordinator` still holding the bridge.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge(
      static_cast<MandatoryReauthBubbleController*>(controller())
          ->GetJavaControllerBridge());
  ASSERT_TRUE(java_bridge);

  controller()->SetupAndShowBubble(base::DoNothing(), cancel_callback.Get(),
                                   base::DoNothing());
  ASSERT_EQ(controller()->GetBubbleView(), nullptr);

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(env->IsSameObject(
      java_bridge.obj(),
      static_cast<MandatoryReauthBubbleController*>(controller())
          ->GetJavaControllerBridge()
          .obj()));

  DeleteContents();

  ASSERT_NO_FATAL_FAILURE(InvokeJavaControllerBridgeOnClosed(
      java_bridge, PaymentsUiClosedReason::kCancelled));
}

// Verifies that `DoShowBubble()` does not eagerly call `destroy()` on the Java
// bridge when `CreateAndShow()` returns null, so a sheet queued in
// `BottomSheetController` can still deliver `kAccepted` while `WebContents` is
// alive.
TEST_F(MandatoryReauthBubbleControllerImplAndroidTest,
       JavaBridgeForwardsAcceptAfterFailedImmediateShow) {
  base::HistogramTester histogram_tester;
  base::MockOnceClosure accept_callback;
  EXPECT_CALL(accept_callback, Run).Times(1);

  base::android::ScopedJavaGlobalRef<jobject> java_bridge(
      static_cast<MandatoryReauthBubbleController*>(controller())
          ->GetJavaControllerBridge());
  ASSERT_TRUE(java_bridge);

  controller()->SetupAndShowBubble(accept_callback.Get(), base::DoNothing(),
                                   base::DoNothing());
  ASSERT_EQ(controller()->GetBubbleView(), nullptr);

  ASSERT_NO_FATAL_FAILURE(InvokeJavaControllerBridgeOnClosed(
      java_bridge, PaymentsUiClosedReason::kAccepted));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleResult.FirstShow",
      autofill_metrics::MandatoryReauthOptInBubbleResult::kAccepted, 1);
}

}  // namespace
}  // namespace autofill
