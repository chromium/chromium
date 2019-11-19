// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_gcm_app_handler.h"

#include <utility>

#include "components/offline_pages/core/offline_event_logger.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {
const char kPrefetchingOfflinePagesAppId[] =
    "com.google.chrome.OfflinePagePrefetch";

PrefetchGCMAppHandler::PrefetchGCMAppHandler() {}
PrefetchGCMAppHandler::~PrefetchGCMAppHandler() = default;

void PrefetchGCMAppHandler::SetService(PrefetchService* service) {
  prefetch_service_ = service;
}

void PrefetchGCMAppHandler::ShutdownHandler() {}

void PrefetchGCMAppHandler::OnStoreReset() {}

void PrefetchGCMAppHandler::OnMessage(const std::string& app_id,
                                      const gcm::IncomingMessage& message) {
  std::string pageBundle;
  auto iter = message.data.find("pageBundle");
  if (iter != message.data.end()) {
    pageBundle = iter->second;
  } else {
    prefetch_service_->GetLogger()->RecordActivity(
        "GCM Message without page bundle received!");
    return;
  }

  prefetch_service_->GetPrefetchDispatcher()
      ->GCMOperationCompletedMessageReceived(pageBundle);
  prefetch_service_->GetLogger()->RecordActivity(
      "Received GCM message. App ID: " + app_id +
      "; pageBundle data: " + pageBundle);
}

void PrefetchGCMAppHandler::OnMessagesDeleted(const std::string& app_id) {}

void PrefetchGCMAppHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {}

void PrefetchGCMAppHandler::OnSendAcknowledged(const std::string& app_id,
                                               const std::string& message_id) {}

bool PrefetchGCMAppHandler::CanHandle(const std::string& app_id) const {
  return app_id == GetAppId();
}

gcm::GCMAppHandler* PrefetchGCMAppHandler::AsGCMAppHandler() {
  return (gcm::GCMAppHandler*)this;
}

std::string PrefetchGCMAppHandler::GetAppId() const {
  return kPrefetchingOfflinePagesAppId;
}

}  // namespace offline_pages
