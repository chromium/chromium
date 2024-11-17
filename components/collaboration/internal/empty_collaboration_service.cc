// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/empty_collaboration_service.h"

namespace collaboration {

EmptyCollaborationService::EmptyCollaborationService() = default;

EmptyCollaborationService::~EmptyCollaborationService() = default;

bool EmptyCollaborationService::IsEmptyService() {
  return true;
}

void EmptyCollaborationService::StartJoinFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    const GURL& url) {}

void EmptyCollaborationService::StartShareFlow(
    std::unique_ptr<CollaborationControllerDelegate> delegate,
    tab_groups::EitherGroupID group_id) {}

ServiceStatus EmptyCollaborationService::GetServiceStatus() {
  return ServiceStatus();
}

data_sharing::MemberRole EmptyCollaborationService::GetCurrentUserRoleForGroup(
    const data_sharing::GroupId& group_id) {
  return data_sharing::MemberRole::kUnknown;
}

}  // namespace collaboration
