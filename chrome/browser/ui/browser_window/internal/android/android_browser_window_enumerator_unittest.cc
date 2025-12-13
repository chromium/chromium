// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window_enumerator.h"

#include <vector>

#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/AndroidBrowserWindowEnumeratorNativeUnitTestSupport_jni.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

std::vector<BrowserWindowInterface*>
GetBrowserWindowInterfacesOrderedByActivationForTesting();

// Heavily based on chrome/browser/ui/browser_list_enumerator_browsertest.h.
class AndroidBrowserWindowEnumeratorTest : public ::testing::Test {
 public:
  AndroidBrowserWindowEnumeratorTest() = default;
  ~AndroidBrowserWindowEnumeratorTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override {
    Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_destroyAllBrowserWindows(
        base::android::AttachCurrentThread());
  }

 protected:
  BrowserWindowInterface* CreateBrowserWindow(int task_id) {
    AndroidBrowserWindow* browser_window = reinterpret_cast<
        AndroidBrowserWindow*>(
        Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_createBrowserWindow(
            base::android::AttachCurrentThread(), task_id,
            profile_->GetJavaObject()));

    // This is needed to avoid the `assert lastActivatedTimeMillis > 0;` in
    // ChromeAndroidTaskImpl.Java:getLastActivatedTimeMillis().
    Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_activateBrowserWindow(
        base::android::AttachCurrentThread(), task_id);

    return browser_window;
  }

  void DestroyBrowserWindow(int task_id) {
    Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_destroyBrowserWindow(
        base::android::AttachCurrentThread(), task_id);
  }

 private:
  // Necessary to use TestingProfile.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AndroidBrowserWindowEnumeratorTest, EmptyEnumerator) {
  AndroidBrowserWindowEnumerator enumerator(
      GetBrowserWindowInterfacesOrderedByActivationForTesting());
  EXPECT_TRUE(enumerator.empty());
}

TEST_F(AndroidBrowserWindowEnumeratorTest, BasicIterator) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  std::vector<BrowserWindowInterface*> visited;
  AndroidBrowserWindowEnumerator enumerator(
      GetBrowserWindowInterfacesOrderedByActivationForTesting());
  while (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  // In this test, windows are activated when they are created. Therefore
  // browser_window_3 is the most recently activated, followed by
  // browser_window_2, then browser_window_1.
  EXPECT_THAT(visited, testing::ElementsAre(browser_window_3, browser_window_2,
                                            browser_window_1));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithInsertions) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = true;
  AndroidBrowserWindowEnumerator enumerator(
      GetBrowserWindowInterfacesOrderedByActivationForTesting(),
      kEnumerateNewBrowser);
  std::vector<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  while (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  // As above, windows are activated in creation order. So browser_window_2 is
  // more recently activated than browser_window_1, and thus appears first in
  // the list. browser_window_3, however, was created after |enumerator|, and it
  // therefore gets appended to the end of the list, despite having been
  // activated most recently.
  EXPECT_THAT(visited, testing::ElementsAre(browser_window_2, browser_window_1,
                                            browser_window_3));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithSkipInsertions) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = false;
  AndroidBrowserWindowEnumerator enumerator(
      GetBrowserWindowInterfacesOrderedByActivationForTesting(),
      kEnumerateNewBrowser);
  std::vector<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  CreateBrowserWindow(/*task_id=*/3);

  while (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::ElementsAre(browser_window_2, browser_window_1));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithRemovals) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  // Start to scan the list.
  AndroidBrowserWindowEnumerator enumerator(
      GetBrowserWindowInterfacesOrderedByActivationForTesting());
  std::vector<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  // Remove a browser while the list is scanned.
  DestroyBrowserWindow(2);

  while (!enumerator.empty()) {
    visited.push_back(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::ElementsAre(browser_window_3, browser_window_1));
}

DEFINE_JNI(AndroidBrowserWindowEnumeratorNativeUnitTestSupport)
