// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "services/network/public/cpp/features.h"');

// SetTimeDialogBrowserTest tests the "Set Time" web UI dialog.
// eslint-disable-next-line no-var
var SetTimeDialogBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://set-time/test_loader.html?module=set_time_dialog_test.js';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
};

TEST_F('SetTimeDialogBrowserTest', 'All', function() {
  mocha.run();
});
