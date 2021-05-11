// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_

#include <string>
#include <utility>
#include <vector>

namespace metrics {
namespace structured {

// A base class for generated structured metrics event objects. This class
// should not be used directly.
class EventBase {
 public:
  EventBase(const EventBase& other);
  virtual ~EventBase();

  // Specifies the type of identifier attached to an event.
  enum class IdType {
    // Events are attached to a per-event (or per-project) id.
    kProjectId = 0,
    // Events are attached to the UMA client_id.
    kUmaId = 1,
    // Events are attached to no id.
    kUnidentified = 2,
  };

  // Specifies which value type a Metric object holds.
  enum class MetricType {
    kString = 0,
    kInt = 1,
  };

  // Stores all information about a single metric: name hash, value, and a
  // specifier of the value type.
  struct Metric {
    Metric(uint64_t name_hash, MetricType type);
    ~Metric();

    // First 8 bytes of the MD5 hash of the metric name, as defined in
    // structured.xml. This is calculated by
    // tools/metrics/structured/codegen.py.
    uint64_t name_hash;
    MetricType type;

    // TODO(crbug.com/10116655): Replace this with a base::Value.
    // All possible value types a metric can take. Exactly one of these should
    // be set. If |string_value| is set (with |type| as MetricType::STRING),
    // only the HMAC digest will be reported, so it is safe to put any value
    // here.
    std::string string_value;
    int64_t int_value;
  };

  // Finalizes the event and sends it for recording. After this call, the event
  // is left in an invalid state and should not be used further.
  void Record();

  std::vector<Metric> metrics() const { return metrics_; }

  uint64_t name_hash() const { return event_name_hash_; }

  uint64_t project_name_hash() const { return project_name_hash_; }

  IdType id_type() const { return id_type_; }

 protected:
  explicit EventBase(uint64_t event_name_hash,
                     uint64_t project_name_hash,
                     IdType id_type);

  void AddStringMetric(uint64_t name_hash, const std::string& value);

  void AddIntMetric(uint64_t name_hash, int64_t value);

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

  IdType id_type_;

  std::vector<Metric> metrics_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
