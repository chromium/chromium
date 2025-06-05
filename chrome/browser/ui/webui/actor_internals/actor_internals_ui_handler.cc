// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui_handler.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace {
std::string ToString(actor::mojom::JournalEntryType type) {
  switch (type) {
    case actor::mojom::JournalEntryType::kBegin:
      return "B";
    case actor::mojom::JournalEntryType::kEnd:
      return "E";
    case actor::mojom::JournalEntryType::kInstant:
      return "I";
  }
  NOTREACHED();
}
}  // namespace

ActorInternalsUIHandler::ActorInternalsUIHandler(
    Profile* profile,
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
    : profile_(profile),
      remote_(std::move(page)),
      receiver_(this, std::move(receiver)) {
  auto& journal = actor::ActorKeyedService::Get(profile_)->GetJournal();
  journal.AddObserver(this);

  for (auto it = journal.Items(); it; ++it) {
    WillAddJournalEntry(***it);
  }
}

ActorInternalsUIHandler::~ActorInternalsUIHandler() {
  auto& journal = actor::ActorKeyedService::Get(profile_)->GetJournal();
  journal.RemoveObserver(this);
}

void ActorInternalsUIHandler::WillAddJournalEntry(
    const actor::AggregatedJournal::Entry& entry) {
  remote_->JournalEntryAdded(actor_internals::mojom::JournalEntry::New(
      entry.url, entry.data->event, ToString(entry.data->type),
      entry.data->details, entry.data->timestamp));
}
