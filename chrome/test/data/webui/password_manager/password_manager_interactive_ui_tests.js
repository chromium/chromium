// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "components/password_manager/core/common/password_manager_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const PasswordManagerUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://password-manager/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['password_manager::features::kPasswordManagerRedesign']};
  }
};

// eslint-disable-next-line no-var
var PasswordManagerUIFocusTest = class extends PasswordManagerUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://password-manager/test_loader.html?module=password_manager/password_manager_focus_test.js';
  }
};

// https://crbug.com/1444623: Flaky on Mac.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

TEST_F('PasswordManagerUIFocusTest', 'MAYBE_All', function() {
  mocha.run();
});
