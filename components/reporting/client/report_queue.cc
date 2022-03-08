// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/report_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

namespace {

StatusOr<std::string> ValueToJson(const base::Value::Dict& record) {
  std::string json_record;
  if (!base::JSONWriter::Write(record, &json_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Provided record was not convertable to a std::string");
  }
  return json_record;
}

StatusOr<std::string> ProtoToString(
    const google::protobuf::MessageLite* record) {
  std::string protobuf_record;
  if (!record->SerializeToString(&protobuf_record)) {
    return Status(error::INVALID_ARGUMENT,
                  "Unabled to serialize record to string. Most likely due to "
                  "unset required fields.");
  }
  return protobuf_record;
}

}  // namespace

ReportQueue::~ReportQueue() = default;

void ReportQueue::Enqueue(base::StringPiece record,
                          Priority priority,
                          ReportQueue::EnqueueCallback callback) const {
  AddRecord(record, priority, std::move(callback));
}

void ReportQueue::Enqueue(const base::Value::Dict& record,
                          Priority priority,
                          ReportQueue::EnqueueCallback callback) const {
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(std::string json_record, callback,
                                     ValueToJson(record));
  AddRecord(json_record, priority, std::move(callback));
}

void ReportQueue::Enqueue(const google::protobuf::MessageLite* record,
                          Priority priority,
                          ReportQueue::EnqueueCallback callback) const {
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(std::string protobuf_record, callback,
                                     ProtoToString(record));
  AddRecord(protobuf_record, priority, std::move(callback));
}

}  // namespace reporting
