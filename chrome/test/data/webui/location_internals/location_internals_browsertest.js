// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://location-internals
 */

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */

var LocationInternalsTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://location-internals/';
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
};

TEST_F('LocationInternalsTest', 'All', async function() {
  const module = await import('chrome://resources/js/static_types.js');
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    // Run all registered tests.
    mocha.run();
  };
  script.src =
      module
          .getTrustedScriptURL`chrome://webui-test/location_internals/location_internals_test.js`;
  document.body.appendChild(script);
});
