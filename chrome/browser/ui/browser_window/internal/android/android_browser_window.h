// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

// Android implementation of |BrowserWindowInterface|.
class AndroidBrowserWindow final : public BrowserWindowInterface {
 public:
  AndroidBrowserWindow();
  AndroidBrowserWindow(const AndroidBrowserWindow&) = delete;
  AndroidBrowserWindow& operator=(const AndroidBrowserWindow&) = delete;
  ~AndroidBrowserWindow() override;

  // Implements |BrowserWindowInterface|.
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;
  ui::BaseWindow* GetWindow() override;
  Profile* GetProfile() override;
  const SessionID& GetSessionID() const override;

  // Implements |content::PageNavigator|, which is inherited by
  // |BrowserWindowInterface|.
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
