// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace record_replay {

class TaskStore;
class TaskDefinition;
class TaskObservation;
class TaskObserver;
class TaskParametersExtractor;

using TaskParameterValues =
    base::flat_map<int, base::flat_map<std::string, std::string>>;

// Service responsible for coordinating the lifecycle of automation tasks.
class TaskService : public KeyedService {
 public:
  using ExecutionCallback =
      base::RepeatingCallback<void(const TaskDefinition&,
                                   const TaskParameterValues&)>;

  TaskService(TaskStore* task_store,
              TaskParametersExtractor* task_parameters_extractor,
              ExecutionCallback execution_callback);
  TaskService(const TaskService&) = delete;
  TaskService& operator=(const TaskService&) = delete;
  TaskService(TaskService&&) = delete;
  TaskService& operator=(TaskService&&) = delete;
  ~TaskService() override;

  // Triggered upon top-level navigations. This will check whether or not to
  // start observing a task. If necessary, it will create a TaskObserver.
  void OnURLVisited(const GURL& visited_url);

  // Triggered when we land on the task-end URL, receives a TaskObservation
  // populated with TabParameterValues. The TaskObserver will receive this
  // as a callback and call it when it recognizes that the task ended.
  void OnTaskCompleted(const TaskObservation& observation);

  // This will be given as a callback to the UI that offers task execution.
  void OnExecutionAccepted(const TaskDefinition& definition,
                           const TaskParameterValues& values);

  // For testing purposes only.
  const std::unique_ptr<TaskObserver>& getObserverForTesting() const {
    return observer_;
  }
 private:
  void OnTaskDefinitionsRetrieved(const GURL& visited_url,
                                  std::vector<TaskDefinition> task_definitions);

  void StartObserving(const GURL& visited_url, TaskDefinition definition);
  void OfferExecuting(const GURL& visited_url, TaskDefinition definition);

  raw_ptr<TaskStore> task_store_;
  raw_ptr<TaskParametersExtractor> task_parameters_extractor_;

  // For simplification, we assume there is just one task to be observed at one
  // time, and therefore there is just one observer.
  std::unique_ptr<TaskObserver> observer_;
  ExecutionCallback execution_callback_;

  base::WeakPtrFactory<TaskService> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_SERVICE_H_
