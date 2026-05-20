// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/internals/browser/actor_internals_handler.h"

#include <utility>

namespace actor_internals {

ActorInternalsHandler::ActorInternalsHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    Delegate* delegate)
    : remote_(std::move(page)),
      receiver_(this, std::move(receiver)),
      delegate_(delegate) {
  CHECK(delegate_);
}

ActorInternalsHandler::~ActorInternalsHandler() = default;

void ActorInternalsHandler::OnJournalEntryAdded(mojom::JournalEntryPtr entry) {
  remote_->JournalEntryAdded(std::move(entry));
}

void ActorInternalsHandler::StartLogging() {
  delegate_->StartLogging();
}

void ActorInternalsHandler::StopLogging() {
  delegate_->StopLogging();
}

}  // namespace actor_internals
