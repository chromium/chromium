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

namespace metrics::structured {

class EventsProto;
class ExternalMetricsTest;

// ExternalMetrics reads structured metrics saved by ChromeOS and uploads them
// to the UMA server on its behalf. This is structured metrics' equivalent of
// `ash::ExternalMetrics`.
//
// When a call to CollectEvents() is done, events in that directory will be
// added to StructuredMetricsRecorder. However, if the user has disabled upload,
// then all events will be dropped and deleted.
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

  void EnableRecording();
  void DisableRecording();

 private:
  friend class ExternalMetricsTest;

  void ScheduleCollector();
  void CollectEventsAndReschedule();

  // Events are collected from |events_directory|. Each file is processed by:
  //
  //    1. Stat on a file to see if it contains any events or if
  //       something in ChromeOS has already deleted the file.
  //    2. Obtains a flock.
  //    3. File is processed and read into memory.
  //    4. File is deleted.
  //    5. Flock is released.
  //
  // The above is done to avoid reading files mid-write from ChromeOS and having
  // ChromeOS write files after they were marked for deletion by Chrome.
  //
  // Files will be ignored if there is failure to obtain the flock or get the
  // file descriptor.
  //
  // Files will be dropped when an event file exceeds a fixed threshold provided
  // by GetFileSizeByteLimit(). Files will also be dropped if there are too many
  // event files existing on the current iteration provided by
  // GetFileLimitPerScan().
  void CollectEvents();

  // Builds a cache of disallow projects from the Finch controlled variable.
  void CacheDisallowedProjectsSet();

  bool recording_enabled_ = false;

  const base::FilePath events_directory_;
  const base::TimeDelta collection_interval_;
  MetricsCollectedCallback callback_;

  // A set of projects that are not allowed to be recorded. This is a cache of
  // GetDisabledProjects().
  base::flat_set<uint64_t> disallowed_projects_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ExternalMetrics> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_
