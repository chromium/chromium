// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window_enumerator.h"

#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/AndroidBrowserWindowEnumeratorNativeUnitTestSupport_jni.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Heavily based on chrome/browser/ui/browser_list_enumerator_browsertest.h.
class AndroidBrowserWindowEnumeratorTest : public ::testing::Test {
 public:
  AndroidBrowserWindowEnumeratorTest() = default;
  ~AndroidBrowserWindowEnumeratorTest() override = default;

  void TearDown() override {
    Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_destroyAllBrowserWindows(
        base::android::AttachCurrentThread());
  }

 protected:
  BrowserWindowInterface* CreateBrowserWindow(int task_id) {
    AndroidBrowserWindow* browser_window = reinterpret_cast<
        AndroidBrowserWindow*>(
        Java_AndroidBrowserWindowEnumeratorNativeUnitTestSupport_createBrowserWindow(
            base::android::AttachCurrentThread(), task_id));

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
};

TEST_F(AndroidBrowserWindowEnumeratorTest, EmptyEnumerator) {
  AndroidBrowserWindowEnumerator enumerator;
  EXPECT_TRUE(enumerator.empty());
}

TEST_F(AndroidBrowserWindowEnumeratorTest, BasicIterator) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  std::set<BrowserWindowInterface*> visited;
  AndroidBrowserWindowEnumerator enumerator;
  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(browser_window_1, browser_window_2,
                                            browser_window_3));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithInsertions) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = true;
  AndroidBrowserWindowEnumerator enumerator(kEnumerateNewBrowser);
  std::set<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(browser_window_1, browser_window_2,
                                            browser_window_3));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithSkipInsertions) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  BrowserWindowInterface* browser_window_2 = CreateBrowserWindow(/*task_id=*/2);

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = false;
  AndroidBrowserWindowEnumerator enumerator(kEnumerateNewBrowser);
  std::set<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  CreateBrowserWindow(/*task_id=*/3);

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited, testing::UnorderedElementsAre(browser_window_1,
                                                     browser_window_2));
}

TEST_F(AndroidBrowserWindowEnumeratorTest, IteratorWithRemovals) {
  BrowserWindowInterface* browser_window_1 = CreateBrowserWindow(/*task_id=*/1);
  CreateBrowserWindow(/*task_id=*/2);
  BrowserWindowInterface* browser_window_3 = CreateBrowserWindow(/*task_id=*/3);

  // Start to scan the list.
  AndroidBrowserWindowEnumerator enumerator;
  std::set<BrowserWindowInterface*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Remove a browser while the list is scanned.
  DestroyBrowserWindow(2);

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited, testing::UnorderedElementsAre(browser_window_1,
                                                     browser_window_3));
}
