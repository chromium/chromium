// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android_bridge_impl.h"

#include <memory>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/span.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/autofill/core/common/form_data.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/android_autofill/browser/jni_headers/FormData_jni.h"

namespace autofill {

namespace {

using jni_zero::ScopedJavaLocalRef;

}  // namespace

FormDataAndroidBridgeImpl::FormDataAndroidBridgeImpl() = default;

FormDataAndroidBridgeImpl::~FormDataAndroidBridgeImpl() = default;

ScopedJavaLocalRef<jobject> FormDataAndroidBridgeImpl::GetOrCreateJavaPeer(
    const FormData& form,
    SessionId session_id,
    base::span<const std::unique_ptr<FormFieldDataAndroid>> fields_android) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (ScopedJavaLocalRef<jobject> obj = java_ref_.get(env); !obj.is_null()) {
    return obj;
  }

  std::vector<ScopedJavaLocalRef<jobject>> android_objects;
  android_objects.reserve(fields_android.size());
  for (const std::unique_ptr<FormFieldDataAndroid>& field : fields_android) {
    android_objects.push_back(field->GetJavaPeer());
  }

  ScopedJavaLocalRef<jobject> obj = Java_FormData_createFormData(
      env, session_id.value(), form.name(),
      /*origin=*/
      form.url().DeprecatedGetOriginAsURL().spec(), android_objects);
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  return obj;
}

}  // namespace autofill
