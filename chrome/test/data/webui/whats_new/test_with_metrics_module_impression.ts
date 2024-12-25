// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // A module intersected with the viewport.
  window.top.postMessage(
      {
        data: {
          event: 'module_impression',
          module_name: 'ChromeFeature',
          section: 'spotlight',
          order: '1',
        },
      },
      'chrome://whats-new/');
};
