// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/form_data_android.h"

#include "base/android/jni_string.h"
#include "components/autofill/android/form_field_data_android.h"
#include "components/autofill/android/jni_headers/FormData_jni.h"
#include "components/autofill/core/browser/form_structure.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

FormDataAndroid::FormDataAndroid(const FormData& form)
    : form_(form), index_(0) {}

FormDataAndroid::~FormDataAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_FormData_onNativeDestroyed(env, obj);
}

ScopedJavaLocalRef<jobject> FormDataAndroid::GetJavaPeer(
    const FormStructure* form_structure) {
  // |form_structure| is ephemeral and shouldn't be used outside this call
  // stack.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    for (size_t i = 0; i < form_.fields.size(); ++i) {
      fields_.push_back(std::unique_ptr<FormFieldDataAndroid>(
          new FormFieldDataAndroid(&form_.fields[i])));
    }
    if (form_structure)
      ApplyHeuristicFieldType(*form_structure);
    ScopedJavaLocalRef<jstring> jname =
        ConvertUTF16ToJavaString(env, form_.name);
    ScopedJavaLocalRef<jstring> jhost =
        ConvertUTF8ToJavaString(env, form_.url.GetOrigin().spec());
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

ScopedJavaLocalRef<jobject> FormDataAndroid::GetNextFormFieldData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  DCHECK(index_ <= fields_.size());
  if (index_ == fields_.size())
    return ScopedJavaLocalRef<jobject>();
  return fields_[index_++]->GetJavaPeer();
}

void FormDataAndroid::OnFormFieldDidChange(size_t index,
                                           const base::string16& value) {
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

void FormDataAndroid::ApplyHeuristicFieldType(
    const FormStructure& form_structure) {
  DCHECK(form_structure.field_count() == fields_.size());
  auto form_field_data_android = fields_.begin();
  for (const auto& autofill_field : form_structure) {
    DCHECK(form_field_data_android->get()->SimilarFieldAs(*autofill_field));
    form_field_data_android->get()->set_heuristic_type(
        AutofillType(autofill_field->heuristic_type()));
    if (++form_field_data_android == fields_.end())
      break;
  }
}

}  // namespace autofill
