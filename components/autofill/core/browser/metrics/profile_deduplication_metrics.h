// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_autofill_history.h"

namespace autofill::autofill_metrics {

// Used to store `CalculateMinimalIncompatibleTypeSets() ` result. It contains
// the `profile` that was being compared and a set of FieldTypes that had
// different values.
struct DifferingProfileWithTypeSet {
  const raw_ptr<const AutofillProfile> profile;
  const FieldTypeSet field_type_set;

  bool operator==(const DifferingProfileWithTypeSet& other) const = default;
};

// Given the result of `CalculateMinimalIncompatibleTypeSets()`, returns the
// minimum number of fields whose removal makes `import_candidate` a duplicate
// of any entry in `existing_profiles`. Returns
// `std::numeric_limits<int>::max()` in case `min_incompatible_sets` is empty.
int GetDuplicationRank(
    base::span<const DifferingProfileWithTypeSet> min_incompatible_sets);

// Logs various metrics around quasi duplicates (= profiles that are duplicates
// except for a small number of types) for the `profiles` a user has stored at
// browser startup.
void LogDeduplicationStartupMetrics(
    base::span<const AutofillProfile* const> profiles,
    std::string_view app_locale);

// Logs various metrics around quasi duplicates after the user was shown a
// new profile prompt for the `import_candidate`. `existing_profiles` are the
// other profiles this user has stored at the time of import, and
// `did_user_accept` indidcates if the user accepted (with or without edits) or
// declined the prompt.
void LogDeduplicationImportMetrics(
    bool did_user_accept,
    const AutofillProfile& import_candidate,
    base::span<const AutofillProfile* const> existing_profiles,
    std::string_view app_locale);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_
