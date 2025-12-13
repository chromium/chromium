// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_JOURNAL_H_
#define CHROME_RENDERER_ACTOR_JOURNAL_H_

#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace actor {

// A logging class that records actions taken by the actor.
// This class employs a buffering strategy to minimize the number of
// IPCs sent to the browser. The following strategy is employed:
// 1) If there is a pending event in the buffer, don't do anything just
//    wait for the previous event logged to trigger (the 200ms timeout).
// 2) If it has been more than 100ms since we last sent data to the browser
//    send it immediately.
// 3) Otherwise schedule a timer to send the contents of the buffer in 200ms.
class Journal {
 public:
  Journal();
  ~Journal();

  // A pending async journal entry.
  class PendingAsyncEntry {
   public:
    // Creation of the event is only from the Journal itself. Use
    // `Journal::CreatePendingAsyncEntry` to create this object.
    PendingAsyncEntry(base::PassKey<Journal>,
                      base::SafeRef<Journal> journal,
                      TaskId task_id,
                      std::string_view event_name);
    ~PendingAsyncEntry();

    // End an pending entry with additional details. This can only be called
    // once and will be automatically called from the destructor if it hasn't
    // been called.
    void EndEntry(std::vector<mojom::JournalDetailsPtr> details);

    // Logs an instant event within the scope of this async entry.
    void Log(std::string_view event_name);
    void Log(std::string_view event_name,
             std::vector<mojom::JournalDetailsPtr> details);

   private:
    base::PassKey<Journal> pass_key_;
    bool terminated_ = false;
    base::SafeRef<Journal> journal_;
    TaskId task_id_;
    std::string event_name_;
  };

  void Bind(mojo::PendingAssociatedRemote<mojom::JournalClient> client);
  void Log(TaskId task_id,
           std::string_view event,
           std::vector<mojom::JournalDetailsPtr> details);

  // Create an async entry. This will log a Begin Entry event and when the
  // PendingAsyncEntry object is destroyed the End Entry will be logged.
  std::unique_ptr<PendingAsyncEntry> CreatePendingAsyncEntry(
      TaskId task_id,
      std::string_view event_name,
      std::vector<mojom::JournalDetailsPtr> details);

  void AddEndEvent(base::PassKey<Journal>,
                   TaskId task_id,
                   const std::string& event_name,
                   std::vector<mojom::JournalDetailsPtr> details);

  // Sends any buffered log entries to the browser process immediately.
  void SendLogBuffer();

 private:
  void AddJournalEntry(mojom::JournalEntryPtr journal_entry);

  mojo::AssociatedRemote<mojom::JournalClient> client_;
  std::vector<mojom::JournalEntryPtr> log_buffer_;
  base::TimeTicks last_log_buffer_send_;

  base::WeakPtrFactory<Journal> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_JOURNAL_H_
