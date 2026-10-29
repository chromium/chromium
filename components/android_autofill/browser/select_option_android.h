// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_SELECT_OPTION_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_SELECT_OPTION_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType(),
// including "base/android/jni_string.h" for `std::u16string`.
#include "components/android_autofill/browser/jni_headers/SelectOption_jni.h"

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::SelectOption>(
    JNIEnv* env,
    const autofill::SelectOption& option) {
  return autofill::Java_SelectOption_Constructor(env, option.value,
                                                 option.text);
}

}  // namespace jni_zero

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_SELECT_OPTION_ANDROID_H_
