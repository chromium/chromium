// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_deref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindow_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

// Helper to get the Profile from the Java side.
Profile* GetProfileFromJava(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_android_browser_window) {
  base::android::ScopedJavaLocalRef<jobject> j_profile =
      Java_AndroidBrowserWindow_getProfile(env, java_android_browser_window);
  CHECK(!j_profile.is_null())
      << "AndroidBrowserWindow is for desktop Android, which assumes that the "
         "associated profile will never be null for the lifetime of the "
         "window. See documentation of BrowserWindowInterface::GetProfile() "
         "for details.";
  return Profile::FromJavaObject(j_profile);
}

}  // namespace

// Implements Java |AndroidBrowserWindow.Natives#create|.
static jlong JNI_AndroidBrowserWindow_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    jint browser_window_type) {
  Profile* profile = GetProfileFromJava(env, caller);
  return reinterpret_cast<intptr_t>(new AndroidBrowserWindow(
      env, caller,
      static_cast<BrowserWindowInterface::Type>(browser_window_type), profile));
}

AndroidBrowserWindow::AndroidBrowserWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_android_browser_window,
    const BrowserWindowInterface::Type type,
    Profile* profile)
    : type_(type),
      profile_(CHECK_DEREF(profile)),
      session_id_(SessionID::NewUnique()) {
  java_android_browser_window_.Reset(env, java_android_browser_window);
}

AndroidBrowserWindow::~AndroidBrowserWindow() {
  Java_AndroidBrowserWindow_clearNativePtr(AttachCurrentThread(),
                                           java_android_browser_window_);
}

void AndroidBrowserWindow::Destroy(JNIEnv* env) {
  delete this;
}

ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost() {
  return unowned_user_data_host_;
}

const ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost()
    const {
  return unowned_user_data_host_;
}

ui::BaseWindow* AndroidBrowserWindow::GetWindow() {
  return reinterpret_cast<ui::BaseWindow*>(
      Java_AndroidBrowserWindow_getOrCreateNativeBaseWindowPtr(
          AttachCurrentThread(), java_android_browser_window_));
}

Profile* AndroidBrowserWindow::GetProfile() {
  return const_cast<Profile*>(
      static_cast<const AndroidBrowserWindow*>(this)->GetProfile());
}

const Profile* AndroidBrowserWindow::GetProfile() const {
  JNIEnv* env = AttachCurrentThread();
  Profile* current_profile_ptr =
      GetProfileFromJava(env, java_android_browser_window_);
  Profile* cached_profile_ptr = &profile_.get();
  CHECK(cached_profile_ptr == current_profile_ptr)
      << "AndroidBrowserWindow is for desktop Android, which assumes a single "
         "Profile for a given window. See documentation of "
         "BrowserWindowInterface::GetProfile() for details.";
  return cached_profile_ptr;
}

const SessionID& AndroidBrowserWindow::GetSessionID() const {
  return session_id_;
}

BrowserWindowInterface::Type AndroidBrowserWindow::GetType() const {
  return type_;
}

content::WebContents* AndroidBrowserWindow::OpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jobject> AndroidBrowserWindow::GetActivity() {
  return Java_AndroidBrowserWindow_getActivity(AttachCurrentThread(),
                                               java_android_browser_window_);
}
