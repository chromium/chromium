// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_DATABASE_CLIENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_DATABASE_CLIENT_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

using UkmEventHash = base::IdTypeU64<class UkmEventHashTag>;
using UkmMetricHash = base::IdTypeU64<class UkmMetricHashTag>;

// Experimental database API for storing and retrieving signals from the
// segmentation databases. The API may change in the future, without breaking
// clients as possible. The API allows querying all the data from segmentation
// databases, which include UMA, user action, and UKM metrics. The metrics
// needed to be queried should be registered in `DatabaseApiClients` in
// `kRegisteredCustomEvents`.
class DatabaseClient {
 public:
  DatabaseClient();
  virtual ~DatabaseClient();

  DatabaseClient(const DatabaseClient&) = delete;
  DatabaseClient& operator=(const DatabaseClient&) = delete;

  enum class ResultStatus { kSuccess, kError };

  // Use `MetadataWriter` to add queries to the `metadata` for querying the
  // features. Most queries work based on a time range with the `end_time`.
  using FeaturesCallback =
      base::OnceCallback<void(ResultStatus, const ModelProvider::Request&)>;
  virtual void ProcessFeatures(const proto::SegmentationModelMetadata& metadata,
                               base::Time end_time,
                               FeaturesCallback callback) = 0;

  // Custom events to be added to the database. All custom events (and other
  // metrics too) have to be registered in `DatabaseApiClients`.
  struct StructuredEvent {
    StructuredEvent();
    StructuredEvent(std::string_view event_name,
                    const std::map<std::string, uint64_t> values);
    ~StructuredEvent();

    StructuredEvent(const StructuredEvent&) = delete;

    // ID for the event. Use HashHistogramName() to hash the event name
    // registered.
    UkmEventHash event_id;

    // The map of metric hash to the metric value for the event. Use
    // HashHistogramName() to hash the metric name registered. Note that each
    // event can have only one record of each metric.
    std::map<UkmMetricHash, int64_t> metric_hash_to_value;
  };

  // Adds event to the SQL database. The event will be removed after the defined
  // TTL in `DatabaseApiClients`. There is no support to delete or modify
  // written events, the database works like a log that can be queried.
  virtual void AddEvent(const StructuredEvent& event) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_DATABASE_CLIENT_H_
