// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/human_presence/human_presence_internals.h"

namespace hps {

// Resource paths.
const char kHumanPresenceInternalsCSS[] = "human_presence_internals.css";
const char kHumanPresenceInternalsJS[] = "human_presence_internals.js";
const char kHumanPresenceInternalsIcon[] = "human_presence_internals_icon.svg";

// Message handlers.
const char kHumanPresenceInternalsConnectCmd[] = "connect";
const char kHumanPresenceInternalsEnableLockOnLeaveCmd[] = "enable_sense";
const char kHumanPresenceInternalsDisableLockOnLeaveCmd[] = "disable_sense";
const char kHumanPresenceInternalsQueryLockOnLeaveCmd[] = "query_sense";
const char kHumanPresenceInternalsEnableSnoopingProtectionCmd[] =
    "enable_notify";
const char kHumanPresenceInternalsDisableSnoopingProtectionCmd[] =
    "disable_notify";
const char kHumanPresenceInternalsQuerySnoopingProtectionCmd[] = "query_notify";

// Events.
const char kHumanPresenceInternalsConnectedEvent[] = "connected";
const char kHumanPresenceInternalsLockOnLeaveChangedEvent[] = "sense_changed";
const char kHumanPresenceInternalsSnoopingProtectionChangedEvent[] =
    "notify_changed";
const char kHumanPresenceInternalsEnableErrorEvent[] = "enable_error";
const char kHumanPresenceInternalsManifestEvent[] = "manifest";

}  // namespace hps
