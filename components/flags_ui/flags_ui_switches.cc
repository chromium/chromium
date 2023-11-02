// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_ui_switches.h"

namespace switches {

// These two flags are added around the switches about:flags adds to the
// command line. This is useful to see which switches were added by about:flags
// on about:version. They don't have any effect.
const char kFlagSwitchesBegin[] = "flag-switches-begin";
const char kFlagSwitchesEnd[] = "flag-switches-end";

}  // namespace switches
