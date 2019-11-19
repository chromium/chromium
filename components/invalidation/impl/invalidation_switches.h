// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H

#include "base/feature_list.h"
#include "build/build_config.h"

namespace invalidation {
namespace switches {

#if defined(OS_CHROMEOS)
extern const char kInvalidationUseGCMChannel[];
#endif  // OS_CHROMEOS

extern const char kSyncNotificationHostPort[];
extern const char kSyncAllowInsecureXmppConnection[];
extern const base::Feature kFCMInvalidationsConservativeEnabling;
extern const base::Feature kFCMInvalidationsStartOnceActiveAccountAvailable;
extern const base::Feature kFCMInvalidationsForSyncDontCheckVersion;
extern const base::Feature kTiclInvalidationsStartInvalidatorOnActiveHandler;

}  // namespace switches
}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_SWITCHES_H
