// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/structured/enums.h"

// Builder classes for sending events are generated in
// //components/metrics/structured/structured_events.h based on XML
// configuration.

namespace metrics::structured {

// Event to be built and sent by StructuredMetrics clients. Builder
// classes will be codegen'd from metrics definitions in structured.xml, but
// clients may choose to use this class directly to build Events.
class COMPONENT_EXPORT(METRICS_STRUCTURED) Event {
 public:
  // There should be a 1-1 mapping between MetricType and the mojom enums.
  //
  // kInt is used to represent enums.
  //
  // TODO(jongahn): Move this into common enum file.
  enum class MetricType {
    kHmac = 0,
    kLong = 1,
    kInt = 2,
    kDouble = 3,
    kRawString = 4,
    kBoolean = 5,
  };

  // Holds the value and the type of the metric encoded.
  struct COMPONENT_EXPORT(METRICS_STRUCTURED) MetricValue {
    MetricValue() = default;
    MetricValue(MetricType type, base::Value value);

    MetricValue(MetricValue&& other);
    MetricValue& operator=(MetricValue&& other);

    bool operator==(const MetricValue& rhs) const;
    ~MetricValue();

    MetricType type;
    base::Value value;
  };

  // Special metadata if event is a sequence project.
  struct COMPONENT_EXPORT(METRICS_STRUCTURED) EventSequenceMetadata {
    explicit EventSequenceMetadata(int reset_counter);
    ~EventSequenceMetadata();

    EventSequenceMetadata(const EventSequenceMetadata& other);
    EventSequenceMetadata& operator=(const EventSequenceMetadata& other);

    // Reset counter used for sequencing events across resets.
    int reset_counter;

    // UUIDv4 generated for every event. This does not contain any timestamp
    // information.
    std::string event_unique_id;
  };

  Event();
  Event(const std::string& project_name, const std::string& event_name);
  Event(const std::string& project_name,
        const std::string& event_name,
        bool is_event_sequence);

  Event(Event&& other);
  Event& operator=(Event&& other);

  virtual ~Event();

  // Whether |this| event part of a sequence.
  bool IsEventSequenceType() const;

  Event Clone() const;

  // Returns true if the value was added successfully. |type| and type of
  // |value| must be consistent and will be enforced. If the data in |value| and
  // |type| do match, then |value| will be moved into |this| when called.
  bool AddMetric(const std::string& metric_name,
                 MetricType type,
                 base::Value&& value);

  // Sets the metadata into |this|. If |IsEventSequenceType()| is false, then
  // this will no-op.
  void SetEventSequenceMetadata(
      const EventSequenceMetadata& event_sequence_metadata);

  // Explicitly set the system uptime.
  void SetRecordedTimeSinceBoot(base::TimeDelta recorded_time_since_boot);

  const std::string& project_name() const { return project_name_; }
  const std::string& event_name() const { return event_name_; }
  bool is_event_sequence() const { return is_event_sequence_; }
  const std::map<std::string, MetricValue>& metric_values() const {
    return metric_values_;
  }
  bool has_system_uptime() const {
    return recorded_time_since_boot_.has_value();
  }
  const base::TimeDelta recorded_time_since_boot() const;
  const EventSequenceMetadata& event_sequence_metadata() const;

 private:
  std::string project_name_;
  std::string event_name_;
  std::map<std::string, MetricValue> metric_values_;

  // System uptime for which the event was recorded.
  std::optional<base::TimeDelta> recorded_time_since_boot_;

  std::optional<EventSequenceMetadata> event_sequence_metadata_;

  // Returns true if part of a sequence.
  bool is_event_sequence_ = false;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_H_
