// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */
function LoadTimeDataTest() {}

LoadTimeDataTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://extensions/',

  /** @override */
  extraLibraries: [
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/parse_html_subset.js',
    '//ui/webui/resources/js/load_time_data.js',
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    'js/load_time_data_test.js',
  ],
};

TEST_F('LoadTimeDataTest', 'All', function() {
  mocha.run();
});
