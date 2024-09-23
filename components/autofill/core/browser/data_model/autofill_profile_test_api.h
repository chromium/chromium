// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"

namespace autofill {

class AutofillProfileTestApi {
 public:
  explicit AutofillProfileTestApi(AutofillProfile* profile)
      : profile_(*profile) {}

  void set_record_type(AutofillProfile::RecordType record_type) {
    profile_->record_type_ = record_type;
  }

  // Same as `AutofillProfile::operator==`, but cares about differences in usage
  // stats.
  bool EqualsIncludingUsageStats(const AutofillProfile& other) const;

 private:
  const raw_ref<AutofillProfile> profile_;
};

inline AutofillProfileTestApi test_api(AutofillProfile& profile) {
  return AutofillProfileTestApi(&profile);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PROFILE_TEST_API_H_
