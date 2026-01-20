// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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
#else   // BUILDFLAG(IS_ANDROID)
  ~GlobalBrowserCollectionPlatformDelegate() override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
#if !BUILDFLAG(IS_ANDROID)
  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override;
#endif  // !BUILDFLAG(IS_ANDROID)

  raw_ref<GlobalBrowserCollection> parent_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_GLOBAL_BROWSER_COLLECTION_PLATFORM_DELEGATE_H_
