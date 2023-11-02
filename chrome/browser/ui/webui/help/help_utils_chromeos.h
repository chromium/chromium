// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_HELP_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_HELP_UTILS_CHROMEOS_H_

namespace help_utils_chromeos {

// Returns true if updates over cellular networks are allowed. If |interactive|
// is true (e.g. the user clicked on a 'check for updates' button), updates over
// cellular are allowed unless prohibited by policy. If |interactive| is false,
// updates over cellular may be allowed by default for the device type, or by
// policy.
bool IsUpdateOverCellularAllowed(bool interactive);

}  // namespace help_utils_chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_HELP_UTILS_CHROMEOS_H_
