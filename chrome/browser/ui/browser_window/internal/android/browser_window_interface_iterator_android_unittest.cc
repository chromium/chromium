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
            base::android::AttachCurrentThread(), task_id));
  }

  void DestroyBrowserWindow(int task_id) {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
        base::android::AttachCurrentThread(), task_id);
  }

  void TearDown() override {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyAllBrowserWindows(
        base::android::AttachCurrentThread());
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
