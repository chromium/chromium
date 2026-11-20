// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_

#include <stddef.h>

#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"

namespace autofill::autofill_metrics {

// The data logged for the stored profiles metric. This is counted separately
// for each `AutofillProfileCategory`.
struct StoredProfileCounts {
  // Total number of stored profiles of the corresponding category.
  size_t total = 0;
  // The subset of profiles that hasn't been used in a fixed period of time.
  size_t disused = 0;
};

// Records statistics for the number of used, disused, and potentially,
// depending on the `category`, country-less address profiles.
// This metric is emitted each time a new Chrome profile is started. It is
// tracked separately for each `category`.
void LogStoredProfileCountStatistics(AutofillProfileRecordTypeCategory category,
                                     const StoredProfileCounts& counts);

// Records the number of days since an address profile was last used. This is
// logged separately for each profile of every `category`, each time a new
// Chrome profile is launched.
void LogStoredProfileDaysSinceLastUse(
    AutofillProfileRecordTypeCategory category,
    size_t days);

// Logs the `LogStoredProfileCountStatistics()` and
// `LogStoredProfileDaysSinceLastUse()` metrics for every
// AutofillProfileRecordTypeCategory and the corresponding subset of `profiles`.
void LogStoredProfileMetrics(
    const std::vector<const AutofillProfile*>& profiles);

// Categorization of the character script(s) used in an Autofill profile's
// name and address fields.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AutofillProfileCharacterScriptType)
enum class AutofillProfileCharacterScriptType {
  // Empty profile or contains only non-letter / ignored characters (e.g.
  // digits, punctuation, or fullwidth Latin characters).
  kUnspecified = 0,
  kAsciiOnly = 1,
  // Non-ASCII Latin characters are entirely within the Latin-1 Supplement block
  // [a-zA-ZÀ-ÿ] (U+00C0-U+00FF, e.g. é, è, ä, ö, ü, ß, ñ, ç, ó).
  kLatin1Supplement = 2,
  // Latin script containing at least one character outside the Basic Latin and
  // Latin-1 Supplement blocks (e.g. Latin Extended characters such as ł, ą, ę,
  // č, š, ž, even if combined with Latin-1 Supplement characters).
  kLatinExtended = 3,
  kCyrillic = 4,
  kArabic = 5,
  kCjk = 6,
  kIndic = 7,
  kGreek = 8,
  kHebrew = 9,
  kThai = 10,
  kOtherNonLatin = 11,
  kMixed = 12,
  kMaxValue = kMixed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillProfileCharacterScriptType)

// Logs the number of profiles that have an alternative name set.
void LogStoredProfileCountWithAlternativeName(
    base::span<const AutofillProfile* const> profiles);

// Logs `Autofill.StoredProfile.CharacterScriptType` (per profile) and
// `Autofill.StoredProfileCountByScriptType.*` (per user) for `profiles`.
// `profiles` must be non-empty. Safe to call on a background thread.
void LogStoredProfileScriptTypeMetrics(
    base::span<const AutofillProfile> profiles);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_STORED_PROFILE_METRICS_H_
