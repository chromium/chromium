// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // The user scrolled past a scroll quartile threshold.
  window.top.postMessage(
      {
        data: {
          event: 'scroll',
          percent_scrolled: '25',  // 0 || 25 || 50 || 75 || 100
        },
      },
      'chrome://whats-new/');
};
