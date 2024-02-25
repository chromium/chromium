// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_utils.h"
#include "chromecast/app/cast_main_delegate.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/jni_zero/jni_zero.h"

extern "C" {
// This JNI registration method is found and called by module framework code.
JNI_BOUNDARY_EXPORT bool JNI_OnLoad_cast_browser(JNIEnv* env) {
  content::Compositor::Initialize();
  content::SetContentMainDelegate(new chromecast::shell::CastMainDelegate);
  return true;
}
}  // extern "C"
