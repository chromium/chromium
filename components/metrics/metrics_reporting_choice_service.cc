// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_reporting_choice_service.h"

#include "base/check.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

MetricsReportingChoiceService::MetricsReportingChoiceService(
    PrefService* local_state)
    : local_state_(local_state) {
  CHECK(local_state_);
}

MetricsReportingChoiceService::~MetricsReportingChoiceService() = default;

// static
void MetricsReportingChoiceService::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // TODO(crbug.com/496476603): Register prefs used by this class.
}

}  // namespace metrics
