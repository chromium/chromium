// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/os_additional_security_permission_util_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_callback.h"
#include "base/functional/callback.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/OsAdditionalSecurityPermissionUtil_jni.h"

namespace permissions {
bool HasJavascriptOptimizerPermission() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_OsAdditionalSecurityPermissionUtil_hasJavascriptOptimizerPermission(
      env);
}
}  // namespace permissions
