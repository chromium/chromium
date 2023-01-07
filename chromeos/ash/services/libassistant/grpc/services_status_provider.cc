// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/services_status_provider.h"

#include "base/logging.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"

namespace ash::libassistant {

namespace {

bool ConvertServerStatus(::assistant::api::LibasServerStatus input,
                         ServicesStatus* output) {
  switch (input) {
    // We consider both states as booting up as a customer.
    case ::assistant::api::CUSTOMER_REGISTRATION_SERVICE_AVAILABLE:
    case ::assistant::api::ESSENTIAL_SERVICES_AVAILABLE:
      *output = ServicesStatus::ONLINE_BOOTING_UP;
      return true;
    case ::assistant::api::ALL_SERVICES_AVAILABLE:
      *output = ServicesStatus::ONLINE_ALL_SERVICES_AVAILABLE;
      return true;
    case ::assistant::api::UNKNOWN_LIBAS_SERVER_STATUS:
      return false;
  }
}

std::string GetServerStatusLogString(ServicesStatus status) {
  switch (status) {
    case ServicesStatus::OFFLINE:
      return "Libassistant service is OFFLINE.";
    case ServicesStatus::ONLINE_BOOTING_UP:
      return "Libassistant service is BOOTING UP.";
    case ServicesStatus::ONLINE_ALL_SERVICES_AVAILABLE:
      return "Libassistant service is ALL READY.";
  }
}

}  // namespace

ServicesStatusProvider::ServicesStatusProvider() = default;

ServicesStatusProvider::~ServicesStatusProvider() = default;

void ServicesStatusProvider::OnGrpcMessage(
    const ::assistant::api::OnHeartbeatEventRequest& request) {
  if (!request.has_current_server_status()) {
    LOG(ERROR) << "Heartbeat signal does not contain server status";
    return;
  }

  auto old_status = status_;
  if (!ConvertServerStatus(request.current_server_status(), &status_)) {
    LOG(ERROR) << "Received unknown Libassistant server status";
    return;
  }

  if (old_status != status_) {
    DVLOG(3) << GetServerStatusLogString(status_);

    // Notify observers on service status change.
    for (auto& observer : observers_)
      observer.OnServicesStatusChanged(status_);
  }
}

void ServicesStatusProvider::AddObserver(ServicesStatusObserver* observer) {
  observers_.AddObserver(observer);
}

void ServicesStatusProvider::RemoveObserver(ServicesStatusObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash::libassistant
