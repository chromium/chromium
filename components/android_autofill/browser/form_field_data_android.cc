// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_field_data_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/android_autofill/browser/jni_headers/FormFieldData_jni.h"
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

namespace {
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfPredictionString(
    JNIEnv* env,
    const std::vector<AutofillType>& server_predictions) {
  if (!server_predictions.empty()) {
    std::vector<std::string> server_prediction_array;
    server_prediction_array.reserve(server_predictions.size());
    for (const auto& p : server_predictions) {
      server_prediction_array.emplace_back(p.ToString());
    }
    return ToJavaArrayOfStrings(env, server_prediction_array);
  }
  return nullptr;
}

}  // namespace

FormFieldDataAndroid::FormFieldDataAndroid(FormFieldData* field)
    : heuristic_type_(AutofillType(UNKNOWN_TYPE)), field_ptr_(field) {}

FormFieldDataAndroid::~FormFieldDataAndroid() = default;

ScopedJavaLocalRef<jobject> FormFieldDataAndroid::GetJavaPeer() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);

  auto ProjectOptions = [this](const std::vector<SelectOption>& options,
                               const auto& projection) {
    std::vector<std::u16string> projected_options;
    base::ranges::transform(field_ptr_->options,
                            std::back_inserter(projected_options), projection);
    return projected_options;
  };
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
    ScopedJavaLocalRef<jobjectArray> joption_values = ToJavaArrayOfStrings(
        env, ProjectOptions(field_ptr_->options, &SelectOption::value));
    ScopedJavaLocalRef<jobjectArray> joption_contents = ToJavaArrayOfStrings(
        env, ProjectOptions(field_ptr_->options, &SelectOption::content));
    ScopedJavaLocalRef<jstring> jheuristic_type;
    if (!heuristic_type_.IsUnknown()) {
      jheuristic_type =
          ConvertUTF8ToJavaString(env, heuristic_type_.ToString());
    }
    ScopedJavaLocalRef<jstring> jserver_type =
        ConvertUTF8ToJavaString(env, server_type_.ToString());
    ScopedJavaLocalRef<jstring> jcomputed_type =
        ConvertUTF8ToJavaString(env, computed_type_.ToString());
    ScopedJavaLocalRef<jobjectArray> jserver_predictions =
        ToJavaArrayOfPredictionString(env, server_predictions_);

    ScopedJavaLocalRef<jobjectArray> jdatalist_values =
        ToJavaArrayOfStrings(env, field_ptr_->datalist_values);
    ScopedJavaLocalRef<jobjectArray> jdatalist_labels =
        ToJavaArrayOfStrings(env, field_ptr_->datalist_labels);

    obj = Java_FormFieldData_createFormFieldData(
        env, jname, jlabel, jvalue, jautocomplete_attr,
        field_ptr_->should_autocomplete, jplaceholder, jtype, jid,
        joption_values, joption_contents, IsCheckable(field_ptr_->check_status),
        IsChecked(field_ptr_->check_status), field_ptr_->max_length,
        jheuristic_type, jserver_type, jcomputed_type, jserver_predictions,
        field_ptr_->bounds.x(), field_ptr_->bounds.y(),
        field_ptr_->bounds.right(), field_ptr_->bounds.bottom(),
        jdatalist_values, jdatalist_labels, field_ptr_->IsFocusable());
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

void FormFieldDataAndroid::OnFormFieldDidChange(const std::u16string& value) {
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

void FormFieldDataAndroid::UpdateAutofillTypes(
    const AutofillType& heuristic_type,
    const AutofillType& server_type,
    const AutofillType& computed_type,
    const std::vector<AutofillType>& server_predictions) {
  heuristic_type_ = heuristic_type;
  server_type_ = server_type;
  computed_type_ = computed_type;
  server_predictions_ = server_predictions;

  // Java peer isn't available when this object is instantiated, update to
  // Java peer if the prediction arrives later.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jserver_type =
      ConvertUTF8ToJavaString(env, server_type_.ToString());
  ScopedJavaLocalRef<jstring> jcomputed_type =
      ConvertUTF8ToJavaString(env, computed_type_.ToString());
  ScopedJavaLocalRef<jobjectArray> jserver_predictions =
      ToJavaArrayOfPredictionString(env, server_predictions_);

  Java_FormFieldData_updateFieldTypes(env, obj, jserver_type, jcomputed_type,
                                      jserver_predictions);
}

}  // namespace autofill
