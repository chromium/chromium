// Copyright 2019 The Chromium Authors
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
  browsePreload: 'chrome://usb-internals/',

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('UsbInternalsTest', 'WebUIValueRenderTest', function() {
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    // Run all registered tests.
    mocha.run();
  };
  const staticUrlPolicy = trustedTypes.createPolicy(
      'usb-test-static',
      {createScriptURL: () => 'chrome://webui-test/usb_internals_test.js'});
  script.src = staticUrlPolicy.createScriptURL('');
  document.body.appendChild(script);
});
