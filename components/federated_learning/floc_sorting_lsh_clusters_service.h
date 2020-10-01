// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_SORTING_LSH_CLUSTERS_SERVICE_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_SORTING_LSH_CLUSTERS_SERVICE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/federated_learning/floc_id.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace federated_learning {

// Responsible for loading the sorting-lsh clusters with custom encoding and
// and calculating the sorting-lsh based floc.
//
// File reading and parsing is posted to |background_task_runner_|.
class FlocSortingLshClustersService {
 public:
  using ApplySortingLshCallback = base::OnceCallback<void(FlocId)>;

  class Observer {
   public:
    virtual void OnSortingLshClustersFileReady() = 0;
  };

  FlocSortingLshClustersService();
  virtual ~FlocSortingLshClustersService();

  FlocSortingLshClustersService(const FlocSortingLshClustersService&) = delete;
  FlocSortingLshClustersService& operator=(
      const FlocSortingLshClustersService&) = delete;

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetBackgroundTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  void ApplySortingLsh(const FlocId& raw_floc_id,
                       ApplySortingLshCallback callback);

  // Virtual for testing.
  virtual void OnSortingLshClustersFileReady(const base::FilePath& file_path);

 private:
  friend class FlocSortingLshClustersServiceTest;

  // Runner for tasks that do not influence user experience.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::Optional<base::FilePath> sorting_lsh_clusters_file_path_;

  base::WeakPtrFactory<FlocSortingLshClustersService> weak_ptr_factory_;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_SORTING_LSH_CLUSTERS_SERVICE_H_
