// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "components/android_autofill/browser/form_data_android.h"

namespace autofill {

class FormFieldDataAndroid;

class FormData;

// Interface for the C++ <-> Android bridge between `FormDataAndroid` and Java
// `FormData`.
// A note on lifetimes:
// - The C++ object, `FormDataAndroid`, owns this bridge.
// - It creates its Java `FormData` counterpart using `GetOrCreateJavaPeer` and
//   then passes ownership to Java where it is owned by an `AutofillRequest`.
//   It keeps a weak reference to it.
// - Subsequent calls to `GetOrCreateJavaPeer` either return a (co-owning)
//   reference to the existing Java `FormData` or, if `AutofillRequest` has
//   nulled its reference to it and it has been gargabe-collected, to a new
//   Java `FormData`.
class FormDataAndroidBridge {
 public:
  virtual ~FormDataAndroidBridge() = default;

  // Returns the Java `FormData` that this bridge keeps a (weak) reference to.
  // If the reference is null or has expired, it creates a new Java `FormData`.
  virtual base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaPeer(
      const FormData& form,
      SessionId session_id,
      base::span<const std::unique_ptr<FormFieldDataAndroid>>
          fields_android) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_BRIDGE_H_
