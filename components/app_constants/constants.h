// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_CONSTANTS_CONSTANTS_H_
#define COMPONENTS_APP_CONSTANTS_CONSTANTS_H_

#include "base/component_export.h"

namespace app_constants {

// The extension id of the Chrome component application.
COMPONENT_EXPORT(APP_CONSTANTS) extern const char kChromeAppId[];

// Fake extension ID for the Lacros chrome browser application.
COMPONENT_EXPORT(APP_CONSTANTS) extern const char kLacrosAppId[];

}  // namespace app_constants

#endif  // COMPONENTS_APP_CONSTANTS_CONSTANTS_H_
