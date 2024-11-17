// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_TEST_UTILS_H_

#include "extensions/common/extension_id.h"

class Browser;

namespace extensions {

// Opens the side panel for `browser` for the given extension's `id`.
// Implemented by extension_side_panel_test_utils.cc in views/.
void OpenExtensionSidePanel(Browser& browser, const ExtensionId& id);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_TEST_UTILS_H_
