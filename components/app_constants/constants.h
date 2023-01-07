// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_CONSTANTS_CONSTANTS_H_
#define COMPONENTS_APP_CONSTANTS_CONSTANTS_H_

#include "base/component_export.h"

namespace app_constants {

// App IDs are a unique internal identifier for apps on Chrome OS. For
// historical reasons, they are a SHA hash of some uniquely identifying
// constant associated with an app, transposed from the [0-9a-f] range to [a-p]
// (the same format used by Chrome's extension IDs). The following are app IDs
// for the Chrome browser application on Chrome OS.

// The ID of the Chrome component application as part of ash.
COMPONENT_EXPORT(APP_CONSTANTS) extern const char kChromeAppId[];

// The ID of the Lacros Chrome browser application that runs outside of ash.
COMPONENT_EXPORT(APP_CONSTANTS) extern const char kLacrosAppId[];

}  // namespace app_constants

#endif  // COMPONENTS_APP_CONSTANTS_CONSTANTS_H_
