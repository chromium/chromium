// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/browser_command_delegate_android.h"

#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BrowserCommandDelegate_jni.h"

BrowserCommandDelegateAndroid::BrowserCommandDelegateAndroid(
    const jni_zero::JavaRef<jobject>& java_delegate)
    : java_delegate_(java_delegate) {}

BrowserCommandDelegateAndroid::~BrowserCommandDelegateAndroid() = default;

bool BrowserCommandDelegateAndroid::ExecuteCommand(int command_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserCommandDelegate_executeCommand(env, java_delegate_,
                                                    command_id);
}

DEFINE_JNI(BrowserCommandDelegate)
