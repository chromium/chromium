// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_

#include <string>
#include <utility>
#include <vector>

#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics {
namespace structured {

// All events must be converted to EventBase to be recorded and persisted to
// disk.
//
// TODO(jongahn): Write directly from Event to disk without making this
// conversion.
class EventBase {
 public:
  EventBase(const EventBase& other);
  virtual ~EventBase();

  // Specifies which value type a Metric object holds.
  enum class MetricType {
    kHmac = 0,
    kInt = 1,
    kRawString = 2,
  };

  // Stores all information about a single metric: name hash, value, and a
  // specifier of the value type.
  struct Metric {
    Metric(uint64_t name_hash, MetricType type);
    ~Metric();

    bool operator==(const Metric& other) const;

    // First 8 bytes of the MD5 hash of the metric name, as defined in
    // structured.xml. This is calculated by
    // tools/metrics/structured/codegen.py.
    uint64_t name_hash;
    MetricType type;

    // TODO(crbug.com/10116655): Replace this with a base::Value.
    // All possible value types a metric can take. Exactly one of these should
    // be set. If |hmac_value| is set (with |type| as MetricType::kHmac),
    // only the HMAC digest will be reported, so it is safe to put any value
    // here. If |raw_string_value| is set (with |type| as MetricType::kString),
    // the unprocessed string will be reported.
    std::string hmac_value;
    int64_t int_value = 0;
    std::string string_value;
  };

  // Returns when the key for |project_name_hash| was last rotated, in days
  // since epoch. Returns nullopt if the information is not available.
  absl::optional<int> LastKeyRotation();

  std::vector<Metric> metrics() const { return metrics_; }

  uint64_t name_hash() const { return event_name_hash_; }

  uint64_t project_name_hash() const { return project_name_hash_; }

  IdType id_type() const { return id_type_; }

  IdScope id_scope() const { return id_scope_; }

  EventType event_type() const { return event_type_; }

  int key_rotation_period() const { return key_rotation_period_; }

  // Converts an unhashed,raw |event| into an EventBase. If |event| is
  // malformatted (ie wrong metric name or metric vlaue type) or is not
  // registered within structured.xml, then returns absl::nullopt.
  static absl::optional<EventBase> FromEvent(const Event& event);

 protected:
  EventBase(uint64_t event_name_hash,
            uint64_t project_name_hash,
            IdType id_type,
            IdScope id_scope,
            EventType event_type,
            int max_key_rotation_period);

  void AddHmacMetric(uint64_t name_hash, const std::string& value);

  void AddIntMetric(uint64_t name_hash, int64_t value);

  void AddRawStringMetric(uint64_t name_hash, const std::string& value);

 private:
  // First 8 bytes of the MD5 hash of the event name, as defined in
  // structured.xml. This is calculated by tools/metrics/structured/codegen.py.
  uint64_t event_name_hash_;

  // The project name hash is used to to determine which key to use for hashing
  // events. The project name comes from this event's definition in
  // structured.xml, and is decided by the rules:
  //
  //  - if this event references a project, eg. <event name="..."
  //    project="...">, use that project's name.
  //
  //  - otherwise, use the event's name.
  //
  // |project_name_hash_| is the first 8 bytes of the MD5 hash of the project
  // name.
  uint64_t project_name_hash_;

  // See enum definition.
  IdType id_type_;

  // See enum definition.
  IdScope id_scope_;

  // Specifies the type of an event, which determines how it is treated after
  // upload. See /third_party/metrics_proto/structured_data.proto for more
  // information.
  EventType event_type_;

  // Key rotation period for this event.
  int key_rotation_period_;

  std::vector<Metric> metrics_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
