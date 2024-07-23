// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_TEST_API_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/profile_token_quality.h"

namespace autofill {

// Exposes some testing operations for `ProfileTokenQuality`.
class ProfileTokenQualityTestApi {
 public:
  using FormSignatureHash = ProfileTokenQuality::FormSignatureHash;

  explicit ProfileTokenQualityTestApi(ProfileTokenQuality* quality);

  void AddObservation(FieldType field_type,
                      ProfileTokenQuality::ObservationType observation_type);

  void AddObservation(FieldType field_type,
                      ProfileTokenQuality::ObservationType observation_type,
                      FormSignatureHash hash);

  std::vector<FormSignatureHash> GetHashesForStoredType(FieldType type) const;

  void disable_randomization() {
    quality_->diable_randomization_for_testing_ = true;
  }

 private:
  raw_ref<ProfileTokenQuality> quality_;
};

inline ProfileTokenQualityTestApi test_api(ProfileTokenQuality& quality) {
  return ProfileTokenQualityTestApi(&quality);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PROFILE_TOKEN_QUALITY_TEST_API_H_
