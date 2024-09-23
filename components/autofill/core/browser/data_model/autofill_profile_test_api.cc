// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile_test_api.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"

namespace autofill {

bool AutofillProfileTestApi::EqualsIncludingUsageStats(
    const AutofillProfile& other) const {
  return profile_->use_count() == other.use_count() &&
         profile_->UseDateEqualsInSeconds(&other) && *profile_ == other;
}

}  // namespace autofill
