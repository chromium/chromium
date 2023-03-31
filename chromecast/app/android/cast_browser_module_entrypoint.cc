// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_utils.h"
#include "chromecast/app/cast_main_delegate.h"
#include "chromecast/cast_shell_jni_registration_generated.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"

extern "C" {
// This JNI registration method is found and called by module framework code.
JNI_GENERATOR_EXPORT bool JNI_OnLoad_cast_browser(JNIEnv* env) {
  if (!base::android::IsSelectiveJniRegistrationEnabled(env) &&
      !RegisterNonMainDexNatives(env)) {
    return false;
  }
  if (!RegisterMainDexNatives(env)) {
    return false;
  }

  content::Compositor::Initialize();
  content::SetContentMainDelegate(new chromecast::shell::CastMainDelegate);
  return true;
}
}  // extern "C"
