// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/mock_window_controller_list_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/android/extensions/windowing/test/native_unit_test_support_jni/ExtensionWindowControllerBridgeNativeUnitTestSupport_jni.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using testing::Return;
}  // namespace

class ExtensionWindowControllerBridgeUnitTest : public testing::Test {
 public:
  ExtensionWindowControllerBridgeUnitTest() = default;
  ~ExtensionWindowControllerBridgeUnitTest() override = default;

  void SetUp() override {
    java_test_support_.Reset(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_Constructor(
            AttachCurrentThread()));

    BrowserWindowInterface* browser = reinterpret_cast<BrowserWindowInterface*>(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_getNativeBrowserWindowPtr(
            AttachCurrentThread(), java_test_support_));

    test_tab_model_ = std::make_unique<TestTabModel>(browser->GetProfile());
    test_tab_model_->AssociateWithBrowserWindow(browser);
  }

  void TearDown() override {
    test_tab_model_.reset();

    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_tearDown(
        AttachCurrentThread(), java_test_support_);
  }

  void InvokeJavaOnAddedToTask() const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnAddedToTask(
        AttachCurrentThread(), java_test_support_);
  }

  void InvokeJavaOnTaskRemoved() const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnTaskRemoved(
        AttachCurrentThread(), java_test_support_);
  }

  void InvokeJavaOnTaskBoundsChanged() const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnTaskBoundsChanged(
        AttachCurrentThread(), java_test_support_);
  }

  ExtensionWindowControllerBridge* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<ExtensionWindowControllerBridge*>(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

 private:
  ScopedJavaGlobalRef<jobject> java_test_support_;

  std::unique_ptr<TestTabModel> test_tab_model_;
};

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       JavaOnAddedToTaskCreatesNativeObjects) {
  // Act.
  InvokeJavaOnAddedToTask();

  // Assert.
  ExtensionWindowControllerBridge* extension_window_controller_bridge =
      InvokeJavaGetNativePtrForTesting();
  const extensions::BrowserExtensionWindowController&
      extension_window_controller =
          extension_window_controller_bridge
              ->GetExtensionWindowControllerForTesting();
  EXPECT_NE(nullptr, extension_window_controller_bridge);
  EXPECT_NE(SessionID::InvalidValue().id(),
            extension_window_controller.GetWindowId());
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

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       JavaOnTaskBoundsChangedNotifiesExtensionWindowController) {
  // Arrange: add a mock WindowControllerListObserver for setting up
  // expectations.
  auto mock_window_list_observer =
      extensions::MockWindowControllerListObserver();
  extensions::WindowControllerList::GetInstance()->AddObserver(
      &mock_window_list_observer);

  // Arrange: create ExtensionWindowControllerBridge (Java + native).
  InvokeJavaOnAddedToTask();
  ExtensionWindowControllerBridge* extension_window_controller_bridge =
      InvokeJavaGetNativePtrForTesting();
  const extensions::BrowserExtensionWindowController&
      browser_extension_window_controller =
          extension_window_controller_bridge
              ->GetExtensionWindowControllerForTesting();

  // Arrange: set up expectations.
  extensions::WindowController& window_controller =
      const_cast<extensions::BrowserExtensionWindowController&>(
          browser_extension_window_controller);
  EXPECT_CALL(mock_window_list_observer,
              OnWindowBoundsChanged(&window_controller))
      .Times(1)
      .WillRepeatedly(Return());

  // Act.
  InvokeJavaOnTaskBoundsChanged();

  // Cleanup
  extensions::WindowControllerList::GetInstance()->RemoveObserver(
      &mock_window_list_observer);
}
