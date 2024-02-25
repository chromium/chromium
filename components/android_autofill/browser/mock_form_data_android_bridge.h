// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_DATA_ANDROID_BRIDGE_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_DATA_ANDROID_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/form_data_android_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class FormFieldDataAndroid;

class MockFormDataAndroidBridge : public FormDataAndroidBridge {
 public:
  MockFormDataAndroidBridge();
  ~MockFormDataAndroidBridge() override;

  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobject>,
              GetOrCreateJavaPeer,
              (const FormData&,
               SessionId,
               base::span<const std::unique_ptr<FormFieldDataAndroid>>),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_MOCK_FORM_DATA_ANDROID_BRIDGE_H_
