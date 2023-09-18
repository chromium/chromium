// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_TEST_API_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_TEST_API_H_

#include "components/android_autofill/browser/autofill_provider_android.h"

namespace autofill {

class AutofillProviderAndroidTestApi {
 public:
  explicit AutofillProviderAndroidTestApi(AutofillProviderAndroid* provider)
      : provider_(*provider) {}

  const FormDataAndroid* form() && { return provider_->form_.get(); }

 private:
  const raw_ref<AutofillProviderAndroid> provider_;
};

inline AutofillProviderAndroidTestApi test_api(
    AutofillProviderAndroid& provider) {
  return AutofillProviderAndroidTestApi(&provider);
}

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_TEST_API_H_
