// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://management/management_ui.js';

import {COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ManagementUIWebuiRefresh2026', function() {
  const WEBUI_REFRESH_ATTR = 'webui-refresh-2026';
  let element: HTMLElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({webuiRefresh2026: ''});
  });

  function createElement() {
    element = document.createElement('management-ui');
    document.body.appendChild(element);
    return microtasksFinished();
  }

  test('Enabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: WEBUI_REFRESH_ATTR});
    await createElement();

    assertNotEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });

  test('Disabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    await createElement();

    assertEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });
});
