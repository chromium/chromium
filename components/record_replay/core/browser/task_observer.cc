// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_observer.h"

#include "base/functional/bind.h"
#include "components/record_replay/core/browser/task_parameters_extractor.h"
#include "url/gurl.h"

namespace record_replay {

TaskObserver::TaskObserver(const TaskDefinition& definition,
                           CompletionCallback completion_callback,
                           TaskParametersExtractor* task_parameters_extractor)
    : completion_callback_(std::move(completion_callback)),
      final_url_(
          definition.task_steps().empty()
              ? GURL(definition.url())
              : GURL(definition.task_steps(definition.task_steps_size() - 1)
                         .url())),
      task_parameters_extractor_(task_parameters_extractor) {
  observation_.mutable_definition()->CopyFrom(definition);
}

TaskObserver::~TaskObserver() {
  if (task_parameters_extractor_) {
    task_parameters_extractor_->FinishExtraction();
  }
}

void TaskObserver::StartObserving(const GURL& visited_url) {
  // TODO(crbug.com/514323476): Implement.
  // Creates a new Observation and records its metadata.
  // In a skeleton implementation, this initiates the observation tracking.
  if (task_parameters_extractor_) {
    task_parameters_extractor_->StartExtraction(observation_.definition());
  }
}

void TaskObserver::OnURLVisited(const GURL& visited_url) {
  // TODO(crbug.com/517491709): Handle special cases of reaching the final URL:
  // it's the same as the starting URL, it's never reached, it's reached before
  // the parameter extraction is completed, etc.
  if (visited_url == final_url_) {
    if (task_parameters_extractor_) {
      task_parameters_extractor_->FillExtractedParametersTo(
          &observation_,
          base::BindOnce(&TaskObserver::OnTaskParameterExtractionCompleted,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void TaskObserver::OnTaskParameterExtractionCompleted(bool success) {
  // TODO(crbug.com/517019298): Handle the case of failed parameter extraction.
  if (task_parameters_extractor_) {
    task_parameters_extractor_->FinishExtraction();
  }
  completion_callback_.Run(observation_);
}

}  // namespace record_replay
