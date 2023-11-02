// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the Material Design downloads page. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var DownloadsTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads';
  }
};

var DownloadsItemTest = class extends DownloadsTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads/test_loader.html?module=downloads/item_tests.js';
  }
};

TEST_F('DownloadsItemTest', 'All', function() {
  mocha.run();
});

var DownloadsManagerTest = class extends DownloadsTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads/test_loader.html?module=downloads/manager_tests.js';
  }
};

TEST_F('DownloadsManagerTest', 'All', function() {
  mocha.run();
});

var DownloadsToolbarTest = class extends DownloadsTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads/test_loader.html?module=downloads/toolbar_tests.js';
  }
};

TEST_F('DownloadsToolbarTest', 'All', function() {
  mocha.run();
});

var DownloadsUrlTest = class extends DownloadsTest {
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

var DownloadsSearchServiceTest = class extends DownloadsTest {
  /** @override */
  get browsePreload() {
    return 'chrome://downloads/test_loader.html?module=downloads/search_service_test.js';
  }
};

TEST_F('DownloadsSearchServiceTest', 'All', function() {
  mocha.run();
});
