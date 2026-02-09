// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/updater_state/updater_state_card.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import {SCOPES} from 'chrome://updater/event_history.js';
import {formatDateLong} from 'chrome://updater/tools.js';
import type {UpdaterStateCardElement} from 'chrome://updater/updater_state/updater_state_card.js';
import {PageHandlerRemote, ShowDirectoryTarget} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UpdaterStateCardElement', () => {
  let item: UpdaterStateCardElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('updater-state-card');

    return microtasksFinished();
  });

  test('renders with all properties', async () => {
    item.scope = 'USER';
    item.version = '1.0.0.0';
    item.inactiveVersions = ['0.9.0.0'];
    item.lastChecked = new Date('2026-01-02T12:00:00');
    item.lastStarted = new Date('2026-01-01T12:00:00');
    item.installPath = '/home/user/updater';
    document.body.appendChild(item);
    await microtasksFinished();

    const label = item.shadowRoot.querySelector('.label');
    assertTrue(!!label);
    assertEquals(loadTimeData.getString('scopeUser'), label.textContent.trim());

    const values = item.shadowRoot.querySelectorAll('.value');
    assertEquals('1.0.0.0', values[1]!.textContent.trim());
    assertTrue(values[2]!.querySelectorAll('div').length > 0);
    assertStringContains(
        values[2]!.textContent,
        formatDateLong(new Date('2026-01-02T12:00:00')));
    assertStringContains(
        values[3]!.textContent,
        formatDateLong(new Date('2026-01-01T12:00:00')));
    assertEquals('/home/user/updater', values[4]!.textContent.trim());

    const inactiveVersions = values[5]!.querySelectorAll('li');
    assertEquals(1, inactiveVersions.length);
    assertEquals('0.9.0.0', inactiveVersions[0]!.textContent.trim());
  });

  test('renders with missing optional properties', async () => {
    item.scope = 'SYSTEM';
    item.version = '1.0.0.0';
    item.inactiveVersions = [];
    item.lastChecked = null;
    item.lastStarted = null;
    item.installPath = '/opt/google/updater';
    document.body.appendChild(item);
    await microtasksFinished();

    const label = item.shadowRoot.querySelector('.label');
    assertTrue(!!label);
    assertEquals(
        loadTimeData.getString('scopeSystem'), label.textContent.trim());

    const values = item.shadowRoot.querySelectorAll('.value');
    assertEquals(
        loadTimeData.getString('never'), values[2]!.textContent.trim());
    assertEquals(
        loadTimeData.getString('never'), values[3]!.textContent.trim());

    // Expected rows: Scope, Version, Last Checked, Last Started, Install Path
    const rows = item.shadowRoot.querySelectorAll('.row');
    assertEquals(5, rows.length);
  });

  suite('opens installation directory when clicked', () => {
    let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;

    setup(() => {
      handler = TestMock.fromClass(PageHandlerRemote);
      BrowserProxyImpl.getInstance().handler = handler;
    });

    SCOPES.forEach(scope => {
      test(`for ${scope} scope`, async () => {
        item.scope = scope;
        item.version = '1.0.0.0';
        item.inactiveVersions = ['0.9.0.0'];
        item.lastChecked = new Date('2026-01-02T12:00:00');
        item.lastStarted = new Date('2026-01-01T12:00:00');
        item.installPath = '/home/user/updater';
        document.body.appendChild(item);
        await microtasksFinished();

        const link =
            item.shadowRoot.querySelector<HTMLAnchorElement>('a.value');
        assertTrue(!!link);
        link.click();

        assertArrayEquals(
            [scope === 'SYSTEM' ? ShowDirectoryTarget.kSystemUpdater :
                                  ShowDirectoryTarget.kUserUpdater],
            handler.getArgs('showDirectory'));
      });
    });
  });
});
