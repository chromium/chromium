// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/browser/ui/tabs/tab_strip_model.h"');
GEN('#include "content/public/browser/web_contents.h"');

function PolymerInteractiveUITest() {}

PolymerInteractiveUITest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  testGenPreamble: function() {
    // Must explicitly focus the web contents before running the test on Mac.
    // See: https://crbug.com/642467.
    GEN('  browser()->tab_strip_model()->GetActiveWebContents()->Focus();');
  },
};
