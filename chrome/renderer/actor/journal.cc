// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/journal.h"

#include "base/rand_util.h"
#include "chrome/common/actor/actor_logging.h"

namespace actor {

namespace {
constexpr base::TimeDelta kMinTimeSinceLastLogBufferSend =
    base::Milliseconds(100);
constexpr base::TimeDelta kSendLogBufferDelay = base::Milliseconds(200);
}  // namespace

Journal::Journal() : current_id_(base::RandUint64()) {}
Journal::~Journal() {
  if (log_buffer_.size() > 0) {
    SendLogBuffer();
  }
}

void Journal::Bind(mojo::PendingAssociatedRemote<mojom::JournalClient> client) {
  client_.Bind(std::move(client));
  client_.reset_on_disconnect();
}

void Journal::Log(int32_t task_id,
                  std::string_view event,
                  std::string_view details) {
  ACTOR_LOG() << event << ": " << details;

  if (!client_) {
    return;
  }

  auto journal_entry = mojom::JournalEntry::New(
      mojom::JournalEntryType::kInstant, task_id, current_id_++,
      base::Time::Now(), std::string(event), std::string(details));
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

void Journal::SendLogBuffer() {
  last_log_buffer_send_ = base::TimeTicks::Now();
  if (client_) {
    client_->AddEntriesToJournal(std::move(log_buffer_));
  } else {
    ACTOR_LOG() << "Clearing journal entries";
    log_buffer_.clear();
  }
}

}  // namespace actor
