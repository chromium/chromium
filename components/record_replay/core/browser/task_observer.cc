// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_observer.h"

#include "url/gurl.h"

namespace record_replay {

TaskObserver::TaskObserver(const TaskDefinition& definition,
                           CompletionCallback completion_callback)
    : completion_callback_(std::move(completion_callback)),
      final_url_(
          definition.task_steps().empty()
              ? GURL(definition.url())
              : GURL(definition.task_steps(definition.task_steps_size() - 1)
                         .url())) {
  observation_.mutable_definition()->CopyFrom(definition);
}

TaskObserver::~TaskObserver() = default;

void TaskObserver::StartObserving(const GURL& visited_url) {
  // TODO(crbug.com/514323476): Implement.
  // Creates a new Observation and records its metadata.
  // In a skeleton implementation, this initiates the observation tracking.
}

void TaskObserver::OnURLVisited(const GURL& visited_url) {
  if (visited_url == final_url_) {
    // TODO(crbug.com/514323476): Implement.
    // Perform parameter extraction and then call
    // TaskObserver::OnTaskCompleted.
  }
}

void TaskObserver::OnTaskParameterExtractionCompleted(bool success) {
  // TODO(crbug.com/514323476): Implement.
  // Triggered when we land on the task-end URL, receives a boolean indicating
  // whether parameter extraction was successful. Should trigger the
  // completion callback if successful.
}

}  // namespace record_replay
