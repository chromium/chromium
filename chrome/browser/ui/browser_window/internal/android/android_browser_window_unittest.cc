// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/android/android_base_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/AndroidBrowserWindowNativeUnitTestSupport_jni.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

class AndroidBrowserWindowUnitTest : public testing::Test {
 public:
  AndroidBrowserWindowUnitTest() = default;
  ~AndroidBrowserWindowUnitTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    SetProfileManager();
    java_test_support_ =
        Java_AndroidBrowserWindowNativeUnitTestSupport_Constructor(
            AttachCurrentThread(), GetLastUsedProfile()->GetJavaObject());
  }

  void TearDown() override {
    SetProfileManager(nullptr);
    InvokeJavaResetAndDestroy();
  }

  AndroidBrowserWindow* InvokeJavaGetOrCreateNativePtr() const {
    return reinterpret_cast<AndroidBrowserWindow*>(
        Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetOrCreateNativePtr(
            AttachCurrentThread(), java_test_support_));
  }

  AndroidBaseWindow* InvokeJavaGetOrCreateNativeBaseWindowPtr() const {
    return reinterpret_cast<AndroidBaseWindow*>(
        Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetOrCreateNativeBaseWindowPtr(
            AttachCurrentThread(), java_test_support_));
  }

  AndroidBrowserWindow* InvokeJavaGetNativePtrForTesting() const {
    return reinterpret_cast<AndroidBrowserWindow*>(
        Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetNativePtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  AndroidBaseWindow* InvokeJavaGetNativeBaseWindowPtrForTesting() const {
    return reinterpret_cast<AndroidBaseWindow*>(
        Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetNativeBaseWindowPtrForTesting(
            AttachCurrentThread(), java_test_support_));
  }

  void InvokeJavaResetAndDestroy() {
    Java_AndroidBrowserWindowNativeUnitTestSupport_invokeResetAndDestroy(
        AttachCurrentThread(), java_test_support_);
  }

  void InvokeJavaSetProfileForTesting(Profile* profile) const {
    Java_AndroidBrowserWindowNativeUnitTestSupport_setProfileForTesting(
        AttachCurrentThread(), java_test_support_, profile->GetJavaObject());
  }

 protected:
  Profile* GetLastUsedProfile() {
    return profile_manager()->GetProfile(
        profile_manager()->GetLastUsedProfileDir());
  }

  void SetProfileManager() {
    SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));
  }

  void SetProfileManager(std::unique_ptr<ProfileManager> profile_manager) {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager));
  }

 private:
  inline FakeProfileManager* profile_manager() const {
    return static_cast<FakeProfileManager*>(
        g_browser_process->profile_manager());
  }
  base::ScopedTempDir temp_dir_;
  // Necessary to use Profile and FakeProfileManager. See
  // docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;
  ScopedJavaGlobalRef<jobject> java_test_support_;
};

TEST_F(AndroidBrowserWindowUnitTest,
       JavaGetOrCreateNativePtrMethodReturnsSamePtr) {
  // Arrange & Act: call Java GetOrCreateNativePtr() twice.
  AndroidBrowserWindow* ptr1 = InvokeJavaGetOrCreateNativePtr();
  AndroidBrowserWindow* ptr2 = InvokeJavaGetOrCreateNativePtr();

  // Assert: the two calls should return the same non-null pointer.
  EXPECT_NE(nullptr, ptr1);
  EXPECT_NE(nullptr, ptr2);
  EXPECT_EQ(ptr1, ptr2);
}

TEST_F(AndroidBrowserWindowUnitTest,
       JavaDestroyMethodClearsBrowserWindowAndBaseWindowPtrValuesInJava) {
  // Arrange.
  InvokeJavaGetOrCreateNativePtr();
  InvokeJavaGetOrCreateNativeBaseWindowPtr();

  // Act: call Java destroy().
  InvokeJavaResetAndDestroy();

  // Assert: the native pointers on the Java side should be set to null.
  AndroidBrowserWindow* android_browser_window =
      InvokeJavaGetNativePtrForTesting();
  AndroidBaseWindow* android_base_window =
      InvokeJavaGetNativeBaseWindowPtrForTesting();
  EXPECT_EQ(nullptr, android_browser_window);
  EXPECT_EQ(nullptr, android_base_window);
}

TEST_F(AndroidBrowserWindowUnitTest, GetWindowReturnsAndroidBaseWindow) {
  // Arrange.
  AndroidBrowserWindow* android_browser_window =
      InvokeJavaGetOrCreateNativePtr();

  // Act.
  ui::BaseWindow* base_window = android_browser_window->GetWindow();

  // Assert.
  AndroidBaseWindow* expected_base_window =
      InvokeJavaGetNativeBaseWindowPtrForTesting();
  EXPECT_EQ(expected_base_window, base_window);
}

TEST_F(AndroidBrowserWindowUnitTest, GetSessionIDReturnsUniqueID) {
  // Arrange: create two AndroidBrowserWindow objects.
  //
  // As each Java AndroidBrowserWindowNativeUnitTestSupport owns one native
  // AndroidBrowserWindow object, we need to create two Java test support
  // objects to get two instances of AndroidBrowserWindow.
  //
  // For clarity, we don't use the test fixture's java_test_support_ field.
  Profile* profile = GetLastUsedProfile();
  ScopedJavaLocalRef<jobject> java_test_support1 =
      Java_AndroidBrowserWindowNativeUnitTestSupport_Constructor(
          AttachCurrentThread(), profile->GetJavaObject());
  ScopedJavaLocalRef<jobject> java_test_support2 =
      Java_AndroidBrowserWindowNativeUnitTestSupport_Constructor(
          AttachCurrentThread(), profile->GetJavaObject());

  AndroidBrowserWindow* android_browser_window1 = reinterpret_cast<
      AndroidBrowserWindow*>(
      Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetOrCreateNativePtr(
          AttachCurrentThread(), java_test_support1));
  AndroidBrowserWindow* android_browser_window2 = reinterpret_cast<
      AndroidBrowserWindow*>(
      Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetOrCreateNativePtr(
          AttachCurrentThread(), java_test_support2));

  const SessionID& session_id1 = android_browser_window1->GetSessionID();
  const SessionID& session_id2 = android_browser_window2->GetSessionID();

  EXPECT_NE(session_id1, session_id2);

  // Clean up.
  Java_AndroidBrowserWindowNativeUnitTestSupport_invokeResetAndDestroy(
      AttachCurrentThread(), java_test_support1);
  Java_AndroidBrowserWindowNativeUnitTestSupport_invokeResetAndDestroy(
      AttachCurrentThread(), java_test_support2);
}

TEST_F(AndroidBrowserWindowUnitTest, GetTypeReturnsBrowserWindowInterfaceType) {
  // Arrange.
  Profile* profile = GetLastUsedProfile();
  ScopedJavaLocalRef<jobject> java_test_support =
      Java_AndroidBrowserWindowNativeUnitTestSupport_Constructor(
          AttachCurrentThread(), BrowserWindowInterface::TYPE_POPUP,
          profile->GetJavaObject());
  AndroidBrowserWindow* android_browser_window = reinterpret_cast<
      AndroidBrowserWindow*>(
      Java_AndroidBrowserWindowNativeUnitTestSupport_invokeGetOrCreateNativePtr(
          AttachCurrentThread(), java_test_support));

  // Assert.
  EXPECT_EQ(BrowserWindowInterface::TYPE_POPUP,
            android_browser_window->GetType());

  // Clean up.
  Java_AndroidBrowserWindowNativeUnitTestSupport_invokeResetAndDestroy(
      AttachCurrentThread(), java_test_support);
}

TEST_F(AndroidBrowserWindowUnitTest, GetProfileReturnsCorrectProfile) {
  // Arrange.
  AndroidBrowserWindow* android_browser_window =
      InvokeJavaGetOrCreateNativePtr();
  Profile* expected_profile = GetLastUsedProfile();

  // Act.
  const Profile* actual_profile = android_browser_window->GetProfile();

  // Assert.
  EXPECT_EQ(expected_profile, actual_profile);
}

DEFINE_JNI(AndroidBrowserWindowNativeUnitTestSupport)
