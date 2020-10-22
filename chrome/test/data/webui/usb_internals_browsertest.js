// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://usb-internals
 */

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */
function UsbInternalsTest() {}

UsbInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://usb-internals/test_loader.html?module=usb_internals_test.js',

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('UsbInternalsTest', 'WebUIValueRenderTest', function() {
  // Run all registered tests.
  mocha.run();
});
