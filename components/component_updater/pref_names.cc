// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/pref_names.h"

#include "build/build_config.h"

namespace prefs {

// Policy that indicates the state of updates for the binary components.
const char kComponentUpdatesEnabled[] =
    "component_updates.component_updates_enabled";

// String that represents the recovery component last downloaded version. This
// takes the usual 'a.b.c.d' notation.
const char kRecoveryComponentVersion[] = "recovery_component.version";

// Full path where last recovery component CRX was unpacked to.
const char kRecoveryComponentUnpackPath[] = "recovery_component.unpack_path";

}  // namespace prefs
