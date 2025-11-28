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

}  // namespace

// Implements Java |AndroidBrowserWindow.Natives#create|.
static jlong JNI_AndroidBrowserWindow_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    jint browser_window_type,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
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

jint AndroidBrowserWindow::GetSessionIdForTesting(JNIEnv* env) const {
  return GetSessionID().id();
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

const ui::BaseWindow* AndroidBrowserWindow::GetWindow() const {
  return reinterpret_cast<ui::BaseWindow*>(
      Java_AndroidBrowserWindow_getOrCreateNativeBaseWindowPtr(
          AttachCurrentThread(), java_android_browser_window_));
}

Profile* AndroidBrowserWindow::GetProfile() {
  return const_cast<Profile*>(
      static_cast<const AndroidBrowserWindow*>(this)->GetProfile());
}

const Profile* AndroidBrowserWindow::GetProfile() const {
  return &profile_.get();
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

DEFINE_JNI(AndroidBrowserWindow)
