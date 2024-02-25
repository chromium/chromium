// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {checkTabLinkVisibility} from './test_util.js';

suite('NetInternalsMainTest', function() {
  test('tab visibility state', function() {
    // Expected visibility state of each tab.
    const tabVisibilityState = {
      events: true,
      proxy: true,
      dns: true,
      sockets: true,
      hsts: true,
      sharedDictionary: true,
      // <if expr="chromeos_ash">
      chromeos: true,
      // </if>
    };

    checkTabLinkVisibility(tabVisibilityState, true);
  });
});
