// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_IMPL_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_IMPL_H_

#include <string_view>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/form_field_data_android_bridge.h"

namespace autofill {

class FormFieldDataAndroidBridgeImpl : public FormFieldDataAndroidBridge {
 public:
  FormFieldDataAndroidBridgeImpl();
  FormFieldDataAndroidBridgeImpl(const FormFieldDataAndroidBridgeImpl&) =
      delete;
  FormFieldDataAndroidBridgeImpl& operator=(
      const FormFieldDataAndroidBridgeImpl&) = delete;
  ~FormFieldDataAndroidBridgeImpl() override;

  // FormFieldDataAndroidBridge:
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaPeer(
      const FormFieldData& field,
      const FormFieldDataAndroid::FieldTypes& field_types) override;
  void UpdateFieldFromJava(FormFieldData& field) override;
  void UpdateFieldTypes(
      const FormFieldDataAndroid::FieldTypes& field_types) override;
  void UpdateValue(std::u16string_view value) override;
  void UpdateVisible(bool visible) override;

 private:
  // A weak reference to the java object.
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_IMPL_H_
