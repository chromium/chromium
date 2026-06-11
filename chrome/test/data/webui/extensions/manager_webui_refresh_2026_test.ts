// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {COLORS_CSS_SELECTOR} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ExtensionManagerWebuiRefresh2026', function() {
  const WEBUI_REFRESH_ATTR = 'webui-refresh-2026';
  let manager: ExtensionsManagerElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({webuiRefresh2026: ''});
  });

  function createManager() {
    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);
    return microtasksFinished();
  }

  test('Enabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: WEBUI_REFRESH_ATTR});
    await createManager();

    assertTrue(!!document.body.querySelector(COLORS_CSS_SELECTOR));
  });

  test('Disabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    await createManager();

    assertFalse(!!document.body.querySelector(COLORS_CSS_SELECTOR));
  });
});
