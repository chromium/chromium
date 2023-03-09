// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace send_tab_to_self {

BASE_FEATURE(kSendTabToSelfSigninPromo,
             "SendTabToSelfSigninPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfEnableNotificationTimeOut,
             "SendTabToSelfEnableNotificationTimeOut",
             base::FEATURE_DISABLED_BY_DEFAULT);


#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kSendTabToSelfV2,
             "SendTabToSelfV2",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self
