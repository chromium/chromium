// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // The QR code dropdown was expanded.
  window.top.postMessage(
      {
        data: {
          event: 'expand_media',
          module_name: 'ChromeFeature',
          section: 'spotlight',
          order: 2,
        },
      },
      'chrome://whats-new/');

  // The QR code dropdown was collapsed.
  window.top.postMessage(
      {
        data: {
          event: 'close_expanded_media',
          module_name: 'ChromeFeature',
          section: 'spotlight',
          order: 2,
        },
      },
      'chrome://whats-new/');
};
