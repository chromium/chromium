// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/conversation_observer.h"

namespace ash::assistant {

ConversationObserver::ConversationObserver() = default;
ConversationObserver::~ConversationObserver() = default;

mojo::PendingRemote<libassistant::mojom::ConversationObserver>
ConversationObserver::BindNewPipeAndPassRemote() {
  // LibassistantService could restart and the remote will be reconnected.
  if (remote_observer_.is_bound()) {
    remote_observer_.reset();
  }

  return remote_observer_.BindNewPipeAndPassRemote();
}

}  // namespace ash::assistant
