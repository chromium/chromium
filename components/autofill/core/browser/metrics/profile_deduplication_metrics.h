// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_profile.h"

namespace autofill::autofill_metrics {

// Logs various metrics around quasi duplicates (= profiles that are duplicates
// except for a small number of types) for the `profiles` a user has stored at
// browser startup.
void LogDeduplicationStartupMetrics(
    const std::vector<AutofillProfile*>& profiles,
    const std::string& app_locale);

// Logs various metrics around quasi duplicates after the user was shown a
// new profile prompt for the `import_candidate`. `existing_profiles` are the
// other profiles this user has stored at the time of import, and
// `did_user_accept` indidcates if the user accepted (with or without edits) or
// declined the prompt.
void LogDeduplicationImportMetrics(
    bool did_user_accept,
    const AutofillProfile& import_candidate,
    const std::vector<AutofillProfile*>& existing_profiles,
    const std::string& app_locale);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PROFILE_DEDUPLICATION_METRICS_H_
