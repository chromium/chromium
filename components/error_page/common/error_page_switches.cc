// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/error_page_switches.h"

namespace error_page {
namespace switches {

// Disables the dinosaur easter egg on the offline interstitial.
const char kDisableDinosaurEasterEgg[] = "disable-dinosaur-easter-egg";

// Values for the kShowSavedCopy flag.
const char kDisableShowSavedCopy[] = "disable";
const char kEnableShowSavedCopyPrimary[] = "primary";
const char kEnableShowSavedCopySecondary[] = "secondary";

// Command line flag offering a "Show saved copy" option to the user if offline.
// The various modes are disabled, primary, or secondary. Primary/secondary
// refers to button placement (for experiment).
const char kShowSavedCopy[] = "show-saved-copy";

}  // namespace switches
}  // namespace error_page
