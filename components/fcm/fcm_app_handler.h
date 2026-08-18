// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FCM_APP_HANDLER_H_
#define COMPONENTS_FCM_FCM_APP_HANDLER_H_

#include <optional>
#include <string>

#include "base/types/strong_alias.h"
#include "components/fcm/fcm_message.h"

namespace fcm {

using InstallationId = base::StrongAlias<class InstallationIdTag, std::string>;

// Interface for applications/features to handle FCM messages and events.
class FcmAppHandler {
 public:
  virtual ~FcmAppHandler() = default;

  // Called when a message is received.
  virtual void OnMessage(const FcmMessage& message) = 0;

  // Called when messages have been deleted on the server.
  virtual void OnMessagesDeleted() = 0;

  // Called when the Firebase Installation ID (FID) has been refreshed or
  // cleared. It will also be called immediately once the app handler is
  // registered to provide the initial value.
  virtual void OnInstallationIdRefreshed(
      const std::optional<InstallationId>& installation_id) = 0;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_FCM_APP_HANDLER_H_
