// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_jni.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using base::android::AttachCurrentThread;

bool VectorContains(const std::vector<BrowserWindowInterface*>& vector,
                    const BrowserWindowInterface* value) {
  return std::find(vector.begin(), vector.end(), value) != vector.end();
}
}  // namespace

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivationForTesting();

class BrowserWindowInterfaceIteratorAndroidUnitTest : public testing::Test {
 public:
  BrowserWindowInterfaceIteratorAndroidUnitTest() = default;
  ~BrowserWindowInterfaceIteratorAndroidUnitTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  BrowserWindowInterface* CreateBrowserWindow(int task_id) {
    BrowserWindowInterface* browser_window = reinterpret_cast<
        BrowserWindowInterface*>(
        Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
            AttachCurrentThread(), task_id, profile_->GetJavaObject()));

    // This is needed to avoid the `assert lastActivatedTimeMillis > 0;` in
    // ChromeAndroidTaskImpl.Java:getLastActivatedTimeMillis().
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_activateBrowserWindow(
        base::android::AttachCurrentThread(), task_id);

    return browser_window;
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

 private:
  // Necessary to use TestingProfile. See docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
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
      GetBrowserWindowInterfacesOrderedByActivationForTesting();
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
      GetBrowserWindowInterfacesOrderedByActivationForTesting();

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

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       ForEachCurrentBrowserWindowInterfaceOrderedByActivationEmpty) {
  bool was_called = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        was_called = true;
        return true;
      });
  EXPECT_FALSE(was_called);
}

TEST_F(
    BrowserWindowInterfaceIteratorAndroidUnitTest,
    ForEachCurrentBrowserWindowInterfaceOrderedByActivationStopsIteratingWhenFalseIsReturned) {
  CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  CreateBrowserWindow(/*task_id=*/3);

  int i = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        i++;
        return i == 1;
      });

  EXPECT_EQ(i, 2);
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       ForEachCurrentBrowserWindowInterfaceOrderedByActivationAddRemove) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  std::vector<BrowserWindowInterface*> visited;
  int i = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        visited.push_back(browser_window);

        if (i == 0) {
          // Create a browser while the list is scanned.
          CreateBrowserWindow(4);

          // Remove a browser while the list is scanned.
          DestroyBrowserWindow(2);
        }

        i++;
        return true;
      });

  // In this test, windows are activated when they are created. Therefore
  // browser_window_3 is the most recently activated, followed by the would-be
  // browser_window_2, then browser_window_1. However, "browser_window_2" was
  // removed during iteration, so it won't appear in the output. The would-be
  // browser_window_4 is not added because this is
  // ForEachCurrentBrowserWindowInterfaceOrderedByActivation(), not
  // ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation().
  EXPECT_THAT(visited,
              testing::ElementsAre(browser_window_3, browser_window_1));
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationEmpty) {
  bool was_called = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        was_called = true;
        return true;
      });
  EXPECT_FALSE(was_called);
}

TEST_F(
    BrowserWindowInterfaceIteratorAndroidUnitTest,
    ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationStopsIteratingWhenFalseIsReturned) {
  CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  CreateBrowserWindow(/*task_id=*/3);

  int i = 0;
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        i++;
        return i == 1;
      });

  EXPECT_EQ(i, 2);
}

TEST_F(BrowserWindowInterfaceIteratorAndroidUnitTest,
       ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationAddRemove) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);
  BrowserWindowInterface* browser_window_4 = nullptr;

  std::vector<BrowserWindowInterface*> visited;
  int i = 0;
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        visited.push_back(browser_window);

        if (i == 0) {
          // Create a browser while the list is scanned.
          browser_window_4 = CreateBrowserWindow(4);

          // Remove a browser while the list is scanned.
          DestroyBrowserWindow(2);
        }

        i++;
        return true;
      });

  // In this test, windows are activated when they are created. Therefore
  // browser_window_3 is the most recently activated, followed by the would-be
  // browser_window_2, then browser_window_1. However, "browser_window_2" was
  // removed during iteration, so it won't appear in the output, and
  // browser_window_4 is appended to the end of the iteration since it was added
  // in the middle.
  EXPECT_THAT(visited, testing::ElementsAre(browser_window_3, browser_window_1,
                                            browser_window_4));
}

DEFINE_JNI(BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport)
