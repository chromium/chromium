// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_
#define COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_

#include <string_view>
#include <utility>
#include <vector>

#include "components/actor/public/mojom/actor_types.mojom.h"

namespace ttc {

struct ServerJournalEvent;

// Interface for recording TTC session events in the actor journal.
class SessionJournal {
 public:
  enum class Track {
    kFrontEnd,
    kMesClientRecv,
    kMesClientSend,
  };

  virtual ~SessionJournal() = default;

  // Logs an instant event on `track`.
  virtual void Log(std::string_view event_name,
                   std::vector<actor::mojom::JournalDetailsPtr> details,
                   Track track) = 0;
  void Log(std::string_view event_name,
           std::vector<actor::mojom::JournalDetailsPtr> details) {
    Log(event_name, std::move(details), Track::kFrontEnd);
  }

  // Records a journal event received from the server.
  virtual void HandleServerJournalEvent(const ServerJournalEvent& event) = 0;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_
