// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_
#define COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace metrics {
namespace structured {

class EventsProto;
class ExternalMetricsTest;

// ExternalMetrics reads structured metrics saved by Chrome OS and uploads them
// to the UMA server on its behalf. This is structured metrics' equivalent of
// `ash::ExternalMetrics`.
//
// Chrome periodically reads a directory of protos and adds their content into
// the StructuredMetricProvider's regular metrics upload. After reading each
// file, it is deleted. Chrome and ChromeOS use flock to prevent concurrent
// read/writes.
class ExternalMetrics {
 public:
  using MetricsCollectedCallback =
      base::RepeatingCallback<void(const EventsProto&)>;

  ExternalMetrics(const base::FilePath& events_directory,
                  const base::TimeDelta& collection_interval,
                  MetricsCollectedCallback callback);
  ~ExternalMetrics();
  ExternalMetrics(const ExternalMetrics&) = delete;
  ExternalMetrics& operator=(const ExternalMetrics&) = delete;

  // Adds a project to the disallowed list for testing.
  void AddDisallowedProjectForTest(uint64_t project_name_hash);

 private:
  friend class ExternalMetricsTest;

  void ScheduleCollector();
  void CollectEventsAndReschedule();
  void CollectEvents();

  // Builds a cache of disallow projects from the Finch controlled variable.
  void CacheDisallowedProjectsSet();

  const base::FilePath events_directory_;
  const base::TimeDelta collection_interval_;
  MetricsCollectedCallback callback_;

  // A set of projects that are not allowed to be recorded. This is a cache of
  // GetDisabledProjects().
  base::flat_set<uint64_t> disallowed_projects_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ExternalMetrics> weak_factory_{this};
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_
