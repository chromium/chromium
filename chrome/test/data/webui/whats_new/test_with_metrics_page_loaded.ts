// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // WNP version 123 has loaded.
  window.top.postMessage(
      {
        data: {
          event: 'page_loaded',
          type: 'version',
          version: 128,
        },
      },
      'chrome://whats-new/');
};
