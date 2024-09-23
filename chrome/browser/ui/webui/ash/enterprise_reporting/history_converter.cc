// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/enterprise_reporting/history_converter.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chromeos/dbus/missive/history_tracker.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/util/status.h"

namespace ash::reporting {
namespace {
std::string StatusFromProto(const ::reporting::StatusProto& source) {
  ::reporting::Status status;
  status.RestoreFrom(source);
  return status.ToString();
}

void PopulateStorageQueueAction(
    const ::reporting::StorageQueueAction& source,
    enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "QueueAction";
  switch (source.action_case()) {
    case ::reporting::StorageQueueAction::ActionCase::kStorageEnqueue:
      dest.parameters.emplace_back(
          enterprise_reporting::mojom::ErpHistoryEventParameter::New(
              "EnqueueSeqId",
              base::NumberToString(source.storage_enqueue().sequencing_id())));
      break;
    case ::reporting::StorageQueueAction::ActionCase::kStorageDequeue:
      dest.parameters.emplace_back(
          enterprise_reporting::mojom::ErpHistoryEventParameter::New(
              "DequeueSeqId",
              base::NumberToString(source.storage_dequeue().sequencing_id())));
      dest.parameters.emplace_back(
          enterprise_reporting::mojom::ErpHistoryEventParameter::New(
              "DequeueCount",
              base::NumberToString(source.storage_dequeue().records_count())));
      break;
    default:
      dest.parameters.emplace_back(
          enterprise_reporting::mojom::ErpHistoryEventParameter::New(
              "UnknownAction", ""));
  }
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  dest.status = StatusFromProto(source.status());
}

void PopulateEnqueueRecord(const ::reporting::EnqueueRecordCall& source,
                           enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "Enqueue";
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Destination", ::reporting::Destination_Name(source.destination())));
  dest.status = StatusFromProto(source.status());
}

void PopulateFlushRecord(const ::reporting::FlushPriorityCall& source,
                         enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "Flush";
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  dest.status = StatusFromProto(source.status());
}

void PopulateConfirmRecord(const ::reporting::ConfirmRecordUploadCall& source,
                           enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "Confirm";
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Sequencing_id", base::NumberToString(source.sequencing_id())));
  if (source.force_confirm()) {
    dest.parameters.emplace_back(
        enterprise_reporting::mojom::ErpHistoryEventParameter::New(
            "Force_confirm", "True"));
  }
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  dest.status = StatusFromProto(source.status());
}

void PopulateUploadRecord(const ::reporting::UploadEncryptedRecordCall& source,
                          enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "Upload";
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Reason", source.upload_reason()));
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  for (const auto& item : source.items()) {
    switch (item.item_case()) {
      case ::reporting::UploadItem::ItemCase::kRecord:
        dest.parameters.emplace_back(
            enterprise_reporting::mojom::ErpHistoryEventParameter::New(
                "Record",
                base::StrCat({"seq=", base::NumberToString(
                                          item.record().sequencing_id())})));
        break;
      case ::reporting::UploadItem::ItemCase::kGap:
        dest.parameters.emplace_back(
            enterprise_reporting::mojom::ErpHistoryEventParameter::New(
                "Gap",
                base::StrCat(
                    {"seq=", base::NumberToString(item.gap().sequencing_id()),
                     " count=", base::NumberToString(item.gap().count())})));
        break;
      default:
        dest.parameters.emplace_back(
            enterprise_reporting::mojom::ErpHistoryEventParameter::New(
                "Unknown", ""));
    }
  }
  dest.status = StatusFromProto(source.status());
}

void PopulateBlockedRecord(const ::reporting::BlockedRecordCall& source,
    enterprise_reporting::mojom::ErpHistoryEvent& dest){
  dest.call = "BlockedRecord";
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Priority", ::reporting::Priority_Name(source.priority())));
  dest.parameters.emplace_back(
      enterprise_reporting::mojom::ErpHistoryEventParameter::New(
          "Destination", ::reporting::Destination_Name(source.destination())));
}

void PopulateBlockedDestinationsUpdated(
    const ::reporting::BlockedDestinationsUpdatedCall& source,
    enterprise_reporting::mojom::ErpHistoryEvent& dest) {
  dest.call = "BlockedDestinations";
  for (const auto& item : source.destinations()) {
    dest.parameters.emplace_back(
        enterprise_reporting::mojom::ErpHistoryEventParameter::New(
            "Destination", ::reporting::Destination_Name(item)));
  }
}
}  // namespace

mojo::StructPtr<enterprise_reporting::mojom::ErpHistoryData> ConvertHistory(
    const ::reporting::ERPHealthData& data) {
  auto result = enterprise_reporting::mojom::ErpHistoryData::New();
  for (const auto& history : data.history()) {
    result->events.emplace_back(
        enterprise_reporting::mojom::ErpHistoryEvent::New());
    result->events.back()->time = history.timestamp_seconds();
    switch (history.record_case()) {
      case ::reporting::HealthDataHistory::RecordCase::kEnqueueRecordCall:
        PopulateEnqueueRecord(history.enqueue_record_call(),
                              *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::kFlushPriorityCall:
        PopulateFlushRecord(history.flush_priority_call(),
                            *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::
          kUploadEncryptedRecordCall:
        PopulateUploadRecord(history.upload_encrypted_record_call(),
                             *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::kConfirmRecordUploadCall:
        PopulateConfirmRecord(history.confirm_record_upload_call(),
                              *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::kStorageQueueAction:
        PopulateStorageQueueAction(history.storage_queue_action(),
                                   *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::kBlockedRecordCall:
        PopulateBlockedRecord(history.blocked_record_call(),
            *result->events.back());
        break;
      case ::reporting::HealthDataHistory::RecordCase::
          kBlockedDestinationsUpdatedCall:
        PopulateBlockedDestinationsUpdated(
            history.blocked_destinations_updated_call(),
            *result->events.back());
        break;

      default:
        result->events.back()->call = "UNKNOWN";
        result->events.back()->status = "N/A";
    }
  }
  return result;
}
}  // namespace ash::reporting
