// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design downloads page. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

// NOTE: Do not add any more tests here, instead add them in
// downloads_browsertest.cc. This file will be deleted once the TODO below is
// addressed.

// TODO(crbug.com/1457360): Figure out how to migrate this test, which does not
// use test_loader.html, to WebUIMochaBrowserTest.
var DownloadsUrlTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads/a/b/';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('DownloadsUrlTest', 'All', async function() {
  await import('chrome://webui-test/mojo_webui_test_support.js');
  suite('loading a nonexistent URL of /a/b/', function() {
    test('should load main page with no console errors', function() {
      return customElements.whenDefined('downloads-manager').then(() => {
        assertEquals('chrome://downloads/', location.href);
      });
    });
  });
  mocha.run();
});
