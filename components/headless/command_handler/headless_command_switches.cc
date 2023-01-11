// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/command_handler/headless_command_switches.h"

namespace headless::switches {

// The background color to be used if the page doesn't specify one. Provided as
// RGB or RGBA integer value in hex, e.g. 'ff0000ff' for red or '00000000' for
// transparent.
const char kDefaultBackgroundColor[] = "default-background-color";

// Instructs headless_shell to print document.body.innerHTML to stdout.
const char kDumpDom[] = "dump-dom";

// Save a pdf file of the loaded page.
const char kPrintToPDF[] = "print-to-pdf";

// Do not display header and footer in the pdf file.
const char kPrintToPDFNoHeader[] = "print-to-pdf-no-header";

// Save a screenshot of the loaded page.
const char kScreenshot[] = "screenshot";

// Issues a stop after the specified number of milliseconds.  This cancels all
// navigation and causes the DOMContentLoaded event to fire.
const char kTimeout[] = "timeout";

// If set the system waits the specified number of virtual milliseconds before
// deeming the page to be ready.  For determinism virtual time does not advance
// while there are pending network fetches (i.e no timers will fire). Once all
// network fetches have completed, timers fire and if the system runs out of
// virtual time is fastforwarded so the next timer fires immediately, until the
// specified virtual time budget is exhausted.
const char kVirtualTimeBudget[] = "virtual-time-budget";

}  // namespace headless::switches
