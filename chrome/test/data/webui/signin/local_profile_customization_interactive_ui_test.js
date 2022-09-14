// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');

var LocalProfileCustomizationFocusTest =
    class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/local_profile_customization_focus_test.js';
  }
};

TEST_F('LocalProfileCustomizationFocusTest', 'All', function() {
  mocha.run();
});
