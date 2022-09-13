// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_SPELLCHECK_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace spellcheck {

namespace android {

// Register all JNI bindings necessary for the spellcheck component.
bool RegisterSpellcheckJni(JNIEnv* env);

}  // namespace android

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
