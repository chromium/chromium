// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_

#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

class PrefService;

namespace privacy_sandbox {

// Attempts to set prefs in order to roll back Mode B.
void MaybeSetRollbackPrefsModeB(PrefService* prefs);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SETTINGS_H_
