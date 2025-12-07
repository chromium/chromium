// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/mock_window_controller_list_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/android/extensions/windowing/test/native_unit_test_support_jni/ExtensionWindowControllerBridgeNativeUnitTestSupport_jni.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
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
    SetUpProfile();
    java_test_support_.Reset(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_Constructor(
            AttachCurrentThread()));

    BrowserWindowInterface* browser = reinterpret_cast<BrowserWindowInterface*>(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_getNativeBrowserWindowPtr(
            AttachCurrentThread(), java_test_support_));

    test_tab_model_ = std::make_unique<TestTabModel>(browser->GetProfile());
    test_tab_model_->AssociateWithBrowserWindow(browser);
  }

  void SetUpProfile() {
    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));
    base::FilePath profile_path =
        profile_manager()->user_data_dir().AppendASCII("test-profile");
    profile_ = static_cast<TestingProfile*>(
        profile_manager()->GetProfile(profile_path));
  }

  void TearDown() override {
    test_tab_model_.reset();

    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_tearDown(
        AttachCurrentThread(), java_test_support_);
    TearDownProfile();
  }

  void TearDownProfile() {
    TestingBrowserProcess::DeleteInstance();
    task_environment_.reset();
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

  void InvokeJavaOnTaskFocusChanged(bool has_focus) const {
    Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeOnTaskFocusChanged(
        AttachCurrentThread(), java_test_support_, has_focus);
  }

  ExtensionWindowControllerBridge* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<ExtensionWindowControllerBridge*>(
        Java_ExtensionWindowControllerBridgeNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

 private:
  FakeProfileManager* profile_manager() {
    return static_cast<FakeProfileManager*>(
        g_browser_process->profile_manager());
  }

  base::ScopedTempDir temp_dir_;
  // Necessary to use FakeProfileManager and TestingProfile. See
  // docs/threading_and_tasks_testing.md.
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  raw_ptr<TestingProfile> profile_;
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

TEST_F(ExtensionWindowControllerBridgeUnitTest,
       JavaOnTaskFocusChangedNotifiesExtensionWindowController) {
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
              OnWindowFocusChanged(&window_controller, /*has_focus=*/true))
      .Times(1)
      .WillRepeatedly(Return());

  // Act.
  InvokeJavaOnTaskFocusChanged(/*has_focus=*/true);

  // Cleanup
  extensions::WindowControllerList::GetInstance()->RemoveObserver(
      &mock_window_list_observer);
}

DEFINE_JNI(ExtensionWindowControllerBridgeNativeUnitTestSupport)
