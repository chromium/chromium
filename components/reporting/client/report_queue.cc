// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

namespace {

StatusOr<std::string> ValueToJson(base::Value::Dict record) {
  std::string json_record;
  if (!base::JSONWriter::Write(record, &json_record)) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Provided record was not convertable to a std::string"));
  }
  return json_record;
}

StatusOr<std::string> ProtoToString(
    std::unique_ptr<const google::protobuf::MessageLite> record) {
  std::string protobuf_record;
  if (!record->SerializeToString(&protobuf_record)) {
    return base::unexpected(
        Status(error::INVALID_ARGUMENT,
               "Unabled to serialize record to string. Most likely due to "
               "unset required fields."));
  }
  return protobuf_record;
}

void EnqueueResponded(ReportQueue::EnqueueCallback callback,
                      Destination destination,
                      Status status) {
  // Log the overall enqueue error status. Gives us insight to how the ERP is
  // performing in general.
  base::UmaHistogramEnumeration(ReportQueue::kEnqueueMetricsName, status.code(),
                                error::Code::MAX_VALUE);

  // Log the type of enqueue error (if any) for this destination. Gives us
  // insights as to what types of errors this destination experiences.
  base::UmaHistogramEnumeration(
      base::StrCat({ReportQueue::kEnqueueMetricsName, ".",
                    Destination_Name(destination)}),
      status.code(), error::Code::MAX_VALUE);

  // Log whether the enqueue succeeded or failed for this destination. Gives us
  // insight as to whether one destination experiences more failures than
  // others. If you find that a destination has more failures than other
  // destinations, it could mean several things:
  //
  // 1. That destination gets much more traffic and is naturally going to
  // experience more errors.
  //
  // 2. The code that enqueues records to that destination may be implemented
  // incorrectly.
  //
  // To dive further, look up the `Browser.ERP.EventEnqueueResult<Destination>`
  // UMA (logged above) to understand what types of errors the destination
  // experiences most often.
  const auto* const enqueue_destination_metrics_name =
      status.ok() ? ReportQueue::kEnqueueSuccessDestinationMetricsName
                  : ReportQueue::kEnqueueFailedDestinationMetricsName;
  base::UmaHistogramExactLinear(enqueue_destination_metrics_name,
                                static_cast<int>(destination),
                                Destination_ARRAYSIZE);
  std::move(callback).Run(std::move(status));
}
}  // namespace

ReportQueue::~ReportQueue() = default;

void ReportQueue::Enqueue(std::string record,
                          Priority priority,
                          ReportQueue::EnqueueCallback callback) const {
  AddProducedRecord(
      base::BindOnce([](std::string record)
                         -> StatusOr<std::string> { return std::move(record); },
                     std::move(record)),
      priority,
      base::BindOnce(&EnqueueResponded, std::move(callback), GetDestination()));
}

void ReportQueue::Enqueue(base::Value::Dict record,
                          Priority priority,
                          ReportQueue::EnqueueCallback callback) const {
  AddProducedRecord(
      base::BindOnce(&ValueToJson, std::move(record)), priority,
      base::BindOnce(&EnqueueResponded, std::move(callback), GetDestination()));
}

void ReportQueue::Enqueue(
    std::unique_ptr<const google::protobuf::MessageLite> record,
    Priority priority,
    ReportQueue::EnqueueCallback callback) const {
  AddProducedRecord(
      base::BindOnce(&ProtoToString, std::move(record)), priority,
      base::BindOnce(&EnqueueResponded, std::move(callback), GetDestination()));
}

}  // namespace reporting
