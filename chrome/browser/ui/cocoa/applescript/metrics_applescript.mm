// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/metrics_applescript.h"

#include "base/metrics/histogram_macros.h"

namespace AppleScript {

void LogAppleScriptUMA(AppleScriptCommand sample) {
  UMA_HISTOGRAM_ENUMERATION("AppleScript.CommandEvent", sample,
                            APPLESCRIPT_COMMAND_EVENTS_COUNT);
}

}  // namespace
