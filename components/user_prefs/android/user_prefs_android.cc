// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/user_prefs/android/jni_headers/UserPrefs_jni.h"

namespace user_prefs {

static base::android::ScopedJavaLocalRef<jobject> JNI_UserPrefs_Get(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jbrowser_context_handle) {
  return UserPrefs::Get(
             content::BrowserContextFromJavaHandle(jbrowser_context_handle))
      ->GetJavaObject();
}

}  // namespace user_prefs
