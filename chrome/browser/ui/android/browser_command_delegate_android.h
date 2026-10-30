// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_BROWSER_COMMAND_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_BROWSER_COMMAND_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"

// Native bridge to a Java BrowserCommandDelegate, used by
// BrowserShortcutManagerAndroid to execute matched IDC_* browser commands.
class BrowserCommandDelegateAndroid {
 public:
  explicit BrowserCommandDelegateAndroid(
      const jni_zero::JavaRef<jobject>& java_delegate);
  BrowserCommandDelegateAndroid(const BrowserCommandDelegateAndroid&) = delete;
  BrowserCommandDelegateAndroid& operator=(
      const BrowserCommandDelegateAndroid&) = delete;
  ~BrowserCommandDelegateAndroid();

  bool ExecuteCommand(int command_id);

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> java_delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_BROWSER_COMMAND_DELEGATE_ANDROID_H_
