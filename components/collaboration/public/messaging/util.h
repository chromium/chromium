// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_UTIL_H_
#define COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_UTIL_H_

#include <optional>
#include <vector>

#include "components/collaboration/public/messaging/message.h"

namespace collaboration::messaging {

// Summarizes the lost access to previously shared tab groups, among the list of
// current persistent messages.
//
// -  If there are no removed collaboration, it returns nil.
// -  If there are one or two removed collaborations, it returns a summary
//    mentioning the last known titles of the groups, if all titles are set.
//    Otherwise, it returns a summary mentioning the total count of removed
//    collaborations.
// -  If there are more than two removed collaborations, it returns a summary
//    mentioning the total count of removed collaborations.
std::optional<std::string> GetRemovedCollaborationsSummary(
    std::vector<PersistentMessage> messages);

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_UTIL_H_
