// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('NetInternalsMainTests', function() {
  test('tab visibility state', function() {
    // Expected visibility state of each tab.
    const tabVisibilityState = {
      events: true,
      proxy: true,
      dns: true,
      sockets: true,
      hsts: true,
      chromeos: cr.isChromeOS
    };

    net_internals_test.checkTabLinkVisibility(tabVisibilityState, true);
  });
});
