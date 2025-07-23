// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_base_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/AndroidBaseWindowNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
}  // namespace

class AndroidBaseWindowUnitTest : public testing::Test {
 public:
  AndroidBaseWindowUnitTest() = default;
  ~AndroidBaseWindowUnitTest() override = default;

  void SetUp() override {
    java_test_support_ =
        Java_AndroidBaseWindowNativeUnitTestSupport_Constructor(
            AttachCurrentThread());
  }

  void TearDown() override { InvokeJavaDestroy(); }

  AndroidBaseWindow* InvokeJavaGetOrCreateNativePtr() const {
    return reinterpret_cast<AndroidBaseWindow*>(
        Java_AndroidBaseWindowNativeUnitTestSupport_invokeGetOrCreateNativePtr(
            AttachCurrentThread(), java_test_support_));
  }

  AndroidBaseWindow* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<AndroidBaseWindow*>(
        Java_AndroidBaseWindowNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroy() const {
    Java_AndroidBaseWindowNativeUnitTestSupport_invokeDestroy(
        AttachCurrentThread(), java_test_support_);
  }

 private:
  ScopedJavaGlobalRef<jobject> java_test_support_;
};

TEST_F(AndroidBaseWindowUnitTest,
       JavaGetOrCreateNativePtrMethodReturnsSamePtr) {
  // Arrange & Act: call Java GetOrCreateNativePtr() twice.
  AndroidBaseWindow* ptr1 = InvokeJavaGetOrCreateNativePtr();
  AndroidBaseWindow* ptr2 = InvokeJavaGetOrCreateNativePtr();

  // Assert: the two calls should return the same non-null pointer.
  EXPECT_NE(nullptr, ptr1);
  EXPECT_NE(nullptr, ptr2);
  EXPECT_EQ(ptr1, ptr2);
}

TEST_F(AndroidBaseWindowUnitTest, JavaDestroyMethodClearsPtrValueInJava) {
  // Arrange.
  InvokeJavaGetOrCreateNativePtr();

  // Act: call Java destroy().
  InvokeJavaDestroy();

  // Assert: the native pointer on the Java side should be set to null.
  AndroidBaseWindow* android_base_window = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, android_base_window);
}
