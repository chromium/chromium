// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_OBSERVERS_HISTORY_DELETE_OBSERVER_H_
#define COMPONENTS_UKM_OBSERVERS_HISTORY_DELETE_OBSERVER_H_

#include <set>

#include "base/scoped_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace ukm {

// Observes multiple HistoryService objects for any events that delete history.
// Handles cleanup and removing observers as objects are destroyed.
class HistoryDeleteObserver : public history::HistoryServiceObserver {
 public:
  HistoryDeleteObserver();
  ~HistoryDeleteObserver() override;

  // Starts observing a service for history deletions.
  void ObserveServiceForDeletions(history::HistoryService* history_service);

  // history::HistoryServiceObserver
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 protected:
  virtual void OnHistoryDeleted() = 0;

 private:
  // Tracks observed history services, for cleanup.
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteObserver);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_OBSERVERS_HISTORY_DELETE_OBSERVER_H_
