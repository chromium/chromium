// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/signal_handler.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/history_service_observer.h"
#include "components/segmentation_platform/internal/signals/signal_filter_processor.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform {

namespace {
BASE_FEATURE(kSegmentationPlatformWriteUmaToSqlDb,
             "SegmentationPlatformWriteUmaToSqlDb",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

SignalHandler::SignalHandler() = default;
SignalHandler::~SignalHandler() = default;

void SignalHandler::Initialize(
    StorageService* storage_service,
    history::HistoryService* history_service,
    PrefService* profile_prefs,
    const base::flat_set<proto::SegmentId>& segment_ids,
    const std::string& profile_id,
    base::RepeatingClosure models_refresh_callback) {
  UkmDatabase* ukm_db = nullptr;
  if (base::FeatureList::IsEnabled(kSegmentationPlatformWriteUmaToSqlDb) &&
      storage_service->ukm_data_manager()->HasUkmDatabase()) {
    ukm_db = storage_service->ukm_data_manager()->GetUkmDatabase();
  }
  if (ukm_db && profile_prefs->GetTime(kSegmentationUmaSqlDatabaseStartTimePref)
                    .is_null()) {
    profile_prefs->SetTime(kSegmentationUmaSqlDatabaseStartTimePref,
                           base::Time::Now());
  }
  user_action_signal_handler_ = std::make_unique<UserActionSignalHandler>(
      profile_id, storage_service->signal_database(), ukm_db);
  histogram_signal_handler_ = std::make_unique<HistogramSignalHandler>(
      profile_id, storage_service->signal_database(), ukm_db);

  if (storage_service->ukm_data_manager()->IsUkmEngineEnabled() &&
      history_service) {
    // TODO(b/290821132): Remove this check.
    if (!storage_service->ukm_data_manager()->HasUkmDatabase()) {
      CHECK_IS_TEST();
    } else {
      // If UKM engine is enabled and history service is not available, then we
      // would write metrics without URLs to the database, which is OK.
      history_service_observer_ = std::make_unique<HistoryServiceObserver>(
          history_service, storage_service, profile_id,
          models_refresh_callback);
    }
  }

  signal_filter_processor_ = std::make_unique<SignalFilterProcessor>(
      storage_service, user_action_signal_handler_.get(),
      histogram_signal_handler_.get(), history_service_observer_.get(),
      segment_ids);
}

void SignalHandler::TearDown() {
  if (histogram_signal_handler_) {
    signal_filter_processor_.reset();
    history_service_observer_.reset();
  }
}

void SignalHandler::EnableMetrics(bool signal_collection_allowed) {
  signal_filter_processor_->EnableMetrics(signal_collection_allowed);
}

void SignalHandler::OnSignalListUpdated() {
  signal_filter_processor_->OnSignalListUpdated();
}

}  // namespace segmentation_platform
