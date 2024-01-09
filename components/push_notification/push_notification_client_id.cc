// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/push_notification_client_id.h"

namespace push_notification {

std::optional<std::string> GetClientIdStr(ClientId id) {
  switch (id) {
    case ClientId::kNearbyPresence:
      return kNearbyPresenceClientId;
    default:
      return std::nullopt;
  }
}

std::optional<ClientId> GetClientIdFromStr(const std::string& id_string) {
  if (id_string == kNearbyPresenceClientId) {
    return ClientId::kNearbyPresence;
  }
  return std::nullopt;
}

}  // namespace push_notification
