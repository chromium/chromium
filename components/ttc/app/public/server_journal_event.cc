// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/public/server_journal_event.h"

namespace ttc {

ServerJournalEvent::ServerJournalEvent() = default;
ServerJournalEvent::ServerJournalEvent(const ServerJournalEvent&) = default;
ServerJournalEvent& ServerJournalEvent::operator=(const ServerJournalEvent&) =
    default;
ServerJournalEvent::ServerJournalEvent(ServerJournalEvent&&) = default;
ServerJournalEvent& ServerJournalEvent::operator=(ServerJournalEvent&&) =
    default;
ServerJournalEvent::~ServerJournalEvent() = default;

}  // namespace ttc
