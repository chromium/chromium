// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_COLLABORATION_ID_H_
#define COMPONENTS_SYNC_BASE_COLLABORATION_ID_H_

#include <string>

#include "base/types/strong_alias.h"

namespace syncer {

// A unique identifier for a collaboration, used for shared data types.
using CollaborationId =
    base::StrongAlias<class CollaborationIdTag, std::string>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_COLLABORATION_ID_H_
