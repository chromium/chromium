// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

NearbyPresenceService::NearbyPresenceService() = default;
NearbyPresenceService::~NearbyPresenceService() = default;

NearbyPresenceService::ScanFilter::ScanFilter(
    ::nearby::internal::IdentityType identity_type,
    const std::vector<Action>& actions)
    : identity_type_(identity_type), actions_(actions) {}

NearbyPresenceService::ScanFilter::~ScanFilter() = default;

NearbyPresenceService::ScanFilter::ScanFilter(const ScanFilter& scan_filter) {
  identity_type_ = scan_filter.identity_type_;
  actions_ = scan_filter.actions_;
}

NearbyPresenceService::ScanDelegate::ScanDelegate() = default;
NearbyPresenceService::ScanDelegate::~ScanDelegate() = default;

NearbyPresenceService::ScanSession::ScanSession(
    mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession>
        pending_remote,
    base::OnceClosure on_disconnect_callback)
    : remote_(std::move(pending_remote)),
      on_disconnect_callback_(std::move(on_disconnect_callback)) {}

NearbyPresenceService::ScanSession::~ScanSession() {
  std::move(on_disconnect_callback_).Run();
}

std::ostream& operator<<(std::ostream& stream,
                         const enums::StatusCode status_code) {
  switch (status_code) {
    case enums::StatusCode::kAbslOk:
      return stream << "OK";
    case enums::StatusCode::kAbslCancelled:
      return stream << "Cancelled";
    case enums::StatusCode::kAbslUnknown:
      return stream << "Unknown";
    case enums::StatusCode::kAbslInvalidArgument:
      return stream << "Invalid Argument";
    case enums::StatusCode::kAbslDeadlineExceeded:
      return stream << "Deadline Exceeded";
    case enums::StatusCode::kAbslNotFound:
      return stream << "Not Found";
    case enums::StatusCode::kAbslAlreadyExists:
      return stream << "Already Exists";
    case enums::StatusCode::kAbslPermissionDenied:
      return stream << "Permission Denied";
    case enums::StatusCode::kAbslResourceExhausted:
      return stream << "Resource Exhausted";
    case enums::StatusCode::kAbslFailedPrecondition:
      return stream << "Failed Precondition";
    case enums::StatusCode::kAbslAborted:
      return stream << "Aborted";
    case enums::StatusCode::kAbslOutOfRange:
      return stream << "Out of Range";
    case enums::StatusCode::kAbslUnimplemented:
      return stream << "Unimplemented";
    case enums::StatusCode::kAbslInternal:
      return stream << "Internal";
    case enums::StatusCode::kAbslUnavailable:
      return stream << "Unavailable";
    case enums::StatusCode::kAbslDataLoss:
      return stream << "Data Loss";
    case enums::StatusCode::kAbslUnauthenticated:
      return stream << "Unauthenticated";
    case enums::StatusCode::kFailedToStartProcess:
      return stream << "Failed to Start Process";
  }
}

}  // namespace ash::nearby::presence
