// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/enterprise_reporting/history_converter.h"

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

using ::enterprise_reporting::mojom::ErpHistoryEvent;
using ::enterprise_reporting::mojom::ErpHistoryEventParameter;

namespace ash::reporting {
namespace {

TEST(HistoryConverterTest, StorageQueueEnqueueTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const storage_record = record->mutable_storage_queue_action();
  storage_record->set_priority(::reporting::FAST_BATCH);
  ::reporting::Status::StatusOK().SaveTo(storage_record->mutable_status());
  auto* const enqueue_record = storage_record->mutable_storage_enqueue();
  enqueue_record->set_sequencing_id(123L);
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("QueueAction")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name,
                              StrEq("EnqueueSeqId")),
                        Field(&ErpHistoryEventParameter::value, StrEq("123")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Priority")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("FAST_BATCH")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, StorageQueueDequeueTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const storage_record = record->mutable_storage_queue_action();
  storage_record->set_priority(::reporting::FAST_BATCH);
  ::reporting::Status::StatusOK().SaveTo(storage_record->mutable_status());
  auto* const dequeue_record = storage_record->mutable_storage_dequeue();
  dequeue_record->set_sequencing_id(123L);
  dequeue_record->set_records_count(5);
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("QueueAction")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name,
                              StrEq("DequeueSeqId")),
                        Field(&ErpHistoryEventParameter::value, StrEq("123")))),
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name,
                              StrEq("DequeueCount")),
                        Field(&ErpHistoryEventParameter::value, StrEq("5")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Priority")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("FAST_BATCH")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, EnqueueRecordCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const call_record = record->mutable_enqueue_record_call();
  call_record->set_destination(::reporting::TELEMETRY_METRIC);
  call_record->set_priority(::reporting::MANUAL_BATCH);
  ::reporting::Status::StatusOK().SaveTo(call_record->mutable_status());
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(
          AllOf(Field(&ErpHistoryEvent::call, StrEq("Enqueue")),
                Field(&ErpHistoryEvent::parameters,
                      UnorderedElementsAre(
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Destination")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("TELEMETRY_METRIC")))),
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Priority")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("MANUAL_BATCH")))))),
                Field(&ErpHistoryEvent::status, StrEq("OK")),
                Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, FlushPriorityCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const flush_record = record->mutable_flush_priority_call();
  flush_record->set_priority(::reporting::MANUAL_BATCH);
  ::reporting::Status::StatusOK().SaveTo(flush_record->mutable_status());
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("Flush")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(Pointee(AllOf(
                    Field(&ErpHistoryEventParameter::name, StrEq("Priority")),
                    Field(&ErpHistoryEventParameter::value,
                          StrEq("MANUAL_BATCH")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, UploadEncryptedRecordCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const upload_record = record->mutable_upload_encrypted_record_call();
  upload_record->set_upload_reason("MANUAL");
  upload_record->set_priority(::reporting::MANUAL_BATCH);
  ::reporting::Status::StatusOK().SaveTo(upload_record->mutable_status());
  {
    auto* const data_item = upload_record->add_items()->mutable_record();
    data_item->set_sequencing_id(123);
  }
  {
    auto* const gap_item = upload_record->add_items()->mutable_gap();
    gap_item->set_sequencing_id(124);
    gap_item->set_count(2);
  }
  {
    auto* const data_item = upload_record->add_items()->mutable_record();
    data_item->set_sequencing_id(126);
  }
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("Upload")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name, StrEq("Reason")),
                        Field(&ErpHistoryEventParameter::value,
                              StrEq("MANUAL")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Priority")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("MANUAL_BATCH")))),
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name, StrEq("Record")),
                        Field(&ErpHistoryEventParameter::value,
                              StrEq("seq=123")))),
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name, StrEq("Gap")),
                        Field(&ErpHistoryEventParameter::value,
                              StrEq("seq=124 count=2")))),
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name, StrEq("Record")),
                        Field(&ErpHistoryEventParameter::value,
                              StrEq("seq=126")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, ConfirmCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const confirm_record = record->mutable_confirm_record_upload_call();
  confirm_record->set_sequencing_id(123L);
  confirm_record->set_priority(::reporting::MANUAL_BATCH);
  ::reporting::Status::StatusOK().SaveTo(confirm_record->mutable_status());
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("Confirm")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name,
                              StrEq("Sequencing_id")),
                        Field(&ErpHistoryEventParameter::value, StrEq("123")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Priority")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("MANUAL_BATCH")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, ForceConfirmCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const confirm_record = record->mutable_confirm_record_upload_call();
  confirm_record->set_sequencing_id(123L);
  confirm_record->set_priority(::reporting::MANUAL_BATCH);
  confirm_record->set_force_confirm(true);
  ::reporting::Status::StatusOK().SaveTo(confirm_record->mutable_status());
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(AllOf(
          Field(&ErpHistoryEvent::call, StrEq("Confirm")),
          Field(&ErpHistoryEvent::parameters,
                UnorderedElementsAre(
                    Pointee(AllOf(
                        Field(&ErpHistoryEventParameter::name,
                              StrEq("Sequencing_id")),
                        Field(&ErpHistoryEventParameter::value, StrEq("123")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Force_confirm")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("True")))),
                    Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                        StrEq("Priority")),
                                  Field(&ErpHistoryEventParameter::value,
                                        StrEq("MANUAL_BATCH")))))),
          Field(&ErpHistoryEvent::status, StrEq("OK")),
          Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, BlockedRecordCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const call_record = record->mutable_blocked_record_call();
  call_record->set_destination(::reporting::OS_EVENTS);
  call_record->set_priority(::reporting::SECURITY);
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(
          AllOf(Field(&ErpHistoryEvent::call, StrEq("BlockedRecord")),
                Field(&ErpHistoryEvent::parameters,
                      UnorderedElementsAre(
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Destination")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("OS_EVENTS")))),
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Priority")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("SECURITY")))))),
                Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, BlockedDestinationsUpdatedCallTest) {
  ::reporting::ERPHealthData history_data;
  auto* const record = history_data.add_history();
  record->set_timestamp_seconds(9876L);
  auto* const call_record = record->mutable_blocked_destinations_updated_call();
  call_record->add_destinations(::reporting::TELEMETRY_METRIC);
  call_record->add_destinations(::reporting::LOCK_UNLOCK_EVENTS);
  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(Pointee(
          AllOf(Field(&ErpHistoryEvent::call, StrEq("BlockedDestinations")),
                Field(&ErpHistoryEvent::parameters,
                      UnorderedElementsAre(
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Destination")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("TELEMETRY_METRIC")))),
                          Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                              StrEq("Destination")),
                                        Field(&ErpHistoryEventParameter::value,
                                              StrEq("LOCK_UNLOCK_EVENTS")))))),
                Field(&ErpHistoryEvent::time, Eq(9876))))));
}

TEST(HistoryConverterTest, MultipleRecordsTest) {
  ::reporting::ERPHealthData history_data;

  {
    auto* const record = history_data.add_history();
    record->set_timestamp_seconds(9876L);
    auto* const call_record = record->mutable_enqueue_record_call();
    call_record->set_destination(::reporting::TELEMETRY_METRIC);
    call_record->set_priority(::reporting::MANUAL_BATCH);
    ::reporting::Status::StatusOK().SaveTo(call_record->mutable_status());
  }

  {
    auto* const record = history_data.add_history();
    record->set_timestamp_seconds(9876L);
    auto* const storage_record = record->mutable_storage_queue_action();
    storage_record->set_priority(::reporting::MANUAL_BATCH);
    ::reporting::Status::StatusOK().SaveTo(storage_record->mutable_status());
    auto* const enqueue_record = storage_record->mutable_storage_enqueue();
    enqueue_record->set_sequencing_id(123L);
  }

  {
    auto* const record = history_data.add_history();
    record->set_timestamp_seconds(9876L);
    auto* const upload_record = record->mutable_upload_encrypted_record_call();
    upload_record->set_upload_reason("MANUAL");
    upload_record->set_priority(::reporting::MANUAL_BATCH);
    ::reporting::Status::StatusOK().SaveTo(upload_record->mutable_status());
    {
      auto* const data_item = upload_record->add_items()->mutable_record();
      data_item->set_sequencing_id(120);
    }
    {
      auto* const gap_item = upload_record->add_items()->mutable_gap();
      gap_item->set_sequencing_id(121);
      gap_item->set_count(2);
    }
    {
      auto* const data_item = upload_record->add_items()->mutable_record();
      data_item->set_sequencing_id(123);
    }
  }

  const auto converted_history = ConvertHistory(history_data);

  EXPECT_THAT(
      converted_history->events,
      ElementsAre(
          Pointee(AllOf(
              Field(&ErpHistoryEvent::call, StrEq("Enqueue")),
              Field(&ErpHistoryEvent::parameters,
                    UnorderedElementsAre(
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Destination")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("TELEMETRY_METRIC")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Priority")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("MANUAL_BATCH")))))),
              Field(&ErpHistoryEvent::status, StrEq("OK")),
              Field(&ErpHistoryEvent::time, Eq(9876)))),
          Pointee(AllOf(
              Field(&ErpHistoryEvent::call, StrEq("QueueAction")),
              Field(&ErpHistoryEvent::parameters,
                    UnorderedElementsAre(
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("EnqueueSeqId")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("123")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Priority")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("MANUAL_BATCH")))))),
              Field(&ErpHistoryEvent::status, StrEq("OK")),
              Field(&ErpHistoryEvent::time, Eq(9876)))),
          Pointee(AllOf(
              Field(&ErpHistoryEvent::call, StrEq("Upload")),
              Field(&ErpHistoryEvent::parameters,
                    UnorderedElementsAre(
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Reason")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("MANUAL")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Priority")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("MANUAL_BATCH")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Record")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("seq=120")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Gap")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("seq=121 count=2")))),
                        Pointee(AllOf(Field(&ErpHistoryEventParameter::name,
                                            StrEq("Record")),
                                      Field(&ErpHistoryEventParameter::value,
                                            StrEq("seq=123")))))),
              Field(&ErpHistoryEvent::status, StrEq("OK")),
              Field(&ErpHistoryEvent::time, Eq(9876))))));
}
}  // namespace
}  // namespace ash::reporting
