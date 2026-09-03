// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_driver.h"

#include "base/check.h"

namespace fcm {
namespace {

// Extracts app_id from the "subtype" field in message data.
std::string ExtractAppIdFromMessage(const FcmMessage& message) {
  auto it = message.data.find("subtype");
  return it != message.data.end() ? it->second : std::string();
}

}  // namespace

FcmDriver::FcmDriver() = default;

FcmDriver::~FcmDriver() = default;

void FcmDriver::AddAppHandler(const std::string& app_id,
                              FcmAppHandler* handler) {
  CHECK(handler);
  CHECK(app_handlers_.find(app_id) == app_handlers_.end());
  app_handlers_[app_id] = handler;

  if (installation_id_) {
    handler->OnInstallationIdRefreshed(installation_id_);
  }
}

void FcmDriver::RemoveAppHandler(const std::string& app_id,
                                 FcmAppHandler* handler) {
  CHECK(handler);
  auto it = app_handlers_.find(app_id);
  CHECK(it != app_handlers_.end() && it->second == handler);
  app_handlers_.erase(it);
}

void FcmDriver::DispatchIncomingMessage(const FcmMessage& message) {
  std::string app_id = ExtractAppIdFromMessage(message);

  auto handler_it = app_handlers_.find(app_id);
  if (handler_it != app_handlers_.end()) {
    handler_it->second->OnMessage(message);
  } else {
    // TODO(b/546500297): Gather messages in memory or persist them for apps
    // which are not registered yet.
  }
}

void FcmDriver::DispatchMessagesDeleted() {
  // FCM server queue overflow/deletion notifications do not specify which app
  // exactly lost the message, so we broadcast to all registered app handlers.
  // TODO(b/546500297): Gather messages in memory or persist them for apps
  // which are not registered yet.
  for (auto& [app_id, handler] : app_handlers_) {
    handler->OnMessagesDeleted();
  }
}

void FcmDriver::DispatchInstallationIdRefreshed(
    const std::optional<InstallationId>& installation_id) {
  installation_id_ = installation_id;
  for (auto& [app_id, handler] : app_handlers_) {
    handler->OnInstallationIdRefreshed(installation_id);
  }
}

}  // namespace fcm
