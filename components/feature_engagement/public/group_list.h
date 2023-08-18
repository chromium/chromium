// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_LIST_H_

#include "base/feature_list.h"

namespace feature_engagement {
using GroupVector = std::vector<const base::Feature*>;

// Returns all the features that are in use for engagement tracking.
GroupVector GetAllGroups();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_GROUP_LIST_H_
