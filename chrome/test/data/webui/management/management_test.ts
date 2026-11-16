// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://management/management_ui.js';

import {ManagementBrowserProxyImpl} from 'chrome://management/management_browser_proxy.js';
import {COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

suite('ManagementUIConnectors', function() {
  let element: HTMLElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('management-ui');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('SecureGatewayConnectorRenderedInTableAndList', async () => {
    await ManagementBrowserProxyImpl.getInstance().getThreatProtectionInfo();
    await microtasksFinished();

    webUIListenerCallback('threat-protection-info-updated', {
      description: 'Threat protection description',
      info: [{
        title: 'managementOnSecureGatewayEvent',
        permission: 'managementOnSecureGatewayVisibleData',
      }],
    });
    await microtasksFinished();

    const expectedEvent =
        loadTimeData.getString('managementOnSecureGatewayEvent');
    const expectedVisibleData =
        loadTimeData.getString('managementOnSecureGatewayVisibleData');
    assertTrue(expectedEvent.length > 0);
    assertTrue(expectedVisibleData.length > 0);

    // Wide-screen semantic table row.
    const tableRowCells = element.shadowRoot!.querySelectorAll<HTMLElement>(
        'table.wide-screen-only td');
    assertEquals(2, tableRowCells.length);
    assertEquals(expectedEvent, tableRowCells[0]!.textContent.trim());
    assertEquals(expectedVisibleData, tableRowCells[1]!.textContent.trim());

    // Small-screen semantic stacked list item.
    const listItem = element.shadowRoot!.querySelector<HTMLElement>(
        'ul.small-screen-only li.connector-item');
    assertNotEquals(null, listItem);
    assertTrue(listItem!.textContent.includes(expectedEvent));
    assertTrue(listItem!.textContent.includes(expectedVisibleData));
  });
});
