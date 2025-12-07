// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/collaboration_utils.h"

namespace collaboration {

data_sharing::MemberRole GetCurrentUserRoleForGroup(
    signin::IdentityManager* identity_manager,
    const data_sharing::GroupData& group_data) {
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (account.IsEmpty()) {
    // No current logged in user.
    return data_sharing::MemberRole::kUnknown;
  }

  for (const data_sharing::GroupMember& member : group_data.members) {
    if (member.gaia_id == account.gaia) {
      return member.role;
    }
  }

  // Current user is not found in group.
  return data_sharing::MemberRole::kUnknown;
}

}  // namespace collaboration
