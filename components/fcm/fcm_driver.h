// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FCM_DRIVER_H_
#define COMPONENTS_FCM_FCM_DRIVER_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/fcm/fcm_app_handler.h"

namespace fcm {

// Base class managing app handlers and dispatching FCM messages/events.
class FcmDriver {
 public:
  FcmDriver();
  virtual ~FcmDriver();

  FcmDriver(const FcmDriver&) = delete;
  FcmDriver& operator=(const FcmDriver&) = delete;

  // Registers an app handler for the given `app_id`.
  void AddAppHandler(const std::string& app_id, FcmAppHandler* handler);

  // Unregisters the app handler for the given `app_id`.
  void RemoveAppHandler(const std::string& app_id, FcmAppHandler* handler);

 protected:
  // Dispatches an incoming FCM message to the appropriate app handler.
  void DispatchIncomingMessage(const FcmMessage& message);

  // Broadcasts that messages were deleted to all registered app handlers.
  void DispatchMessagesDeleted();

  // Broadcasts an updated installation ID to all registered app handlers.
  void DispatchInstallationIdRefreshed(
      const std::optional<InstallationId>& installation_id);

  // Cached installation ID, if available.
  std::optional<InstallationId> installation_id_;

 private:
  std::map<std::string, raw_ptr<FcmAppHandler>> app_handlers_;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_FCM_DRIVER_H_
