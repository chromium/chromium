// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_OBSERVER_H_
#define COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_OBSERVER_H_

// Observer class for the RepeatableQueriesService.
class RepeatableQueriesServiceObserver : public base::CheckedObserver {
 public:
  // Called after a Refresh() call on the service, either directly or as a
  // result of default search provider or signin status change. Note that this
  // is called after each Refresh(), even if the network request failed, or if
  // it didn't result in an actual change to the cached data. Observers can get
  // the repeatable queries via RepeatableQueriesService::repeatable_queries().
  virtual void OnRepeatableQueriesUpdated() = 0;

  // Called when the service is shutting down allowing the observers to
  // unregister themselves and clear references to the service.
  virtual void OnRepeatableQueriesServiceShuttingDown() {}
};

#endif  // COMPONENTS_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_OBSERVER_H_
