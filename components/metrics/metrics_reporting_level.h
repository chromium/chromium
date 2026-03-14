// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_REPORTING_LEVEL_H_
#define COMPONENTS_METRICS_METRICS_REPORTING_LEVEL_H_

namespace metrics {

// The three levels of the metrics reporting setting.
// TODO(b/492510818): This is part of a new feature being developed to
// restructure metrics privacy settings into a cleaner, three-state model.
enum class MetricsReportingLevel {
  kNone = 0,
  kBasic = 1,
  kAdvanced = 2,
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_REPORTING_LEVEL_H_
