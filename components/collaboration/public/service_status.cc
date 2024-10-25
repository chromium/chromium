// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/service_status.h"

namespace collaboration {

// LINT.IfChange(ServiceStatus)
bool ServiceStatus::IsAllowedToJoin() {
  // Please keep logic consistent with
  // //components/collaboration/public/android/java/src/org/chromium/components/collaboration/ServiceStatus.java.
  switch (collaboration_status) {
    case CollaborationStatus::kDisabled:
    case CollaborationStatus::kDisabledForPolicy:
      return false;
    case CollaborationStatus::kAllowedToJoin:
    case CollaborationStatus::kEnabledJoinOnly:
    case CollaborationStatus::kEnabledCreateAndJoin:
      return true;
  }
}

bool ServiceStatus::IsAllowedToCreate() {
  // Please keep logic consistent with
  // //components/collaboration/public/android/java/src/org/chromium/components/collaboration/ServiceStatus.java.
  switch (collaboration_status) {
    case CollaborationStatus::kDisabled:
    case CollaborationStatus::kDisabledForPolicy:
    case CollaborationStatus::kAllowedToJoin:
    case CollaborationStatus::kEnabledJoinOnly:
      return false;
    case CollaborationStatus::kEnabledCreateAndJoin:
      return true;
  }
}
// LINT.ThenChange(//components/collaboration/public/android/java/src/org/chromium/components/collaboration/ServiceStatus.java)

}  // namespace collaboration
