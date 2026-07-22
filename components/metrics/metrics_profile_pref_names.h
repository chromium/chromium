// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_PROFILE_PREF_NAMES_H_
#define COMPONENTS_METRICS_METRICS_PROFILE_PREF_NAMES_H_

namespace metrics::prefs {

// Boolean pref indicating whether advanced metrics reporting (including UKM) is
// enabled at the profile level.
// TODO(crbug.com/483042750): Feature under development.
inline constexpr char kAdvancedReportingEnabled[] =
    "metrics.advanced_reporting_enabled";

// Boolean pref indicating whether profile migration to the new advanced
// reporting consent state is done.
// TODO(crbug.com/483042750): Feature under development.
inline constexpr char kAdvancedReportingProfileMigrationDone[] =
    "metrics.advanced_reporting_profile_migration_done";

}  // namespace metrics::prefs

#endif  // COMPONENTS_METRICS_METRICS_PROFILE_PREF_NAMES_H_
