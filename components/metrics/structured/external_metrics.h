// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_
#define COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"

namespace metrics {
namespace structured {

class EventsProto;
class ExternalMetricsTest;

// ExternalMetrics reads structured metrics saved by Chrome OS and uploads them
// to the UMA server on its behalf. This is structured metrics' equivalent of
// chromeos::ExternalMetrics.
//
// Chrome periodically reads a directory of protos and adds their content into
// the StructuredMetricProvider's regular metrics upload. After reading each
// file, it is deleted. Chrome and ChromeOS use flock to prevent concurrent
// read/writes.
class ExternalMetrics {
 public:
  using MetricsCollectedCallback = base::RepeatingCallback<void(EventsProto)>;

  ExternalMetrics(const base::FilePath& events_directory,
                  const base::TimeDelta& collection_interval,
                  MetricsCollectedCallback callback);
  ~ExternalMetrics();
  ExternalMetrics(const ExternalMetrics&) = delete;
  ExternalMetrics& operator=(const ExternalMetrics&) = delete;

 private:
  friend class ExternalMetricsTest;

  void ScheduleCollector();
  void CollectEventsAndReschedule();
  void CollectEvents();
  void OnEventsCollected(EventsProto events);

  const base::FilePath events_directory_;
  const base::TimeDelta collection_interval_;
  MetricsCollectedCallback callback_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ExternalMetrics> weak_factory_{this};
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EXTERNAL_METRICS_H_
