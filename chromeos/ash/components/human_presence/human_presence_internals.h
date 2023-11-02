// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_INTERNALS_H_
#define CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_INTERNALS_H_

namespace hps {

// Resource paths.
extern const char kHumanPresenceInternalsCSS[];
extern const char kHumanPresenceInternalsJS[];
extern const char kHumanPresenceInternalsIcon[];

// Message handlers.
extern const char kHumanPresenceInternalsConnectCmd[];
extern const char kHumanPresenceInternalsEnableLockOnLeaveCmd[];
extern const char kHumanPresenceInternalsDisableLockOnLeaveCmd[];
extern const char kHumanPresenceInternalsQueryLockOnLeaveCmd[];
extern const char kHumanPresenceInternalsEnableSnoopingProtectionCmd[];
extern const char kHumanPresenceInternalsDisableSnoopingProtectionCmd[];
extern const char kHumanPresenceInternalsQuerySnoopingProtectionCmd[];

// Events.
extern const char kHumanPresenceInternalsConnectedEvent[];
extern const char kHumanPresenceInternalsLockOnLeaveChangedEvent[];
extern const char kHumanPresenceInternalsSnoopingProtectionChangedEvent[];
extern const char kHumanPresenceInternalsEnableErrorEvent[];
extern const char kHumanPresenceInternalsManifestEvent[];

}  // namespace hps

#endif  // CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_INTERNALS_H_
