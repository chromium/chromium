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

}  // namespace switches
