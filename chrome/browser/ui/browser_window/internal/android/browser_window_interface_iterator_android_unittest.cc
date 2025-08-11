// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;

bool VectorContains(const std::vector<BrowserWindowInterface*>& vector,
                    const BrowserWindowInterface* value) {
  return std::find(vector.begin(), vector.end(), value) != vector.end();
}
}  // namespace

class BrowserWindowInterfaceIteratorAndroidUnitTest : public testing::Test {
 public:
  BrowserWindowInterfaceIteratorAndroidUnitTest() = default;
  ~BrowserWindowInterfaceIteratorAndroidUnitTest() override = default;

  BrowserWindowInterface* CreateBrowserWindow(int task_id) {
    return reinterpret_cast<BrowserWindowInterface*>(
        Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
            AttachCurrentThread(), task_id));
  }

  void InvokeOnTopResumedActivityChanged(int task_id,
                                         bool is_top_resumed_activity) {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_invokeOnTopResumedActivityChanged(
        AttachCurrentThread(), task_id, is_top_resumed_activity);
  }

  void DestroyBrowserWindow(int task_id) {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
        AttachCurrentThread(), task_id);
  }

  void TearDown() override {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyAllBrowserWindows(
        AttachCurrentThread());
  }
};

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       GetAllBrowserWindowInterfacesReturnsEmptyVectorWhenNoWindowsExist) {
  std::vector<BrowserWindowInterface*> browser_windows =
      GetAllBrowserWindowInterfaces();
  EXPECT_TRUE(browser_windows.empty());
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       GetAllBrowserWindowInterfacesReturnsAllWindows) {
  BrowserWindowInterface* browser_window1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window2 = CreateBrowserWindow(/*task_id=*/2);

  std::vector<BrowserWindowInterface*> browser_windows =
      GetAllBrowserWindowInterfaces();

  EXPECT_EQ(2u, browser_windows.size());
  EXPECT_TRUE(VectorContains(browser_windows, browser_window1));
  EXPECT_TRUE(VectorContains(browser_windows, browser_window2));
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       GetAllBrowserWindowInterfacesDoesNotReturnDestroyedWindows) {
  CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window2 = CreateBrowserWindow(/*task_id=*/2);
  CreateBrowserWindow(/*task_id=*/3);
  DestroyBrowserWindow(/*task_id=*/1);
  DestroyBrowserWindow(/*task_id=*/3);

  std::vector<BrowserWindowInterface*> browser_windows =
      GetAllBrowserWindowInterfaces();

  EXPECT_EQ(1u, browser_windows.size());
  EXPECT_TRUE(VectorContains(browser_windows, browser_window2));
}

TEST_F(
    BrowserWindowInterfaceIteratorAndroidUnitTest,
    GetBrowserWindowInterfaceOrderedByActivationReturnsEmptyVectorWhenNoWindowsExist) {
  std::vector<BrowserWindowInterface*> browser_windows_ordered_by_activation =
      GetBrowserWindowInterfacesOrderedByActivation();
  EXPECT_TRUE(browser_windows_ordered_by_activation.empty());
}

TEST_F(
    BrowserWindowInterfaceIteratorAndroidUnitTest,
    GetBrowserWindowInterfacesOrderedByActivationReturnsCorrectlyOrderedWindows) {
  // Arrange:
  // Create 2 windows, and simulate Android system's behavior regarding
  // calls to |OnTopResumedActivityChanged|
  BrowserWindowInterface* browser_window1 = CreateBrowserWindow(/*task_id=*/1);
  InvokeOnTopResumedActivityChanged(/*task_id=*/1,
                                    /*is_top_resumed_activity=*/true);
  BrowserWindowInterface* browser_window2 = CreateBrowserWindow(/*task_id=*/2);
  InvokeOnTopResumedActivityChanged(/*task_id=*/1,
                                    /*is_top_resumed_activity=*/false);
  InvokeOnTopResumedActivityChanged(/*task_id=*/2,
                                    /*is_top_resumed_activity=*/true);

  std::vector<BrowserWindowInterface*> browser_windows_ordered_by_activation =
      GetBrowserWindowInterfacesOrderedByActivation();

  EXPECT_EQ(2u, browser_windows_ordered_by_activation.size());
  EXPECT_EQ(browser_window2, browser_windows_ordered_by_activation[0]);
  EXPECT_EQ(browser_window1, browser_windows_ordered_by_activation[1]);
}

TEST_F(
    BrowserWindowInterfaceIteratorAndroidUnitTest,
    GetLastActiveBrowserWindowInterfaceWithAnyProfileReturnsNullWhenNoWindowsExist) {
  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  EXPECT_EQ(nullptr, browser_window);
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       GetLastActiveBrowserWindowInterfaceWithAnyProfileReturnsCorrectWindow) {
  // Arrange:
  // Create 2 windows, and simulate Android system's behavior regarding
  // calls to |OnTopResumedActivityChanged|
  CreateBrowserWindow(/*task_id=*/1);
  InvokeOnTopResumedActivityChanged(/*task_id=*/1,
                                    /*is_top_resumed_activity=*/true);
  BrowserWindowInterface* browser_window2 = CreateBrowserWindow(/*task_id=*/2);
  InvokeOnTopResumedActivityChanged(/*task_id=*/1,
                                    /*is_top_resumed_activity=*/false);
  InvokeOnTopResumedActivityChanged(/*task_id=*/2,
                                    /*is_top_resumed_activity=*/true);

  BrowserWindowInterface* last_active_browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  EXPECT_EQ(browser_window2, last_active_browser_window);
}
