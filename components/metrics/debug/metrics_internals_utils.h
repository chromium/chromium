// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_UTILS_H_
#define COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_UTILS_H_

#include "base/values.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/variations/variations_seed_store.h"

namespace metrics {

base::Value::List GetUmaSummary(MetricsService* metrics_service);

base::Value::List GetVariationsSummary(
    metrics_services_manager::MetricsServicesManager* metrics_service_manager);

void GetStoredSeedInfo(
    base::OnceCallback<void(base::ValueView)> done_callback,
    metrics_services_manager::MetricsServicesManager* metrics_service_manager,
    variations::VariationsSeedStore::SeedType seed_type);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEBUG_METRICS_INTERNALS_UTILS_H_
