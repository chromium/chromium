// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace send_tab_to_self {

// If this feature is enabled and a signed-out user attempts to share a tab,
// they will see a promo to sign-in.
BASE_DECLARE_FEATURE(kSendTabToSelfSigninPromo);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// If this feature is enabled, show received tabs in a new UI next to the
// profile icon rather than in a system notification.
//
// V2 is the default on desktop and the V1 code path has been deleted there, so
// this base::Feature no longer exists on desktop platforms.
BASE_DECLARE_FEATURE(kSendTabToSelfV2);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
