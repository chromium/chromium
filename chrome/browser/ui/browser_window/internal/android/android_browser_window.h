// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

// Android implementation of |BrowserWindowInterface|.
class AndroidBrowserWindow final : public BrowserWindowInterface {
 public:
  AndroidBrowserWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_android_browser_window,
      const BrowserWindowInterface::Type type,
      Profile* profile);
  AndroidBrowserWindow(const AndroidBrowserWindow&) = delete;
  AndroidBrowserWindow& operator=(const AndroidBrowserWindow&) = delete;
  ~AndroidBrowserWindow() override;

  // Implements Java |AndroidBrowserWindow.Natives#destroy|.
  void Destroy(JNIEnv* env);

  // Implements Java |AndroidBrowserWindow.Natives#getSessionIdForTesting|.
  jint GetSessionIdForTesting(JNIEnv* env) const;

  // Implements |BrowserWindowInterface|.
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;
  ui::BaseWindow* GetWindow() override;
  const ui::BaseWindow* GetWindow() const override;
  Profile* GetProfile() override;
  const Profile* GetProfile() const override;
  const SessionID& GetSessionID() const override;
  Type GetType() const override;

  // Implements |content::PageNavigator|, which is inherited by
  // |BrowserWindowInterface|.
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  // Returns a ChromeTabbedActivity Java object for this window, may be null if
  // the task does not have an activity.
  base::android::ScopedJavaLocalRef<jobject> GetActivity();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_android_browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  const BrowserWindowInterface::Type type_;
  const raw_ref<Profile> profile_;
  const SessionID session_id_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BROWSER_WINDOW_H_
