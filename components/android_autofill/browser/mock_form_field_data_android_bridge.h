// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_FIELD_DATA_ANDROID_BRIDGE_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_FIELD_DATA_ANDROID_BRIDGE_H_

#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/form_field_data_android_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockFormFieldDataAndroidBridge : public FormFieldDataAndroidBridge {
 public:
  MockFormFieldDataAndroidBridge();
  ~MockFormFieldDataAndroidBridge() override;

  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetOrCreateJavaPeer,
              (const FormFieldData&, const FormFieldDataAndroid::FieldTypes&),
              (override));
  MOCK_METHOD(void, UpdateFieldFromJava, (FormFieldData&), (override));
  MOCK_METHOD(void,
              UpdateFieldTypes,
              (const FormFieldDataAndroid::FieldTypes&),
              (override));
  MOCK_METHOD(void, UpdateValue, (std::u16string_view), (override));
  MOCK_METHOD(void, UpdateVisible, (bool), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_FIELD_DATA_ANDROID_BRIDGE_H_
