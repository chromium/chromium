// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_DROPDOWN_KEY_VALUE_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_DROPDOWN_KEY_VALUE_ANDROID_H_

#include <string>

#include "base/component_export.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class
// org.chromium.components.autofill.DropdownKeyValue. It is used to pass
// key-value pairs to populate a dropdown list in Android.
struct COMPONENT_EXPORT(AUTOFILL) DropdownKeyValueAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const DropdownKeyValueAndroid& key_value);

  static DropdownKeyValueAndroid FromJavaDropdownKeyValue(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_key_value);

  DropdownKeyValueAndroid(std::string key, std::u16string value);
  ~DropdownKeyValueAndroid() = default;
  DropdownKeyValueAndroid(const DropdownKeyValueAndroid&) = default;
  DropdownKeyValueAndroid(DropdownKeyValueAndroid&&) = default;
  DropdownKeyValueAndroid& operator=(const DropdownKeyValueAndroid&) = default;
  DropdownKeyValueAndroid& operator=(DropdownKeyValueAndroid&&) = default;

  std::string key;
  std::u16string value;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::DropdownKeyValueAndroid
FromJniType<autofill::DropdownKeyValueAndroid>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object) {
  return autofill::DropdownKeyValueAndroid::FromJavaDropdownKeyValue(env,
                                                                     j_object);
}
template <>
inline jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<autofill::DropdownKeyValueAndroid>(
    JNIEnv* env,
    const autofill::DropdownKeyValueAndroid& key_value) {
  return autofill::DropdownKeyValueAndroid::Create(env, key_value);
}
}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_DROPDOWN_KEY_VALUE_ANDROID_H_
