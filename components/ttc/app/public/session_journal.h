// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_
#define COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_

#include <stdint.h>

#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
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

  // Records an encoded audio clip (e.g. a WAV file). This is stored in the
  // journal's screenshot slot since that's the only binary blob it supports.
  virtual void LogAudio(std::string_view mime_type,
                        base::span<const uint8_t> data) = 0;
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_SESSION_JOURNAL_H_
