// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_JOURNAL_H_
#define CHROME_RENDERER_ACTOR_JOURNAL_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace actor {

// A logging class that records actions taken be the actor.
// This class employs a buffering strategy to minimize the number of
// IPCs sent to the browser. The following strategy is employed:
// 1) If there is a pending event in the buffer, don't do anything just
//    wait for the previous event logged to trigger (the 200ms timeout).
// 2) If it has been more than 100ms since we last sent data to the browser
//    send it immediately.
// 3) Otherwise schedule a timer to send the contents of the buffer in 200ms.
class Journal {
 public:
  using TaskId = int32_t;

  Journal();
  ~Journal();

  void Bind(mojo::PendingAssociatedRemote<mojom::JournalClient> client);
  void Log(TaskId task_id, std::string_view event, std::string_view details);

 private:
  void SendLogBuffer();

  mojo::AssociatedRemote<mojom::JournalClient> client_;
  std::vector<mojom::JournalEntryPtr> log_buffer_;
  base::TimeTicks last_log_buffer_send_;
  uint64_t current_id_;

  base::WeakPtrFactory<Journal> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_JOURNAL_H_
