// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/autofill/core/browser/test_address_normalizer.h"

namespace autofill {

TestAddressNormalizer::TestAddressNormalizer() = default;
TestAddressNormalizer::~TestAddressNormalizer() = default;

void TestAddressNormalizer::NormalizeAddressAsync(
    const AutofillProfile& profile,
    int timeout_seconds,
    AddressNormalizer::NormalizationCallback callback) {
  if (instantaneous_normalization_) {
    std::move(callback).Run(/*success=*/true, profile);
    return;
  }

  // Setup the necessary variables for the delayed normalization.
  profile_ = profile;
  callback_ = std::move(callback);
}

bool TestAddressNormalizer::NormalizeAddressSync(AutofillProfile* profile) {
  return true;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
TestAddressNormalizer::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>();
}
#endif  // BUILDFLAG(IS_ANDROID)

void TestAddressNormalizer::DelayNormalization() {
  instantaneous_normalization_ = false;
}

void TestAddressNormalizer::CompleteAddressNormalization() {
  DCHECK(instantaneous_normalization_ == false);
  std::move(callback_).Run(/*success=*/true, profile_);
}

}  // namespace autofill
