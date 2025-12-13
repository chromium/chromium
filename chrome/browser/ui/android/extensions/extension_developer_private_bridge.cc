// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_developer_private_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionDeveloperPrivateBridge_jni.h"

namespace extensions {

// static
void ExtensionDeveloperPrivateBridge::ShowSiteSettings(
    const std::string& extension_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionDeveloperPrivateBridge_showSiteSettings(env, extension_id);
}

}  // namespace extensions

DEFINE_JNI(ExtensionDeveloperPrivateBridge)
