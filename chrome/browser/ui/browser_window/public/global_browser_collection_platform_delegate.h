// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#else
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include "base/memory/raw_ref.h"

class GlobalBrowserCollection;

class GlobalBrowserCollectionPlatformDelegate final
#if !BUILDFLAG(IS_ANDROID)
    : public BrowserCollectionObserver
#endif  // !BUILDFLAG(IS_ANDROID)
{
 public:
  explicit GlobalBrowserCollectionPlatformDelegate(
      GlobalBrowserCollection& parent);
  GlobalBrowserCollectionPlatformDelegate(
      const GlobalBrowserCollectionPlatformDelegate&) = delete;
  GlobalBrowserCollectionPlatformDelegate& operator=(
      const GlobalBrowserCollectionPlatformDelegate&) = delete;
#if BUILDFLAG(IS_ANDROID)
  ~GlobalBrowserCollectionPlatformDelegate();
#else
  ~GlobalBrowserCollectionPlatformDelegate() override;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  void OnBrowserCreated(JNIEnv* env, int64_t j_browser_window_ptr);
  void OnBrowserClosed(JNIEnv* env, int64_t j_browser_window_ptr);
  void OnBrowserActivated(JNIEnv* env, int64_t j_browser_window_ptr);
  void OnBrowserDeactivated(JNIEnv* env, int64_t j_browser_window_ptr);
#endif  // BUILDFLAG(IS_ANDROID)

 private:
#if !BUILDFLAG(IS_ANDROID)
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;
#else
  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;
#endif  // !BUILDFLAG(IS_ANDROID)

  raw_ref<GlobalBrowserCollection> parent_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_
