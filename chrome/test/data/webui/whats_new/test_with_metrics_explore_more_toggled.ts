// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // The "Explore More" section was expanded.
  window.top.postMessage(
      {
        data: {
          event: 'explore_more_open',
          module_name: 'archive',
        },
      },
      'chrome://whats-new/');

  // The "Explore More" section was collapsed.
  window.top.postMessage(
      {
        data: {
          event: 'explore_more_close',
          module_name: 'archive',
        },
      },
      'chrome://whats-new/');
};
