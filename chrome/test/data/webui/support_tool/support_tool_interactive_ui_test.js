// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer Support Tool element. */
const SupportToolInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://support-tool';
  }

  /** @override */
  get webuiHost() {
    return 'support-tool';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSupportTool',
        'features::kSupportToolScreenshot',
        'features::kSupportToolCopyTokenButton',
      ],
    };
  }
};

// js2gtest.js requires using var (see crbug/1033337 for details).
// eslint-disable-next-line no-var
var SupportToolTest = class extends SupportToolInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://support-tool/test_loader.html?' +
        'module=support_tool/support_tool_test.js';
  }
};

TEST_F('SupportToolTest', 'All', function() {
  mocha.run();
});
