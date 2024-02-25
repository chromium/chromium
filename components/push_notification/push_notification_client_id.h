// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_ID_H_
#define COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_ID_H_

#include <optional>
#include <string>

namespace push_notification {

enum class ClientId {
  kNearbyPresence,
};

static constexpr char kNearbyPresenceClientId[] = "nearby_presence";

std::optional<std::string> GetClientIdStr(ClientId id);
std::optional<ClientId> GetClientIdFromStr(const std::string& id_string);

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_ID_H_
