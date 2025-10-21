// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);
  window.top.postMessage(
      {
        data: {
          event: 'carousel_scroll_button_click',
        },
      },
      'chrome://whats-new/');
};
