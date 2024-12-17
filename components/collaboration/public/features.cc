// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/features.h"

#include "base/feature_list.h"

namespace collaboration::messaging {

BASE_FEATURE(kCollaborationMessaging,
             "CollaborationMessaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace collaboration::messaging
