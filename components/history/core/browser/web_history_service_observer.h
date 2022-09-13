// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_OBSERVER_H_

namespace history {

// Observes WebHistoryService actions. Currently only history expiration
// is supported.
// TODO(msramek): Consider defining ObservableHistoryService as a common
// abstract ancestor for HistoryService and WebHistoryService. Then, we could
// change the signatures of HistoryServiceObserver to use it for both classes.
class WebHistoryServiceObserver {
 public:
  // Called after a successful WebHistoryService deletion request. This could
  // be partial or complete history deletion.
  virtual void OnWebHistoryDeleted() = 0;

 protected:
  virtual ~WebHistoryServiceObserver() {}
};

}  // history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_WEB_HISTORY_SERVICE_OBSERVER_H_
