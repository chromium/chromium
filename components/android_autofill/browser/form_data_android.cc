// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/jni_headers/FormData_jni.h"
#include "components/autofill/core/browser/form_structure.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

FormDataAndroid::FormDataAndroid(const FormData& form) : form_(form) {}

FormDataAndroid::~FormDataAndroid() = default;

ScopedJavaLocalRef<jobject> FormDataAndroid::GetJavaPeer(
    const FormStructure* form_structure) {
  // |form_structure| is ephemeral and shouldn't be used outside this call
  // stack.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    for (size_t i = 0; i < form_.fields.size(); ++i) {
      fields_.push_back(
          std::make_unique<FormFieldDataAndroid>(&form_.fields[i]));
    }
    if (form_structure)
      UpdateFieldTypes(*form_structure);
    ScopedJavaLocalRef<jstring> jname =
        ConvertUTF16ToJavaString(env, form_.name);
    ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(
        env, form_.url.DeprecatedGetOriginAsURL().spec());
    obj = Java_FormData_createFormData(env, reinterpret_cast<intptr_t>(this),
                                       jname, jhost, form_.fields.size());
    java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  }
  return obj;
}

const FormData& FormDataAndroid::GetAutofillValues() {
  for (std::unique_ptr<FormFieldDataAndroid>& field : fields_)
    field->GetValue();
  return form_;
}

ScopedJavaLocalRef<jobject> FormDataAndroid::GetNextFormFieldData(JNIEnv* env) {
  DCHECK(index_ <= fields_.size());
  if (index_ == fields_.size())
    return ScopedJavaLocalRef<jobject>();
  return fields_[index_++]->GetJavaPeer();
}

void FormDataAndroid::OnFormFieldDidChange(size_t index,
                                           const std::u16string& value) {
  form_.fields[index].value = value;
  fields_[index]->OnFormFieldDidChange(value);
}

bool FormDataAndroid::GetFieldIndex(const FormFieldData& field, size_t* index) {
  for (size_t i = 0; i < form_.fields.size(); ++i) {
    if (form_.fields[i].SameFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::GetSimilarFieldIndex(const FormFieldData& field,
                                           size_t* index) {
  for (size_t i = 0; i < form_.fields.size(); ++i) {
    if (form_.fields[i].SimilarFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::SimilarFormAs(const FormData& form) {
  return form_.SimilarFormAs(form);
}

void FormDataAndroid::UpdateFieldTypes(const FormStructure& form_structure) {
  // This form has been changed after the query starts, ignore this response,
  // new one is on the way.
  if (form_structure.field_count() != fields_.size())
    return;
  auto form_field_data_android = fields_.begin();
  for (const auto& autofill_field : form_structure) {
    DCHECK(form_field_data_android->get()->SimilarFieldAs(*autofill_field));
    std::vector<AutofillType> server_predictions;
    for (const auto& prediction : autofill_field->server_predictions()) {
      server_predictions.emplace_back(
          static_cast<ServerFieldType>(prediction.type()));
    }
    form_field_data_android->get()->UpdateAutofillTypes(
        AutofillType(autofill_field->heuristic_type()),
        AutofillType(autofill_field->server_type()),
        autofill_field->ComputedType(), server_predictions);
    if (++form_field_data_android == fields_.end())
      break;
  }
}

}  // namespace autofill
