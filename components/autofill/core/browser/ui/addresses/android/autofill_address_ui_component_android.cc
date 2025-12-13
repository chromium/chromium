// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/android/autofill_address_ui_component_android.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/AutofillAddressUiComponent_jni.h"

namespace autofill {

AutofillAddressUiComponentAndroid::AutofillAddressUiComponentAndroid(
    FieldType field_type,
    std::string label,
    bool is_required,
    bool is_full_line)
    : field_type(field_type),
      label(std::move(label)),
      is_required(is_required),
      is_full_line(is_full_line) {}

jni_zero::ScopedJavaLocalRef<jobject> AutofillAddressUiComponentAndroid::Create(
    JNIEnv* env,
    const AutofillAddressUiComponentAndroid& component) {
  return Java_AutofillAddressUiComponent_Constructor(
      env, component.field_type, component.label, component.is_required,
      component.is_full_line);
}

AutofillAddressUiComponentAndroid
AutofillAddressUiComponentAndroid::FromJavaAutofillAddressUiComponent(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_component) {
  autofill::FieldType field_type = ToSafeFieldType(
      Java_AutofillAddressUiComponent_getFieldType(env, j_component),
      UNKNOWN_TYPE);
  std::string label =
      Java_AutofillAddressUiComponent_getLabel(env, j_component);
  bool is_required =
      Java_AutofillAddressUiComponent_isRequired(env, j_component);
  bool is_full_line =
      Java_AutofillAddressUiComponent_isFullLine(env, j_component);
  return AutofillAddressUiComponentAndroid(field_type, std::move(label),
                                           is_required, is_full_line);
}

}  // namespace autofill

DEFINE_JNI(AutofillAddressUiComponent)
