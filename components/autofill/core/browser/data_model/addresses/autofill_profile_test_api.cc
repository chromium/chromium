// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"

#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"

namespace autofill {

bool AutofillProfileTestApi::EqualsIncludingUsageStats(
    const AutofillProfile& other) const {
  return profile_->usage_history().use_count() ==
             other.usage_history().use_count() &&
         profile_->usage_history().UseDateEqualsInSeconds(
             other.usage_history()) &&
         *profile_ == other;
}

}  // namespace autofill
