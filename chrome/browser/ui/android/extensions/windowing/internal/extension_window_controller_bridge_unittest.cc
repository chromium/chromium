// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/extensions/windowing/test/native_unit_test_support_jni/ExtensionWindowControllerBridgeNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
}  // namespace

class ExtensionWindowControllerBridgeUnitTest : public testing::Test {
 public:
  ExtensionWindowControllerBridgeUnitTest() = default;
  ~ExtensionWindowControllerBridgeUnitTest() override = default;

  void SetUp() override {
    java_test_support_.Reset(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_Constructor(
            AttachCurrentThread()));
  }

  void TearDown() override { InvokeJavaOnTaskRemoved(); }

  void InvokeJavaOnAddedToTask() const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnAddedToTask(
        AttachCurrentThread(), java_test_support_);
  }

  void InvokeJavaOnTaskRemoved() const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnTaskRemoved(
        AttachCurrentThread(), java_test_support_);
  }

  ExtensionWindowControllerBridge* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<ExtensionWindowControllerBridge*>(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

 private:
  ScopedJavaGlobalRef<jobject> java_test_support_;
};

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       JavaOnAddedToTaskCreatesNativeObj) {
  // Act.
  InvokeJavaOnAddedToTask();

  // Assert.
  ExtensionWindowControllerBridge* native_ptr =
      InvokeJavaGetNativePtrForTesting();
  EXPECT_NE(nullptr, native_ptr);
}

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       CallingJavaOnAddedToTaskTwiceFails) {
  EXPECT_DEATH(
      {
        InvokeJavaOnAddedToTask();
        InvokeJavaOnAddedToTask();
      },
      /*matcher=*/"");
}

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       JavaOnTaskRemovedClearsNativePtrValueInJava) {
  // Arrange.
  InvokeJavaOnAddedToTask();

  // Act.
  InvokeJavaOnTaskRemoved();

  // Assert.
  EXPECT_EQ(nullptr, InvokeJavaGetNativePtrForTesting());
}

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       CallingJavaOnTaskRemovedTwiceDoesNotCrash) {
  // Arrange.
  InvokeJavaOnAddedToTask();

  // Act.
  InvokeJavaOnTaskRemoved();
  InvokeJavaOnTaskRemoved();

  // Assert.
  EXPECT_EQ(nullptr, InvokeJavaGetNativePtrForTesting());
}
