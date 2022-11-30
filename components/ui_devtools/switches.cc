// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/switches.h"

namespace ui_devtools {
namespace switches {

// Enables DevTools server for UI (mus, ash, etc). Value should be the port the
// server is started on. Default port is 9223.
const char kEnableUiDevTools[] = "enable-ui-devtools";

}  // namespace switches
}  // namespace ui_devtools
