// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/android/dropdown_key_value_android.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/DropdownKeyValue_jni.h"

namespace autofill {

DropdownKeyValueAndroid::DropdownKeyValueAndroid(std::string key,
                                                 std::u16string value)
    : key(std::move(key)), value(std::move(value)) {}

jni_zero::ScopedJavaLocalRef<jobject> DropdownKeyValueAndroid::Create(
    JNIEnv* env,
    const DropdownKeyValueAndroid& key_value) {
  return Java_DropdownKeyValue_Constructor(env, key_value.key, key_value.value);
}

DropdownKeyValueAndroid DropdownKeyValueAndroid::FromJavaDropdownKeyValue(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& key_value) {
  return DropdownKeyValueAndroid(
      Java_DropdownKeyValue_getKey(env, key_value),
      Java_DropdownKeyValue_getValue(env, key_value));
}

}  // namespace autofill

DEFINE_JNI_FOR_DropdownKeyValue()
