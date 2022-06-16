// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/public/cpp/conversation_observer.h"

namespace chromeos {
namespace assistant {

ConversationObserver::ConversationObserver() = default;
ConversationObserver::~ConversationObserver() = default;

mojo::PendingRemote<chromeos::libassistant::mojom::ConversationObserver>
ConversationObserver::BindNewPipeAndPassRemote() {
  DCHECK(!remote_observer_.is_bound());

  return remote_observer_.BindNewPipeAndPassRemote();
}

}  // namespace assistant
}  // namespace chromeos
