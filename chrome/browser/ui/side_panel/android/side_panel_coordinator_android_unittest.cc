// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/side_panel/test/android/native_unit_test_support_jni/SidePanelCoordinatorAndroidNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

class SidePanelCoordinatorAndroidUnitTest : public testing::Test {
 public:
  SidePanelCoordinatorAndroidUnitTest() = default;
  ~SidePanelCoordinatorAndroidUnitTest() override = default;

  void SetUp() override { SetUpJavaSupport(); }

  void TearDown() override { InvokeJavaDestroy(); }

  SidePanelCoordinatorAndroid* InvokeJavaCreateNativePtr() const {
    return reinterpret_cast<SidePanelCoordinatorAndroid*>(
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeCreateNativePtr(
            AttachCurrentThread(), java_test_support_));
  }

  SidePanelCoordinatorAndroid* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<SidePanelCoordinatorAndroid*>(
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroy() const {
    Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeDestroy(
        AttachCurrentThread(), java_test_support_);
  }

 protected:
  void SetUpJavaSupport() {
    java_test_support_ =
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_Constructor(
            AttachCurrentThread());
  }

  ScopedJavaGlobalRef<jobject> java_test_support_;
};

TEST_F(SidePanelCoordinatorAndroidUnitTest,
       JavaCreateNativePtrMethodReturnsValidPtr) {
  SidePanelCoordinatorAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);
}

TEST_F(SidePanelCoordinatorAndroidUnitTest,
       JavaCreateNativePtrMethodCrashesIfCalledTwice) {
  SidePanelCoordinatorAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  EXPECT_DEATH(InvokeJavaCreateNativePtr(), "");
}

TEST_F(SidePanelCoordinatorAndroidUnitTest,
       JavaDestroyMethodClearsPtrValueInJava) {
  // Arrange.
  SidePanelCoordinatorAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  // Act: call Java destroy().
  InvokeJavaDestroy();

  // Assert: the native pointer on the Java side should be set to null.
  ptr = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, ptr);
}

DEFINE_JNI(SidePanelCoordinatorAndroidNativeUnitTestSupport)
