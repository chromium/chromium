// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_deduplication_metrics.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/address_data_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::autofill_metrics {

namespace {

void LogDeduplicationStartupMetricsForProfile(
    const AutofillProfile& profile,
    std::vector<FieldTypeSet> min_incompatible_sets) {
  // TODO(b/325452461): Implement metrics.
}

}  // namespace

void LogDeduplicationStartupMetrics(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale) {
  auto log_metrics = [](std::vector<AutofillProfile> profiles,
                        const std::string& app_locale) {
    AutofillProfileComparator comparator(app_locale);
    for (AutofillProfile& profile : profiles) {
      LogDeduplicationStartupMetricsForProfile(
          profile, AddressDataCleaner::CalculateMinimalIncompatibleTypeSets(
                       profile, profiles, comparator));
    }
  };
  // Since computing the metrics is quadratic in `profiles.size()`, it is done
  // on a background thread. Create a copy of the `profiles`, to avoid passing
  // pointers between threads.
  std::vector<AutofillProfile> profiles_copy;
  for (const AutofillProfile* profile : profiles) {
    profiles_copy.push_back(*profile);
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(log_metrics, std::move(profiles_copy), app_locale));
}

}  // namespace autofill::autofill_metrics
