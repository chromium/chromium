// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/features.h"

#include "base/feature_list.h"

namespace collaboration::features {

BASE_FEATURE(kCollaborationMessaging,
             "CollaborationMessaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCollaborationFlowAndroid,
             "CollaborationFlowAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace collaboration::features
