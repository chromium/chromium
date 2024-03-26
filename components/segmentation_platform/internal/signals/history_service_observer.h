// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_

#include <optional>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

class HistoryDelegateImpl;
class StorageService;
class UrlSignalHandler;

// Observes history service for visits.
class HistoryServiceObserver : public history::HistoryServiceObserver {
 public:
  HistoryServiceObserver(history::HistoryService* history_service,
                         StorageService* storage_service,
                         const std::string& profile_id,
                         base::RepeatingClosure models_refresh_callback);
  // For tests.
  HistoryServiceObserver();
  ~HistoryServiceObserver() override;

  HistoryServiceObserver(const HistoryServiceObserver&) = delete;
  HistoryServiceObserver& operator=(const HistoryServiceObserver&) = delete;

  // history::HistoryServiceObserver impl:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // Sets the list of segment IDs that are based on history data.
  virtual void SetHistoryBasedSegments(
      base::flat_set<proto::SegmentId> history_based_segments);

 private:
  void DeleteResultsForHistoryBasedSegments();

  const raw_ptr<StorageService> storage_service_;
  const raw_ptr<UrlSignalHandler> url_signal_handler_;

  // List of segment IDs that depend on history data, that will be cleared when
  // history is deleted.
  std::optional<base::flat_set<proto::SegmentId>> history_based_segments_;
  bool pending_deletion_based_on_history_based_segments_ = false;

  base::RepeatingClosure models_refresh_callback_;
  std::unique_ptr<base::CancelableOnceClosure> posted_model_refresh_task_;

  const std::string profile_id_;
  std::unique_ptr<HistoryDelegateImpl> history_delegate_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_
