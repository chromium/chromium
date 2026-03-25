// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/window_scoped_side_panel_registry_bridge.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel/test/android/native_unit_test_support_jni/WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

class WindowScopedSidePanelRegistryBridgeUnitTest : public testing::Test {
 public:
  WindowScopedSidePanelRegistryBridgeUnitTest() = default;
  ~WindowScopedSidePanelRegistryBridgeUnitTest() override = default;

  void SetUp() override {
    SetUpJavaSupport();
    SetUpMockBrowserWindow();
  }

  void TearDown() override { InvokeJavaDestroyNativePtr(); }

  WindowScopedSidePanelRegistryBridge* InvokeJavaCreateNativePtr() const {
    return reinterpret_cast<WindowScopedSidePanelRegistryBridge*>(
        Java_WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeCreateNativePtr(
            AttachCurrentThread(), java_test_support_,
            reinterpret_cast<int64_t>(mock_browser_.get())));
  }

  WindowScopedSidePanelRegistryBridge* InvokeJavaGetNativePtrForTesting()
      const {
    return reinterpret_cast<WindowScopedSidePanelRegistryBridge*>(
        Java_WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroyNativePtr() const {
    Java_WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeDestroyNativePtr(
        AttachCurrentThread(), java_test_support_);
  }

 protected:
  void SetUpJavaSupport() {
    java_test_support_ =
        Java_WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport_Constructor(
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

TEST_F(WindowScopedSidePanelRegistryBridgeUnitTest,
       JavaCreateNativePtrMethodReturnsValidPtr) {
  WindowScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);
}

TEST_F(WindowScopedSidePanelRegistryBridgeUnitTest,
       JavaCreateNativePtrMethodCrashesIfCalledTwice) {
  WindowScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  EXPECT_DEATH(InvokeJavaCreateNativePtr(), "");
}

TEST_F(WindowScopedSidePanelRegistryBridgeUnitTest,
       JavaDestroyNativePtrMethodClearsPtrValueInJava) {
  // Arrange.
  WindowScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtr();
  EXPECT_NE(nullptr, ptr);

  // Act: call Java destroy().
  InvokeJavaDestroyNativePtr();

  // Assert: the native pointer on the Java side should be set to null.
  ptr = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, ptr);
}

TEST_F(WindowScopedSidePanelRegistryBridgeUnitTest,
       SidePanelRegistryCanBeRetrievedFromBrowserWindow) {
  // Arrange.
  WindowScopedSidePanelRegistryBridge* bridge = InvokeJavaCreateNativePtr();
  SidePanelRegistry* expected_registry =
      bridge->GetSidePanelRegistryForTesting();
  EXPECT_NE(nullptr, expected_registry);

  // Act.
  SidePanelRegistry* actual_registry =
      SidePanelRegistry::From(mock_browser_.get());

  // Assert.
  EXPECT_EQ(expected_registry, actual_registry);
}

DEFINE_JNI(WindowScopedSidePanelRegistryBridgeNativeUnitTestSupport)
