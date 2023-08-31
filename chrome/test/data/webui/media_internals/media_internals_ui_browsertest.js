// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Histograms WebUI.
 */

GEN('#include "base/metrics/histogram.h"');
GEN('#include "content/public/test/browser_test.h"');

function MediaInternalsUIBrowserTest() {}

MediaInternalsUIBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://media-internals',

  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('MediaInternalsUIBrowserTest', 'Integration', async function() {
  const module = await import('chrome://resources/js/static_types.js');
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    // Run all registered tests.
    mocha.run();
  };
  script.src =
      module
          .getTrustedScriptURL`chrome://webui-test/media_internals/integration_test.js`;
  document.body.appendChild(script);
});

TEST_F('MediaInternalsUIBrowserTest', 'Manager', async function() {
  const module = await import('chrome://resources/js/static_types.js');
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    // Run all registered tests.
    mocha.run();
  };
  script.src =
      module
          .getTrustedScriptURL`chrome://webui-test/media_internals/manager_test.js`;
  document.body.appendChild(script);
});

TEST_F('MediaInternalsUIBrowserTest', 'PlayerInfo', async function() {
  const module = await import('chrome://resources/js/static_types.js');
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    // Run all registered tests.
    mocha.run();
  };
  script.src =
      module
          .getTrustedScriptURL`chrome://webui-test/media_internals/player_info_test.js`;
  document.body.appendChild(script);
});
