// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

var ColorProviderCSSColorsTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://test/test_loader.html?module=color_provider_css_colors_test.js&host=webui-test';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

TEST_F('ColorProviderCSSColorsTest', 'All', function() {
  mocha.run();
});
