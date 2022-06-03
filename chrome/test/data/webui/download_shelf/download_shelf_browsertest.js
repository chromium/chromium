// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI tab search. */

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

class DownloadShelfBrowserTest extends testing.Test {
  get isAsync() {
    return true;
  }

  get webuiHost() {
    return 'download-shelf.top-chrome';
  }

  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kWebUIDownloadShelf',
      ]
    };
  }
}

// eslint-disable-next-line no-var
var DownloadListTest = class extends DownloadShelfBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://download-shelf.top-chrome/test_loader.html?module=download_shelf/download_list_test.js';
  }
};

TEST_F('DownloadListTest', 'All', function() {
  mocha.run();
});
