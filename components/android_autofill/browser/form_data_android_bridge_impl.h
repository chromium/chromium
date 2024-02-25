// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_IMPL_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_IMPL_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/form_data_android_bridge.h"

namespace autofill {

class FormFieldDataAndroid;

class FormDataAndroidBridgeImpl : public FormDataAndroidBridge {
 public:
  FormDataAndroidBridgeImpl();
  ~FormDataAndroidBridgeImpl() override;

  // Returns the Java `FormData` that this bridge keeps a reference to. If
  // the reference is null or has expired, it creates a new Java
  // `FormData`.
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaPeer(
      const FormData& form,
      SessionId session_id,
      base::span<const std::unique_ptr<FormFieldDataAndroid>> fields_android)
      override;

 private:
  // A weak reference to the Java object.
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_IMPL_H_
