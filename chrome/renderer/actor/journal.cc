// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/journal.h"

#include "base/rand_util.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"

namespace actor {

namespace {
constexpr base::TimeDelta kMinTimeSinceLastLogBufferSend =
    base::Milliseconds(100);
constexpr base::TimeDelta kSendLogBufferDelay = base::Milliseconds(200);

}  // namespace

Journal::PendingAsyncEntry::PendingAsyncEntry(base::PassKey<Journal> pass_key,
                                              base::SafeRef<Journal> journal,
                                              TaskId task_id,
                                              std::string_view event_name)
    : pass_key_(pass_key),
      journal_(journal),
      task_id_(task_id),
      event_name_(event_name) {}

Journal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!terminated_) {
    EndEntry({});
  }
}

void Journal::PendingAsyncEntry::EndEntry(
    std::vector<mojom::JournalDetailsPtr> details) {
  CHECK(!terminated_);
  terminated_ = true;
  ACTOR_LOG() << "End " << event_name_ << ": " << details;
  journal_->AddEndEvent(pass_key_, task_id_, event_name_, std::move(details));
}

void Journal::PendingAsyncEntry::Log(std::string_view event_name) {
  journal_->Log(task_id_, event_name, {});
}

void Journal::PendingAsyncEntry::Log(
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  journal_->Log(task_id_, event_name, std::move(details));
}

Journal::Journal() = default;
Journal::~Journal() {
  if (log_buffer_.size() > 0) {
    SendLogBuffer();
  }
}

void Journal::Bind(mojo::PendingAssociatedRemote<mojom::JournalClient> client) {
  client_.Bind(std::move(client));
  client_.reset_on_disconnect();
}

void Journal::Log(TaskId task_id,
                  std::string_view event,
                  std::vector<mojom::JournalDetailsPtr> details) {
  ACTOR_LOG() << event << ": " << details;

  if (!client_) {
    return;
  }

  auto journal_entry = mojom::JournalEntry::New(
      mojom::JournalEntryType::kInstant, task_id, base::Time::Now(),
      std::string(event), MakeRendererTrackUUID(task_id), std::move(details));

  AddJournalEntry(std::move(journal_entry));
}

std::unique_ptr<Journal::PendingAsyncEntry> Journal::CreatePendingAsyncEntry(
    TaskId task_id,
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  ACTOR_LOG() << "Begin " << event_name << ": " << details;

  AddJournalEntry(mojom::JournalEntry::New(
      mojom::JournalEntryType::kBegin, task_id, base::Time::Now(),
      std::string(event_name), MakeRendererTrackUUID(task_id),
      std::move(details)));
  return base::WrapUnique(new PendingAsyncEntry(base::PassKey<Journal>(),
                                                weak_factory_.GetSafeRef(),
                                                task_id, event_name));
}

void Journal::AddJournalEntry(mojom::JournalEntryPtr journal_entry) {
  log_buffer_.push_back(std::move(journal_entry));
  if (log_buffer_.size() > 1) {
    // A delayed task has already been posted for sending the buffer contents.
    return;
  }

  if ((base::TimeTicks::Now() - last_log_buffer_send_) >
      kMinTimeSinceLastLogBufferSend) {
    SendLogBuffer();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Journal::SendLogBuffer, weak_factory_.GetWeakPtr()),
        kSendLogBufferDelay);
  }
}

void Journal::AddEndEvent(base::PassKey<Journal> pass_key,
                          TaskId task_id,
                          const std::string& event_name,
                          std::vector<mojom::JournalDetailsPtr> details) {
  AddJournalEntry(mojom::JournalEntry::New(
      mojom::JournalEntryType::kEnd, task_id, base::Time::Now(), event_name,
      MakeRendererTrackUUID(task_id), std::move(details)));
}

void Journal::SendLogBuffer() {
  last_log_buffer_send_ = base::TimeTicks::Now();

  if (log_buffer_.empty()) {
    return;
  }

  if (client_) {
    client_->AddEntriesToJournal(std::move(log_buffer_));
  } else {
    ACTOR_LOG() << "Clearing journal entries";
    log_buffer_.clear();
  }
}

}  // namespace actor
