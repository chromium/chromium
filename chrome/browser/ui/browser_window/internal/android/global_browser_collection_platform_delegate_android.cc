// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"

#include "chrome/browser/ui/browser_window/internal/jni/GlobalBrowserCollectionPlatformDelegate_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

GlobalBrowserCollectionPlatformDelegate::
    GlobalBrowserCollectionPlatformDelegate(GlobalBrowserCollection& parent)
    : parent_(parent) {
  JNIEnv* env = base::android::AttachCurrentThread();
  j_delegate_.Reset(Java_GlobalBrowserCollectionPlatformDelegate_create(
      env, reinterpret_cast<int64_t>(this)));
}

GlobalBrowserCollectionPlatformDelegate::
    ~GlobalBrowserCollectionPlatformDelegate() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlobalBrowserCollectionPlatformDelegate_destroy(env, j_delegate_);
}

void GlobalBrowserCollectionPlatformDelegate::OnBrowserCreated(
    JNIEnv* env,
    int64_t j_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr);
  parent_->OnBrowserCreated(browser_window);
}

void GlobalBrowserCollectionPlatformDelegate::OnBrowserClosed(
    JNIEnv* env,
    int64_t j_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr);
  parent_->OnBrowserClosed(browser_window);
}

// TODO(crbug.com/474120522): Call this from Java.
void GlobalBrowserCollectionPlatformDelegate::OnBrowserActivated(
    JNIEnv* env,
    int64_t j_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr);
  parent_->OnBrowserActivated(browser_window);
}

// TODO(crbug.com/474120522): Call this from Java.
void GlobalBrowserCollectionPlatformDelegate::OnBrowserDeactivated(
    JNIEnv* env,
    int64_t j_browser_window_ptr) {
  BrowserWindowInterface* browser_window =
      reinterpret_cast<BrowserWindowInterface*>(j_browser_window_ptr);
  parent_->OnBrowserDeactivated(browser_window);
}

DEFINE_JNI(GlobalBrowserCollectionPlatformDelegate)
