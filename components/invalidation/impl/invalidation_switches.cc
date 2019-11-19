// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_switches.h"

namespace invalidation {
namespace switches {

#if defined(OS_CHROMEOS)
// Device invalidation service should use GCM network channel.
const char kInvalidationUseGCMChannel[] = "invalidation-use-gcm-channel";
#endif  // OS_CHROMEOS

// Overrides the default host:port used for notifications.
const char kSyncNotificationHostPort[] = "sync-notification-host-port";

// Allows insecure XMPP connections for sync (for testing).
const char kSyncAllowInsecureXmppConnection[] =
    "sync-allow-insecure-xmpp-connection";

const base::Feature kFCMInvalidationsConservativeEnabling = {
    "FCMInvalidationsConservativeEnabling", base::FEATURE_ENABLED_BY_DEFAULT};

// This feature affects only Android.
const base::Feature kFCMInvalidationsStartOnceActiveAccountAvailable = {
    "FCMInvalidationsStartOnceActiveAccountAvailable",
    base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kFCMInvalidationsForSyncDontCheckVersion;
const base::Feature kFCMInvalidationsForSyncDontCheckVersion = {
    "FCMInvalidationsForSyncDontCheckVersion",
    base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(melandory): Once FCM invalidations are launched, this feature toggle
// should be removed.
// TODO(crbug.com/964296): Re-enable when bug is resolved.
const base::Feature kTiclInvalidationsStartInvalidatorOnActiveHandler = {
    "TiclInvalidationsStartInvalidatorOnActiveHandler",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
}  // namespace invalidation
