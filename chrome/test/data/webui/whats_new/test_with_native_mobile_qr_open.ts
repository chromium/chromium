// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // The "Get Chrome for your phone" button was clicked on a page that was told
  // to use Chrome's native QR code bubble.
  window.top.postMessage(
      {
        data: {
          event: 'native_mobile_qr_open',
          module_name: 'main nav',
        },
      },
      'chrome://whats-new/');
};
