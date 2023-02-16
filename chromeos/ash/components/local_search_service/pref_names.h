// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PREF_NAMES_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PREF_NAMES_H_

namespace ash::local_search_service::prefs {

// Integer pref used by the metrics::DailyEvent owned by
// local_search_service::SearchMetricsReporter.
extern const char kLocalSearchServiceMetricsDailySample[];
// TODO(thanhdng): clean this up after LSS is sandboxed.
extern const char kLocalSearchServiceSyncMetricsDailySample[];

// Integer prefs used to back event counts reported by
// local_search_service::SearchMetricsReporter.
extern const char kLocalSearchServiceMetricsCrosSettingsCount[];
extern const char kLocalSearchServiceMetricsHelpAppCount[];
extern const char kLocalSearchServiceMetricsHelpAppLauncherCount[];
extern const char kLocalSearchServiceMetricsPersonalizationCount[];
extern const char kLocalSearchServiceMetricsShortcutsAppCount[];
// TODO(thanhdng): clean this up after LSS is sandboxed.
extern const char kLocalSearchServiceSyncMetricsCrosSettingsCount[];
extern const char kLocalSearchServiceSyncMetricsHelpAppCount[];
extern const char kLocalSearchServiceSyncMetricsHelpAppLauncherCount[];

}  // namespace ash::local_search_service::prefs

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PREF_NAMES_H_
