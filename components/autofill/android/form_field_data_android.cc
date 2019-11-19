// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/form_field_data_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/jni_headers/FormFieldData_jni.h"
#include "components/autofill/core/common/autofill_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace autofill {

FormFieldDataAndroid::FormFieldDataAndroid(FormFieldData* field)
    : heuristic_type_(AutofillType(UNKNOWN_TYPE)), field_ptr_(field) {}

ScopedJavaLocalRef<jobject> FormFieldDataAndroid::GetJavaPeer() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    ScopedJavaLocalRef<jstring> jname =
        ConvertUTF16ToJavaString(env, field_ptr_->name);
    ScopedJavaLocalRef<jstring> jlabel =
        ConvertUTF16ToJavaString(env, field_ptr_->label);
    ScopedJavaLocalRef<jstring> jvalue =
        ConvertUTF16ToJavaString(env, field_ptr_->value);
    ScopedJavaLocalRef<jstring> jautocomplete_attr =
        ConvertUTF8ToJavaString(env, field_ptr_->autocomplete_attribute);
    ScopedJavaLocalRef<jstring> jplaceholder =
        ConvertUTF16ToJavaString(env, field_ptr_->placeholder);
    ScopedJavaLocalRef<jstring> jid =
        ConvertUTF16ToJavaString(env, field_ptr_->id_attribute);
    ScopedJavaLocalRef<jstring> jtype =
        ConvertUTF8ToJavaString(env, field_ptr_->form_control_type);
    ScopedJavaLocalRef<jobjectArray> joption_values =
        ToJavaArrayOfStrings(env, field_ptr_->option_values);
    ScopedJavaLocalRef<jobjectArray> joption_contents =
        ToJavaArrayOfStrings(env, field_ptr_->option_contents);
    ScopedJavaLocalRef<jstring> jheuristic_type;
    if (!heuristic_type_.IsUnknown())
      jheuristic_type =
          ConvertUTF8ToJavaString(env, heuristic_type_.ToString());

    obj = Java_FormFieldData_createFormFieldData(
        env, jname, jlabel, jvalue, jautocomplete_attr,
        field_ptr_->should_autocomplete, jplaceholder, jtype, jid,
        joption_values, joption_contents, IsCheckable(field_ptr_->check_status),
        IsChecked(field_ptr_->check_status), field_ptr_->max_length,
        jheuristic_type);
    java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  }
  return obj;
}

void FormFieldDataAndroid::GetValue() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  if (IsCheckable(field_ptr_->check_status)) {
    bool checked = Java_FormFieldData_isChecked(env, obj);
    SetCheckStatus(field_ptr_, true, checked);
  } else {
    ScopedJavaLocalRef<jstring> jvalue = Java_FormFieldData_getValue(env, obj);
    if (jvalue.is_null())
      return;
    field_ptr_->value = ConvertJavaStringToUTF16(env, jvalue);
  }
  field_ptr_->is_autofilled = true;
}

void FormFieldDataAndroid::OnFormFieldDidChange(const base::string16& value) {
  field_ptr_->value = value;
  field_ptr_->is_autofilled = false;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_FormFieldData_updateValue(env, obj,
                                 ConvertUTF16ToJavaString(env, value));
}

bool FormFieldDataAndroid::SimilarFieldAs(const FormFieldData& field) const {
  return field_ptr_->SimilarFieldAs(field);
}

}  // namespace autofill
