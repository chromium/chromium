// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_H_

namespace history_clusters {

// An abstract class for a task run by `HistoryClustersService`.
// It is an extension of `HistoryClustersService`; rather than pollute the
// latter's namespace with a bunch of callbacks.
class HistoryClustersServiceTask {
 public:
  virtual ~HistoryClustersServiceTask() = default;

  // Returns whether the task has completed.
  bool Done() { return done_; }

 protected:
  HistoryClustersServiceTask() = default;

  // Set to true when the task is done.
  bool done_ = false;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_HISTORY_CLUSTERS_SERVICE_TASK_H_
