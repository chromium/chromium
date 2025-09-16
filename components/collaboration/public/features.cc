// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace collaboration::features {

BASE_FEATURE(kCollaborationMessaging, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCollaborationMessagingDatabase, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCollaborationComments, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace collaboration::features
