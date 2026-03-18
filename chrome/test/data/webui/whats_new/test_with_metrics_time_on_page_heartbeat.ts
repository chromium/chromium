// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // Total time spent on page as a heartbeat.
  window.top.postMessage(
      {
        data: {
          event: 'time_on_page_heartbeat_ms',
          time: 3000,  // milliseconds
        },
      },
      'chrome://whats-new/');

  // Send another event to signal that the heartbeat has been processed.
  window.top.postMessage(
      {
        data: {
          event: 'cta_click',
        },
      },
      'chrome://whats-new/');
};
