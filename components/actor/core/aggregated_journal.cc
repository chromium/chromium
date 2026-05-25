// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/aggregated_journal.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "components/actor/core/actor_logging.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/journal_details_builder.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace actor {

namespace {

bool ShouldLogJournal() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableActorJournalVLog);
  return enabled || VLOG_IS_ON(1);
}

std::string DetermineProtoType(std::string_view override_type,
                               const google::protobuf::MessageLite& message) {
  if (!override_type.empty()) {
    return std::string(override_type);
  }
  if (!message.GetTypeName().empty()) {
    return std::string(message.GetTypeName());
  }
  return "Unknown Proto";
}

std::string ToBase64(const google::protobuf::MessageLite& proto) {
  std::string buffer;
  proto.SerializeToString(&buffer);
  return base::Base64Encode(buffer);
}

}  // namespace

// Redefine ACTOR_LOG here to enable gathering logs from this file with the
// --enable-actor-journal-vlog flag, this is done because the default
// implementation on official Android builds removes all VLOGs. Limited to this
// file to minimize binary size impact.
#undef ACTOR_LOG
#define ACTOR_LOG() \
  LAZY_STREAM(VLOG_STREAM(1), ShouldLogJournal()) << "[ActorTool]: "

AggregatedJournal::Entry::Entry(const std::string& location,
                                mojom::JournalEntryPtr data_arg)
    : url(location), data(std::move(data_arg)) {}
AggregatedJournal::Entry::~Entry() = default;

AggregatedJournal::AggregatedJournal() = default;
AggregatedJournal::~AggregatedJournal() = default;

AggregatedJournal::PendingAsyncEntry::PendingAsyncEntry(
    base::PassKey<AggregatedJournal> pass_key,
    base::SafeRef<AggregatedJournal> journal,
    TaskId task_id,
    std::string_view event_name,
    uint64_t track_uuid)
    : pass_key_(pass_key),
      journal_(journal),
      task_id_(task_id),
      event_name_(event_name),
      begin_time_(base::TimeTicks::Now()),
      track_uuid_(track_uuid) {}

AggregatedJournal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!terminated_) {
    EndEntry({});
  }
}

void AggregatedJournal::PendingAsyncEntry::EndEntry(
    std::vector<mojom::JournalDetailsPtr> details) {
  CHECK(!terminated_);
  terminated_ = true;
  ACTOR_LOG() << "End " << event_name_ << ": " << details;
  journal_->AddEndEvent(pass_key_, task_id_, event_name_, track_uuid_,
                        std::move(details));
}

AggregatedJournal& AggregatedJournal::PendingAsyncEntry::GetJournal() {
  return *journal_;
}

TaskId AggregatedJournal::PendingAsyncEntry::GetTaskId() {
  return task_id_;
}

base::SafeRef<AggregatedJournal> AggregatedJournal::GetSafeRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetSafeRef();
}

base::WeakPtr<AggregatedJournal> AggregatedJournal::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

uint64_t AggregatedJournal::AllocateDynamicTrackUUID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static uint64_t next_track_id = 1000;
  return ++next_track_id;
}

std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
AggregatedJournal::CreatePendingAsyncEntry(
    const GURL& url,
    TaskId task_id,
    uint64_t track_uuid,
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ACTOR_LOG() << "Begin " << event_name << ": " << details;

  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kBegin, task_id,
                               base::Time::Now(), std::string(event_name),
                               track_uuid, std::move(details))));
  return std::make_unique<PendingAsyncEntry>(base::PassKey<AggregatedJournal>(),
                                             weak_ptr_factory_.GetSafeRef(),
                                             task_id, event_name, track_uuid);
}

void AggregatedJournal::Log(const GURL& url,
                            TaskId task_id,
                            std::string_view event_name,
                            std::vector<mojom::JournalDetailsPtr> details) {
  Log(url, task_id, MakeBrowserTrackUUID(task_id), event_name,
      std::move(details));
}

void AggregatedJournal::Log(const GURL& url,
                            TaskId task_id,
                            uint64_t track_uuid,
                            std::string_view event_name,
                            std::vector<mojom::JournalDetailsPtr> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ACTOR_LOG() << event_name << ": " << details;
  AddEntry(std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kInstant, task_id,
                               base::Time::Now(), std::string(event_name),
                               track_uuid, std::move(details))));
}

void AggregatedJournal::LogProto(const GURL& url,
                                 TaskId task_id,
                                 std::string_view event_name,
                                 std::vector<mojom::JournalDetailsPtr> details,
                                 const google::protobuf::MessageLite& message,
                                 std::string_view proto_type_override) {
  LogProto(url, task_id, MakeBrowserTrackUUID(task_id), event_name,
           std::move(details), message, proto_type_override);
}

void AggregatedJournal::LogProto(const GURL& url,
                                 TaskId task_id,
                                 uint64_t track_uuid,
                                 std::string_view event_name,
                                 std::vector<mojom::JournalDetailsPtr> details,
                                 const google::protobuf::MessageLite& message,
                                 std::string_view proto_type_override) {
  // TODO(b/512580434): Avoid logging protos in protos as base64 strings. Move
  // logging arbitrary fields as proto_bytes inside the stream.
  std::string base64_proto = ToBase64(message);
  std::string proto_type = DetermineProtoType(proto_type_override, message);

  JournalDetailsBuilder builder;
  builder.Add("proto_type", proto_type).Add("proto", base64_proto);
  for (auto& detail : details) {
    builder.Add(detail->key, detail->value);
  }

  Log(url, task_id, track_uuid, event_name, std::move(builder).Build());
}

void AggregatedJournal::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void AggregatedJournal::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void AggregatedJournal::AppendJournalEntries(
    const GURL& url,
    std::vector<mojom::JournalEntryPtr> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string location = url.possibly_invalid_spec();
  for (auto& renderer_entry : entries) {
    AddEntry(std::make_unique<Entry>(location, std::move(renderer_entry)));
  }
}

void AggregatedJournal::AddEndEvent(
    base::PassKey<AggregatedJournal> pass_key,
    TaskId task_id,
    const std::string& event_name,
    uint64_t track_uuid,
    std::vector<mojom::JournalDetailsPtr> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddEntry(std::make_unique<Entry>(
      std::string(),
      mojom::JournalEntry::New(mojom::JournalEntryType::kEnd, task_id,
                               base::Time::Now(), event_name, track_uuid,
                               std::move(details))));
}

void AggregatedJournal::LogScreenshot(const GURL& url,
                                      TaskId task_id,
                                      std::string_view mime_type,
                                      base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id, base::Time::Now(),
          "Screenshot", MakeBrowserTrackUUID(task_id),
          /*details=*/std::vector<mojom::JournalDetailsPtr>()));
  entry->screenshot.emplace(data.begin(), data.end());
  AddEntry(std::move(entry));
}

void AggregatedJournal::LogAnnotatedPageContent(
    const GURL& url,
    TaskId task_id,
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = std::make_unique<Entry>(
      url.possibly_invalid_spec(),
      mojom::JournalEntry::New(
          mojom::JournalEntryType::kInstant, task_id, base::Time::Now(),
          "PageContext", MakeBrowserTrackUUID(task_id),
          /*details=*/std::vector<mojom::JournalDetailsPtr>()));
  entry->annotated_page_content.emplace(data.begin(), data.end());
  AddEntry(std::move(entry));
}

void AggregatedJournal::AddEntry(std::unique_ptr<Entry> new_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.WillAddJournalEntry(*new_entry);
  }
  entries_.SaveToBuffer(std::move(new_entry));
}

}  // namespace actor
