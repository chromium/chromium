// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/android/side_panel_ui_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/side_panel/test/android/native_unit_test_support_jni/SidePanelUIAndroidNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

class SidePanelUIAndroidUnitTest : public testing::Test {
 public:
  SidePanelUIAndroidUnitTest() = default;
  ~SidePanelUIAndroidUnitTest() override = default;

  void SetUp() override { SetUpJavaSupport(); }

  void TearDown() override { InvokeJavaDestroy(); }

  SidePanelUIAndroid* InvokeJavaCreateNativePtr() const {
    return reinterpret_cast<SidePanelUIAndroid*>(
        Java_SidePanelUIAndroidNativeUnitTestSupport_invokeCreateNativePtr(
            AttachCurrentThread(), java_test_support_));
  }

  SidePanelUIAndroid* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<SidePanelUIAndroid*>(
        Java_SidePanelUIAndroidNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroy() const {
    Java_SidePanelUIAndroidNativeUnitTestSupport_invokeDestroy(
        AttachCurrentThread(), java_test_support_);
  }

 protected:
  void SetUpJavaSupport() {
    java_test_support_ =
        Java_SidePanelUIAndroidNativeUnitTestSupport_Constructor(
            AttachCurrentThread());
  }

  ScopedJavaGlobalRef<jobject> java_test_support_;
};

TEST_F(SidePanelUIAndroidUnitTest, JavaCreateNativePtrMethodReturnsValidPtr) {
  SidePanelUIAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);
}

TEST_F(SidePanelUIAndroidUnitTest,
       JavaCreateNativePtrMethodCrashesIfCalledTwice) {
  SidePanelUIAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  EXPECT_DEATH(InvokeJavaCreateNativePtr(), "");
}

TEST_F(SidePanelUIAndroidUnitTest, JavaDestroyMethodClearsPtrValueInJava) {
  // Arrange.
  SidePanelUIAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  // Act: call Java destroy().
  InvokeJavaDestroy();

  // Assert: the native pointer on the Java side should be set to null.
  ptr = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, ptr);
}

DEFINE_JNI(SidePanelUIAndroidNativeUnitTestSupport)
