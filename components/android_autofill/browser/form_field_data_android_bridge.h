// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_H_

#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "components/android_autofill/browser/form_field_data_android.h"

namespace autofill {

class FormFieldData;

// Interface for the C++ <-> Android bridge between `FormFieldDataAndroid` and
// Java `FormFieldData`.
class FormFieldDataAndroidBridge {
 public:
  virtual ~FormFieldDataAndroidBridge() = default;

  // Returns the Java `FormFieldData` that this bridge keeps a reference to. If
  // the reference is null or has expired, it creates a new Java
  // `FormFieldData`.
  virtual base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaPeer(
      const FormFieldData& field,
      const FormFieldDataAndroid::FieldTypes& type_predictions) = 0;

  // Updates the `is_autofilled`, `check_status` and `value` members of `field`
  // based on the values of the Java `FormFieldData`. If the reference is to the
  // Java object null, this is a no-op.
  virtual void UpdateFieldFromJava(FormFieldData& field) = 0;

  // Updates the field types of the Java `FormFieldData`. If the reference to
  // the Java object is null, this is a no-op.
  virtual void UpdateFieldTypes(
      const FormFieldDataAndroid::FieldTypes& type_predictions) = 0;

  // Updates the value of the Java `FormFieldData` to `value`. If the reference
  // to the Java object is null, this is a no-op.
  virtual void UpdateValue(std::u16string_view value) = 0;

  // Updates the visibility member variable of the Java `FormFieldData` to
  // `visible`. If the reference to the Java object is null, this is a no-op.
  virtual void UpdateVisible(bool visible) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_BRIDGE_H_
