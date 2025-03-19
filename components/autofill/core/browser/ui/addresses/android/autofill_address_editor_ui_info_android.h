// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_EDITOR_UI_INFO_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_EDITOR_UI_INFO_ANDROID_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/autofill/core/browser/ui/addresses/android/autofill_address_ui_component_android.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class
// org.chromium.components.autofill.AutofillAddressEditorUIInfo. It is used to
// pass the information about the list of fields to display in the Chrome
// address editors.
struct COMPONENT_EXPORT(AUTOFILL) AutofillAddressEditorUiInfoAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const AutofillAddressEditorUiInfoAndroid& component);

  static AutofillAddressEditorUiInfoAndroid FromJavaAutofillAddressEditorUIInfo(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_token);

  AutofillAddressEditorUiInfoAndroid(
      std::string best_language_tag,
      const std::vector<AutofillAddressUiComponentAndroid>& components);
  ~AutofillAddressEditorUiInfoAndroid();
  AutofillAddressEditorUiInfoAndroid(const AutofillAddressEditorUiInfoAndroid&);
  AutofillAddressEditorUiInfoAndroid(AutofillAddressEditorUiInfoAndroid&&);
  AutofillAddressEditorUiInfoAndroid& operator=(
      const AutofillAddressEditorUiInfoAndroid&);
  AutofillAddressEditorUiInfoAndroid& operator=(
      AutofillAddressEditorUiInfoAndroid&&);

  std::string best_language_tag;
  std::vector<AutofillAddressUiComponentAndroid> components;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::AutofillAddressEditorUiInfoAndroid
FromJniType<autofill::AutofillAddressEditorUiInfoAndroid>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object) {
  return autofill::AutofillAddressEditorUiInfoAndroid::
      FromJavaAutofillAddressEditorUIInfo(env, j_object);
}
template <>
inline jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<autofill::AutofillAddressEditorUiInfoAndroid>(
    JNIEnv* env,
    const autofill::AutofillAddressEditorUiInfoAndroid& editor_info) {
  return autofill::AutofillAddressEditorUiInfoAndroid::Create(env, editor_info);
}
}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_AUTOFILL_ADDRESS_EDITOR_UI_INFO_ANDROID_H_
