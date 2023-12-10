// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_STORAGE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_STORAGE_H_

#include "base/files/file_path.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics {
class StructuredEventProto;
class ChromeUserMetricsExtension;
}  // namespace metrics

namespace metrics::structured {

// Abstraction for how events are stored in Structured Metrics.
class EventStorage {
 public:
  EventStorage() = default;

  virtual ~EventStorage() = default;

  EventStorage(const EventStorage&) = delete;
  EventStorage& operator=(const EventStorage&) = delete;

  virtual bool IsReady();

  // A callback to be run when the storage is ready.
  virtual void OnReady() {}

  // Add a new StructuredEventProto to be stored.
  virtual void AddEvent(StructuredEventProto&& event) = 0;

  // Events are moved to UMA proto to be uploaded.
  virtual void MoveEvents(ChromeUserMetricsExtension& uma_proto) = 0;

  // The number of events that have been recorded.
  virtual int RecordedEventsCount() const = 0;

  // Checks if events have been stored.
  bool HasEvents() const { return RecordedEventsCount() > 0; }

  // Delete all events.
  virtual void Purge() = 0;

  // Temporary API for notifying storage that a profile has been added.
  virtual void OnProfileAdded(const base::FilePath& path) {}

  // Temporary API for external metrics.
  virtual void AddBatchEvents(
      const google::protobuf::RepeatedPtrField<StructuredEventProto>& events) {}
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_STORAGE_H_
