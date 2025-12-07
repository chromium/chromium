// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_UTILS_H_
#define COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_UTILS_H_

#include "components/data_sharing/public/group_data.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace collaboration {

// Get the group member information of the current user given the GroupData.
data_sharing::MemberRole GetCurrentUserRoleForGroup(
    signin::IdentityManager* identity_manager,
    const data_sharing::GroupData& group_data);

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_COLLABORATION_UTILS_H_
