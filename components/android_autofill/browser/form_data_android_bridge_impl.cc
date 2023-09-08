// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android_bridge_impl.h"

#include <memory>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/jni_headers/FormData_jni.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfObjects;

constexpr char kFormFieldDataAndroidClassname[] =
    "org/chromium/components/autofill/FormFieldData";

}  // namespace

FormDataAndroidBridgeImpl::FormDataAndroidBridgeImpl() = default;

FormDataAndroidBridgeImpl::~FormDataAndroidBridgeImpl() = default;

base::android::ScopedJavaLocalRef<jobject>
FormDataAndroidBridgeImpl::GetOrCreateJavaPeer(
    const FormData& form,
    base::span<const std::unique_ptr<FormFieldDataAndroid>> fields_android) {
  JNIEnv* env = AttachCurrentThread();
  if (ScopedJavaLocalRef<jobject> obj = java_ref_.get(env); !obj.is_null()) {
    return obj;
  }

  std::vector<ScopedJavaLocalRef<jobject>> android_objects;
  android_objects.reserve(fields_android.size());
  for (const std::unique_ptr<FormFieldDataAndroid>& field : fields_android) {
    android_objects.push_back(field->GetJavaPeer());
  }

  ScopedJavaLocalRef<jobject> obj = Java_FormData_createFormData(
      env, ConvertUTF16ToJavaString(env, form.name),
      /*origin=*/
      ConvertUTF8ToJavaString(env, form.url.DeprecatedGetOriginAsURL().spec()),
      ToJavaArrayOfObjects(env, GetClass(env, kFormFieldDataAndroidClassname),
                           android_objects));
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  return obj;
}

}  // namespace autofill
