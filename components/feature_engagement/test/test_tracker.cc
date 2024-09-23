// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/test/test_tracker.h"

#include <utility>

#include "components/feature_engagement/internal/chrome_variations_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/init_aware_event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/noop_display_lock_controller.h"
#include "components/feature_engagement/internal/system_time_provider.h"
#include "components/feature_engagement/internal/tracker_impl.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

// static
std::unique_ptr<Tracker> CreateTestTracker() {
  return CreateTestTracker(nullptr);
}

// static
std::unique_ptr<Tracker> CreateTestTracker(
    std::unique_ptr<TrackerEventExporter> event_exporter) {
  auto configuration = std::make_unique<ChromeVariationsConfiguration>();
  configuration->LoadConfigs(Tracker::GetDefaultConfigurationProviders(),
                             GetAllFeatures(), GetAllGroups());

  auto storage_validator =
      std::make_unique<FeatureConfigEventStorageValidator>();
  storage_validator->InitializeFeatures(GetAllFeatures(), GetAllGroups(),
                                        *configuration);

  auto raw_event_model = std::make_unique<EventModelImpl>(
      std::make_unique<InMemoryEventStore>(), std::move(storage_validator));

  auto event_model =
      std::make_unique<InitAwareEventModel>(std::move(raw_event_model));

  return std::make_unique<TrackerImpl>(
      std::move(event_model), std::make_unique<NeverAvailabilityModel>(),
      std::move(configuration), std::make_unique<NoopDisplayLockController>(),
      std::make_unique<FeatureConfigConditionValidator>(),
      std::make_unique<SystemTimeProvider>(), std::move(event_exporter),
      nullptr);
}

}  // namespace feature_engagement
