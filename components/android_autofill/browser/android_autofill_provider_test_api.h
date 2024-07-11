// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_TEST_API_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_TEST_API_H_

#include "components/android_autofill/browser/android_autofill_provider.h"

namespace autofill {

class AndroidAutofillProviderTestApi {
 public:
  explicit AndroidAutofillProviderTestApi(AndroidAutofillProvider* provider)
      : provider_(*provider) {}

  const FormDataAndroid* form() && { return provider_->form_.get(); }
  const FieldGlobalId last_focused_field_id() && {
    return provider_->current_field_.id;
  }

  TouchToFillKeyboardSuppressor& keyboard_suppressor() {
    return *provider_->keyboard_suppressor_;
  }

 private:
  const raw_ref<AndroidAutofillProvider> provider_;
};

inline AndroidAutofillProviderTestApi test_api(
    AndroidAutofillProvider& provider) {
  return AndroidAutofillProviderTestApi(&provider);
}

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_TEST_API_H_
