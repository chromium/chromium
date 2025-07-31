// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindow_jni.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

std::vector<BrowserWindowInterface*>& GetAndroidWindowList() {
  static base::NoDestructor<std::vector<BrowserWindowInterface*>> list;
  return *list;
}

}  // namespace

// Implements Java |AndroidBrowserWindow.Natives#create|.
static jlong JNI_AndroidBrowserWindow_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new AndroidBrowserWindow(env, caller));
}

AndroidBrowserWindow::AndroidBrowserWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_android_browser_window)
    : session_id_(SessionID::NewUnique()) {
  java_android_browser_window_.Reset(env, java_android_browser_window);
  GetAndroidWindowList().push_back(this);
}

AndroidBrowserWindow::~AndroidBrowserWindow() {
  Java_AndroidBrowserWindow_clearNativePtr(AttachCurrentThread(),
                                           java_android_browser_window_);
  std::vector<BrowserWindowInterface*>& all_windows = GetAndroidWindowList();
  auto iter = std::find(all_windows.begin(), all_windows.end(), this);
  CHECK(iter != all_windows.end());
  all_windows.erase(iter);
}

// static
std::vector<BrowserWindowInterface*>
AndroidBrowserWindow::GetAllAndroidBrowserWindowsByCreationTime() {
  return GetAndroidWindowList();
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
  // TODO(crbug.com/429037015): Return a proper Profile.
  // Temporarily return nullptr to avoid crashing callers.
  return nullptr;
}

const SessionID& AndroidBrowserWindow::GetSessionID() const {
  return session_id_;
}

content::WebContents* AndroidBrowserWindow::OpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  NOTREACHED();
}
