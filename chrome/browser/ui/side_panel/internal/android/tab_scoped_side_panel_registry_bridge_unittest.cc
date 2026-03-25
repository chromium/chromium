// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/tab_scoped_side_panel_registry_bridge.h"

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel/test/android/native_unit_test_support_jni/TabScopedSidePanelRegistryBridgeNativeUnitTestSupport_jni.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using tabs::MockTabInterface;
}  // namespace

class TabScopedSidePanelRegistryBridgeUnitTest : public testing::Test {
 public:
  TabScopedSidePanelRegistryBridgeUnitTest() = default;
  ~TabScopedSidePanelRegistryBridgeUnitTest() override = default;

  void SetUp() override {
    SetUpJavaSupport();
    SetUpMockTab();
  }

  void TearDown() override { InvokeJavaDestroyNativePtr(); }

  TabScopedSidePanelRegistryBridge* InvokeJavaCreateNativePtrForTesting()
      const {
    return reinterpret_cast<TabScopedSidePanelRegistryBridge*>(
        Java_TabScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeCreateNativePtrForTesting(
            AttachCurrentThread(), java_test_support_,
            reinterpret_cast<int64_t>(mock_tab_.get())));
  }

  TabScopedSidePanelRegistryBridge* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<TabScopedSidePanelRegistryBridge*>(
        Java_TabScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaDestroyNativePtr() const {
    Java_TabScopedSidePanelRegistryBridgeNativeUnitTestSupport_invokeDestroyNativePtr(
        AttachCurrentThread(), java_test_support_);
  }

 protected:
  void SetUpJavaSupport() {
    java_test_support_ =
        Java_TabScopedSidePanelRegistryBridgeNativeUnitTestSupport_Constructor(
            AttachCurrentThread());
  }

  void SetUpMockTab() {
    mock_tab_ = std::make_unique<MockTabInterface>();
    ON_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
  }

  ScopedJavaGlobalRef<jobject> java_test_support_;
  std::unique_ptr<MockTabInterface> mock_tab_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(TabScopedSidePanelRegistryBridgeUnitTest,
       JavaCreateNativePtrMethodReturnsValidPtr) {
  TabScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtrForTesting();
  EXPECT_NE(nullptr, ptr);
}

TEST_F(TabScopedSidePanelRegistryBridgeUnitTest,
       JavaCreateNativePtrMethodCrashesIfCalledTwice) {
  TabScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtrForTesting();
  EXPECT_NE(nullptr, ptr);

  EXPECT_DEATH(InvokeJavaCreateNativePtrForTesting(), "");
}

TEST_F(TabScopedSidePanelRegistryBridgeUnitTest,
       JavaDestroyNativePtrMethodClearsPtrValueInJava) {
  // Arrange.
  TabScopedSidePanelRegistryBridge* ptr = InvokeJavaCreateNativePtrForTesting();
  EXPECT_NE(nullptr, ptr);

  // Act: call Java destroy().
  InvokeJavaDestroyNativePtr();

  // Assert: the native pointer on the Java side should be set to null.
  ptr = InvokeJavaGetNativePtrForTesting();
  EXPECT_EQ(nullptr, ptr);
}

TEST_F(TabScopedSidePanelRegistryBridgeUnitTest,
       SidePanelRegistryCanBeRetrievedFromTab) {
  // Arrange.
  TabScopedSidePanelRegistryBridge* bridge =
      InvokeJavaCreateNativePtrForTesting();
  SidePanelRegistry* expected_registry =
      bridge->GetSidePanelRegistryForTesting();
  EXPECT_NE(nullptr, expected_registry);

  // Act.
  SidePanelRegistry* actual_registry = SidePanelRegistry::From(mock_tab_.get());

  // Assert.
  EXPECT_EQ(expected_registry, actual_registry);
}

DEFINE_JNI(TabScopedSidePanelRegistryBridgeNativeUnitTestSupport)
