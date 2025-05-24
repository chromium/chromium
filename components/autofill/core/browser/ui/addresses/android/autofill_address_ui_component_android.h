// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_UI_COMPONENT_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_UI_COMPONENT_ANDROID_H_

#include <string>

#include "base/component_export.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class
// org.chromium.components.autofill.AutofillAddressUIComponent. It is used to
// pass the information about the fields to display in the Chrome address
// editors.
struct COMPONENT_EXPORT(AUTOFILL) AutofillAddressUiComponentAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const AutofillAddressUiComponentAndroid& component);

  static AutofillAddressUiComponentAndroid FromJavaAutofillAddressUiComponent(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_token);

  AutofillAddressUiComponentAndroid(FieldType field_type,
                                    std::string label,
                                    bool is_required,
                                    bool is_full_line);
  AutofillAddressUiComponentAndroid(const AutofillAddressUiComponentAndroid&) =
      default;
  AutofillAddressUiComponentAndroid(AutofillAddressUiComponentAndroid&&) =
      default;
  AutofillAddressUiComponentAndroid& operator=(
      const AutofillAddressUiComponentAndroid&) = default;
  AutofillAddressUiComponentAndroid& operator=(
      AutofillAddressUiComponentAndroid&&) = default;

  // The type of the field, e.g., FieldType.NAME_FULL.
  FieldType field_type = FieldType::UNKNOWN_TYPE;
  // The localized display label for the field, e.g., "City.".
  std::string label;
  // Whether the field is required.
  bool is_required = false;
  // Whether the field takes up the full line.
  bool is_full_line = false;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::AutofillAddressUiComponentAndroid
FromJniType<autofill::AutofillAddressUiComponentAndroid>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object) {
  return autofill::AutofillAddressUiComponentAndroid::
      FromJavaAutofillAddressUiComponent(env, j_object);
}
template <>
inline jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<autofill::AutofillAddressUiComponentAndroid>(
    JNIEnv* env,
    const autofill::AutofillAddressUiComponentAndroid& component) {
  return autofill::AutofillAddressUiComponentAndroid::Create(env, component);
}
}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_UI_COMPONENT_ANDROID_H_
