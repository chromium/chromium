// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

window.onload = function() {
  assertTrue(!!window.top);

  // The user clicks a link in a module.
  window.top.postMessage(
      {
        data: {
          event: 'general_link_click',
          link_text: 'Google Search',
          link_type: 'external',
          link_url: 'https://google.com',
          module_name: '100-feature-with-link',
          section: 'spotlight',
          order: '1',
        },
      },
      'chrome://whats-new/');
};
