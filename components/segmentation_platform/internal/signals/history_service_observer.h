// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

namespace segmentation_platform {

class HistoryDelegateImpl;
class UrlSignalHandler;

// Observes history service for visits.
class HistoryServiceObserver : public history::HistoryServiceObserver {
 public:
  HistoryServiceObserver(history::HistoryService* history_service,
                         UrlSignalHandler* url_signal_handler);
  ~HistoryServiceObserver() override;

  HistoryServiceObserver(HistoryServiceObserver&) = delete;
  HistoryServiceObserver& operator=(HistoryServiceObserver&) = delete;

  // history::HistoryServiceObserver impl:
  void OnURLVisited(history::HistoryService* history_service,
                    ui::PageTransition transition,
                    const history::URLRow& row,
                    const history::RedirectList& redirects,
                    base::Time visit_time) override;
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

 private:
  raw_ptr<UrlSignalHandler> url_signal_handler_;
  std::unique_ptr<HistoryDelegateImpl> history_delegate_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_HISTORY_SERVICE_OBSERVER_H_
