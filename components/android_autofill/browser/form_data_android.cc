// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/jni_headers/FormData_jni.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

constexpr char kFormFieldDataAndroidClassname[] =
    "org/chromium/components/autofill/FormFieldData";
}  // namespace

FormDataAndroid::FormDataAndroid(const FormData& form) : form_(form) {}

FormDataAndroid::~FormDataAndroid() = default;

ScopedJavaLocalRef<jobject> FormDataAndroid::GetJavaPeer(
    const FormStructure* form_structure) {
  // `form_structure` is ephemeral and shouldn't be used outside this call
  // stack.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    fields_.clear();
    fields_.reserve(form_.fields.size());
    for (FormFieldData& field : form_.fields) {
      fields_.push_back(std::make_unique<FormFieldDataAndroid>(&field));
    }

    if (form_structure)
      UpdateFieldTypes(*form_structure);

    ScopedJavaLocalRef<jstring> jname =
        ConvertUTF16ToJavaString(env, form_.name);
    ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(
        env, form_.url.DeprecatedGetOriginAsURL().spec());
    std::vector<ScopedJavaLocalRef<jobject>> fields_android;
    fields_android.reserve(fields_.size());
    for (std::unique_ptr<FormFieldDataAndroid>& field : fields_) {
      fields_android.push_back(field->GetJavaPeer());
    }
    ScopedJavaLocalRef<jclass> field_class =
        base::android::GetClass(env, kFormFieldDataAndroidClassname);

    obj = Java_FormData_createFormData(
        env, reinterpret_cast<intptr_t>(this), jname, jhost,
        base::android::ToJavaArrayOfObjects(env, field_class, fields_android));
    java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  }
  return obj;
}

void FormDataAndroid::UpdateFromJava() {
  for (std::unique_ptr<FormFieldDataAndroid>& field : fields_)
    field->UpdateFromJava();
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

bool FormDataAndroid::SimilarFormAs(const FormData& form) const {
  return form_.SimilarFormAs(form);
}

void FormDataAndroid::UpdateFieldTypes(const FormStructure& form_structure) {
  // This form has been changed after the query starts. Ignore this response,
  // a new one is on the way.
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
