// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

// eslint-disable-next-line no-var
var TabSearchInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://tab-search.top-chrome/test_loader.html?module=tab_search/tab_search_app_focus_test.js';
  }
};

GEN('#if defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('TabSearchInteractiveUITest', 'MAYBE_All', function() {
  mocha.run();
});
