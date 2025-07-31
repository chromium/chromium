// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

// Android implementation of |BrowserWindowInterface|.
class AndroidBrowserWindow final : public BrowserWindowInterface {
 public:
  AndroidBrowserWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_android_browser_window);
  AndroidBrowserWindow(const AndroidBrowserWindow&) = delete;
  AndroidBrowserWindow& operator=(const AndroidBrowserWindow&) = delete;
  ~AndroidBrowserWindow() override;

  // Returns a list of all active AndroidBrowserWindows, ordered by creation
  // time.
  // TODO(https://crbug.com/419057482, https://crbug.com/435264038): This is a
  // possibly-temporary solution for tracking BrowserWindowInterfaces, and
  // might be removed in the future.
  static std::vector<BrowserWindowInterface*>
  GetAllAndroidBrowserWindowsByCreationTime();

  // Implements Java |AndroidBrowserWindow.Natives#destroy|.
  void Destroy(JNIEnv* env);

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

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_android_browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  const SessionID session_id_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
