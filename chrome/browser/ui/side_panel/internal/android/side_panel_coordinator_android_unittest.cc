// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
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

  void SetUp() override {
    SetUpJavaSupport();
    SetUpMockBrowserWindow();
  }

  void TearDown() override { InvokeJavaDestroyNativePtr(); }

  SidePanelCoordinatorAndroid* InvokeJavaCreateNativePtr() const {
    return reinterpret_cast<SidePanelCoordinatorAndroid*>(
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeCreateNativePtr(
            AttachCurrentThread(), java_test_support_,
            reinterpret_cast<int64_t>(mock_browser_.get())));
  }

  SidePanelCoordinatorAndroid* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<SidePanelCoordinatorAndroid*>(
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroyNativePtr() const {
    Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_invokeDestroyNativePtr(
        AttachCurrentThread(), java_test_support_);
  }

 protected:
  void SetUpJavaSupport() {
    java_test_support_ =
        Java_SidePanelCoordinatorAndroidNativeUnitTestSupport_Constructor(
            AttachCurrentThread());
  }

  void SetUpMockBrowserWindow() {
    mock_browser_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*mock_browser_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
  }

  ScopedJavaGlobalRef<jobject> java_test_support_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(SidePanelCoordinatorAndroidUnitTest,
       FromReturnsCorrectPtrForValidBrowserWindow) {
  SidePanelCoordinatorAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_EQ(ptr, SidePanelCoordinatorAndroid::From(mock_browser_.get()));
}

TEST_F(SidePanelCoordinatorAndroidUnitTest,
       FromReturnsNullPtrForNullBrowserWindow) {
  InvokeJavaCreateNativePtr();
  EXPECT_EQ(nullptr, SidePanelCoordinatorAndroid::From(/*browser=*/nullptr));
}

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
       JavaDestroyNativePtrMethodClearsPtrValueInJava) {
  // Arrange.
  SidePanelCoordinatorAndroid* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  // Act: call Java destroy().
  InvokeJavaDestroyNativePtr();

  // Assert: the native pointer on the Java side should be set to null.
  ptr = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, ptr);
}

DEFINE_JNI(SidePanelCoordinatorAndroidNativeUnitTestSupport)
