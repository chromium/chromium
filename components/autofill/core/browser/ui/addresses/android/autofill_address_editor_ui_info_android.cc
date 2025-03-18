// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/android/autofill_address_editor_ui_info_android.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/AutofillAddressEditorUiInfo_jni.h"
#include "components/autofill/core/browser/ui/addresses/android/autofill_address_ui_component_android.h"

namespace autofill {

AutofillAddressEditorUiInfoAndroid::AutofillAddressEditorUiInfoAndroid(
    std::string best_language_tag,
    const std::vector<AutofillAddressUiComponentAndroid>& components)
    : best_language_tag(std::move(best_language_tag)), components(components) {}

AutofillAddressEditorUiInfoAndroid::~AutofillAddressEditorUiInfoAndroid() =
    default;

AutofillAddressEditorUiInfoAndroid::AutofillAddressEditorUiInfoAndroid(
    const AutofillAddressEditorUiInfoAndroid&) = default;
AutofillAddressEditorUiInfoAndroid::AutofillAddressEditorUiInfoAndroid(
    AutofillAddressEditorUiInfoAndroid&&) = default;
AutofillAddressEditorUiInfoAndroid&
AutofillAddressEditorUiInfoAndroid::operator=(
    const AutofillAddressEditorUiInfoAndroid&) = default;
AutofillAddressEditorUiInfoAndroid&
AutofillAddressEditorUiInfoAndroid::operator=(
    AutofillAddressEditorUiInfoAndroid&&) = default;

jni_zero::ScopedJavaLocalRef<jobject>
AutofillAddressEditorUiInfoAndroid::Create(
    JNIEnv* env,
    const AutofillAddressEditorUiInfoAndroid& editor_info) {
  return Java_AutofillAddressEditorUiInfo_Constructor(
      env, editor_info.best_language_tag, editor_info.components);
}

AutofillAddressEditorUiInfoAndroid
AutofillAddressEditorUiInfoAndroid::FromJavaAutofillAddressEditorUIInfo(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_editor_info) {
  std::string best_language_code =
      Java_AutofillAddressEditorUiInfo_getBestLanguageTag(env, j_editor_info);
  std::vector<AutofillAddressUiComponentAndroid> ui_components =
      Java_AutofillAddressEditorUiInfo_getComponents(env, j_editor_info);
  return AutofillAddressEditorUiInfoAndroid(std::move(best_language_code),
                                            std::move(ui_components));
}

}  // namespace autofill

DEFINE_JNI_FOR_AutofillAddressEditorUiInfo()
