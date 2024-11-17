// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_field_data_android_bridge_impl.h"

#include <string>
#include <string_view>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/android_autofill/browser/jni_headers/FormFieldData_jni.h"

namespace autofill {

namespace {

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using jni_zero::AttachCurrentThread;

// Converts the `AutofillType`s to strings and returns a Java array of strings.
// Returns `nullptr` instead if `server_predictions` is empty.
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfPredictionStrings(
    JNIEnv* env,
    const std::vector<AutofillType>& server_predictions) {
  if (!server_predictions.empty()) {
    std::vector<std::string> server_prediction_array;
    server_prediction_array.reserve(server_predictions.size());
    for (const auto& p : server_predictions) {
      server_prediction_array.emplace_back(std::string(p.ToStringView()));
    }
    return ToJavaArrayOfStrings(env, server_prediction_array);
  }
  return nullptr;
}

}  // namespace

FormFieldDataAndroidBridgeImpl::FormFieldDataAndroidBridgeImpl() = default;

FormFieldDataAndroidBridgeImpl::~FormFieldDataAndroidBridgeImpl() = default;

base::android::ScopedJavaLocalRef<jobject>
FormFieldDataAndroidBridgeImpl::GetOrCreateJavaPeer(
    const FormFieldData& field,
    const FormFieldDataAndroid::FieldTypes& field_types) {
  JNIEnv* env = AttachCurrentThread();
  if (ScopedJavaLocalRef<jobject> obj = java_ref_.get(env); !obj.is_null()) {
    return obj;
  }

  auto ProjectOptions = [env](base::span<const SelectOption> options,
                              const auto& projection) {
    std::vector<std::u16string> projected_options;
    projected_options.reserve(options.size());
    base::ranges::transform(options, std::back_inserter(projected_options),
                            projection);
    return ToJavaArrayOfStrings(env, projected_options);
  };

  ScopedJavaLocalRef<jobject> obj = Java_FormFieldData_createFormFieldData(
      env, ConvertUTF16ToJavaString(env, field.name()),
      ConvertUTF16ToJavaString(env, field.label()),
      ConvertUTF16ToJavaString(env, field.value()),
      ConvertUTF8ToJavaString(env, field.autocomplete_attribute()),
      field.should_autocomplete(),
      ConvertUTF16ToJavaString(env, field.placeholder()),
      ConvertUTF8ToJavaString(
          env, FormControlTypeToString(field.form_control_type())),
      ConvertUTF16ToJavaString(env, field.id_attribute()),
      /*optionValues=*/ProjectOptions(field.options(), &SelectOption::value),
      /*optionContents=*/
      ProjectOptions(field.options(), &SelectOption::text),
      IsCheckable(field.check_status()), IsChecked(field.check_status()),
      field.max_length(),
      /*heuristicType=*/field_types.heuristic_type.IsUnknown()
          ? nullptr
          : ConvertUTF8ToJavaString(env,
                                    field_types.heuristic_type.ToStringView()),
      ConvertUTF8ToJavaString(env, field_types.server_type.ToStringView()),
      ConvertUTF8ToJavaString(env, field_types.computed_type.ToStringView()),
      ToJavaArrayOfPredictionStrings(env, field_types.server_predictions),
      field.bounds().x(), field.bounds().y(), field.bounds().right(),
      field.bounds().bottom(),
      /*datalistValues=*/
      ProjectOptions(field.datalist_options(), &SelectOption::value),
      /*datalistLabels=*/
      ProjectOptions(field.datalist_options(), &SelectOption::text),
      /*visible=*/field.IsFocusable(), field.is_autofilled());
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  return obj;
}

void FormFieldDataAndroidBridgeImpl::UpdateFieldFromJava(FormFieldData& field) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  field.set_is_autofilled(Java_FormFieldData_isAutofilled(env, obj));
  if (IsCheckable(field.check_status())) {
    SetCheckStatus(&field, true, Java_FormFieldData_isChecked(env, obj));
    return;
  }
  if (ScopedJavaLocalRef<jstring> jvalue =
          Java_FormFieldData_getValue(env, obj);
      !jvalue.is_null()) {
    field.set_value(ConvertJavaStringToUTF16(env, jvalue));
  }
}

void FormFieldDataAndroidBridgeImpl::UpdateFieldTypes(
    const FormFieldDataAndroid::FieldTypes& field_types) {
  // TODO(crbug.com/40929724): Investigate why heuristic type is not updated.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_FormFieldData_updateFieldTypes(
      env, obj,
      ConvertUTF8ToJavaString(env, field_types.server_type.ToStringView()),
      ConvertUTF8ToJavaString(env, field_types.computed_type.ToStringView()),
      ToJavaArrayOfPredictionStrings(env, field_types.server_predictions));
}

void FormFieldDataAndroidBridgeImpl::UpdateValue(std::u16string_view value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_FormFieldData_updateValue(env, obj,
                                 ConvertUTF16ToJavaString(env, value));
}

void FormFieldDataAndroidBridgeImpl::UpdateVisible(bool visible) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_FormFieldData_updateVisible(env, obj, visible);
}

}  // namespace autofill
