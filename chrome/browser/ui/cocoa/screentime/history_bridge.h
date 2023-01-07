// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_H_

#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace screentime {

class HistoryDeleter;

// A HistoryBridge connects a HistoryService to a HistoryDeleter, which wraps
// the system ScreenTime backend. HistoryBridge is responsible for observing
// deletions of part or all of the history in a HistoryService and deleting the
// corresponding history from ScreenTime. It passes these to the provided
// HistoryDeleter, which proxies to the system API (when in production use) or
// to a test fake.
class HistoryBridge : public KeyedService,
                      public history::HistoryServiceObserver {
 public:
  HistoryBridge(history::HistoryService* history_service,
                std::unique_ptr<HistoryDeleter> deleter);
  HistoryBridge(const HistoryBridge& other) = delete;
  HistoryBridge& operator=(const HistoryBridge& other) = delete;
  ~HistoryBridge() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  std::unique_ptr<HistoryDeleter> deleter_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observer_{this};
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_HISTORY_BRIDGE_H_
