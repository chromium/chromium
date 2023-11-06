// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/database_client.h"
#include "base/metrics/metrics_hashes.h"

namespace segmentation_platform {

DatabaseClient::StructuredEvent::StructuredEvent() = default;

DatabaseClient::StructuredEvent::StructuredEvent(
    base::StringPiece event_name,
    const std::map<base::StringPiece, uint64_t> values) {
  event_id = UkmEventHash::FromUnsafeValue(base::HashMetricName(event_name));
  for (const auto& it : values) {
    metric_hash_to_value[UkmMetricHash::FromUnsafeValue(
        base::HashMetricName(it.first))] = it.second;
  }
}

DatabaseClient::StructuredEvent::~StructuredEvent() = default;

DatabaseClient::DatabaseClient() = default;
DatabaseClient::~DatabaseClient() = default;

}  // namespace segmentation_platform
