// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI read later. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "components/reading_list/features/reading_list_switches.h"');
GEN('#include "content/public/test/browser_test.h"');

class ReadLaterBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }

  /** @override */
  get featureList() {
    return {enabled: ['reading_list::switches::kReadLater']};
  }
}

// eslint-disable-next-line no-var
var ReadLaterAppTest = class extends ReadLaterBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://read-later.top-chrome/test_loader.html?module=read_later/read_later_app_test.js';
  }
};

TEST_F('ReadLaterAppTest', 'All', function() {
  mocha.run();
});
