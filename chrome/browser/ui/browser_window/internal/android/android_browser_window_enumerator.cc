// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window_enumerator.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindowEnumerator_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

AndroidBrowserWindowEnumerator::AndroidBrowserWindowEnumerator(
    std::vector<BrowserWindowInterface*> browser_windows,
    bool enumerate_new_browser_windows)
    : browser_windows_(browser_windows) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_enumerator_.Reset(Java_AndroidBrowserWindowEnumerator_create(
      env, reinterpret_cast<int64_t>(this), enumerate_new_browser_windows));
}

AndroidBrowserWindowEnumerator::~AndroidBrowserWindowEnumerator() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AndroidBrowserWindowEnumerator_destroy(env, j_enumerator_);
}

BrowserWindowInterface* AndroidBrowserWindowEnumerator::Next() {
  BrowserWindowInterface* browser_window = browser_windows_.front();
  browser_windows_.erase(browser_windows_.begin());
  return browser_window;
}

void AndroidBrowserWindowEnumerator::OnBrowserWindowAdded(
    JNIEnv* env,
    jlong j_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr);
  DCHECK(!base::Contains(browser_windows_, browser_window));
  browser_windows_.push_back(browser_window);
}

void AndroidBrowserWindowEnumerator::OnBrowserWindowRemoved(
    JNIEnv* env,
    jlong j_browser_window_ptr) {
  std::erase(browser_windows_,
             reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr));
}

DEFINE_JNI(AndroidBrowserWindowEnumerator)
