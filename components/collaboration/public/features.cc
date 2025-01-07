// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace collaboration::features {

BASE_FEATURE(kCollaborationMessaging,
             "CollaborationMessaging",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kCollaborationFlowAndroid,
             "CollaborationFlowAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace collaboration::features
