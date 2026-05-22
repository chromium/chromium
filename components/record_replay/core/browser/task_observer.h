// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_OBSERVER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_OBSERVER_H_

#include "base/functional/callback.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "url/gurl.h"

namespace record_replay {

class TaskObserver {
 public:
  using CompletionCallback =
      base::RepeatingCallback<void(const TaskObservation&)>;

  TaskObserver(const TaskDefinition& definition,
               CompletionCallback completion_callback);
  ~TaskObserver();
  TaskObserver(const TaskObserver&) = delete;
  TaskObserver& operator=(const TaskObserver&) = delete;

  // Creates a new Observation and records its metadata.
  void StartObserving(const GURL& visited_url);

  // Observes URL navigations. Checks if the URL is the final one or one of the
  // steps in the journey.
  void OnURLVisited(const GURL& visited_url);

  // Triggered after the task parameter extractor finishes filling extracted
  // data to the task observation. Receives a boolean indicating whether
  // parameter extraction was successful.
  void OnTaskParameterExtractionCompleted(bool success);

  const TaskObservation& observation() const { return observation_; }

 private:
  TaskObservation observation_;
  CompletionCallback completion_callback_;
  GURL final_url_;  // Caches the parsed final/target URL
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_OBSERVER_H_
