// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_switches.h"

namespace switches {

// Comma-separated options to troubleshoot the component updater. Only valid
// for the browser process.
const char kComponentUpdater[] = "component-updater";

// Optional testing override of the Trust Tokens key commitment component's
// path.
const char kComponentUpdaterTrustTokensComponentPath[] =
    "component-updater-trust-tokens-component-path";

// Switch to control which serving campaigns file versions to select in test
// cohort. Example: `--campaigns-test-tag=dev1` will select test cohort which
// tag matches dev1.
const char kCampaignsTestTag[] = "campaigns-test-tag";

// Switch to control which serving demo mode app versions to select in test
// cohort. Example: `--demo-app-test-tag=dev1` will select test cohort which tag
// matches dev1.
const char kDemoModeTestTag[] = "demo-app-test-tag";

}  // namespace switches
