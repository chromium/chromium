// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "services/network/public/cpp/features.h"');

/**
 * TestFixture for Discards WebUI testing.
 */
var DiscardsTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://test?module=discards/discards_test.js';
  }

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

  /** @override */
  get webuiHost() {
    return 'discards';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};


TEST_F('DiscardsTest', 'All', function() {
  mocha.run();
});
