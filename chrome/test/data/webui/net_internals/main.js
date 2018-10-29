// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * Checks visibility of tab handles against expectations, and navigates to all
 * tabs with visible handles, validating visibility of all other tabs as it
 * goes.
 */
TEST_F('NetInternalsTest', 'netInternalsTourTabs', function() {
  // Expected visibility state of each tab.
  var tabVisibilityState = {
    events: true,
    proxy: true,
    dns: true,
    sockets: true,
    hsts: true,
    chromeos: cr.isChromeOS
  };

  NetInternalsTest.checkTabLinkVisibility(tabVisibilityState, true);

  testDone();
});

})();  // Anonymous namespace
