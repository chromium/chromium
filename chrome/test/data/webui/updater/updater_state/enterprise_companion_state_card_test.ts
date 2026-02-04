// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://updater/updater_state/enterprise_companion_state_card.js';

import {BrowserProxyImpl} from 'chrome://updater/browser_proxy.js';
import type {EnterpriseCompanionStateCardElement} from 'chrome://updater/updater_state/enterprise_companion_state_card.js';
import {PageHandlerRemote, ShowDirectoryTarget} from 'chrome://updater/updater_ui.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EnterpriseCompanionStateCardElement', () => {
  let item: EnterpriseCompanionStateCardElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('enterprise-companion-state-card');

    return microtasksFinished();
  });

  test('renders with all properties', async () => {
    item.version = '1.0.0.0';
    item.installPath = '/home/user/companion';
    document.body.appendChild(item);
    await microtasksFinished();

    const label = item.shadowRoot.querySelector('.label');
    assertTrue(!!label);
    assertEquals('Chrome Enterprise Companion App', label.textContent.trim());

    const values = item.shadowRoot.querySelectorAll('.value');
    assertEquals('1.0.0.0', values[1]!.textContent.trim());
    assertEquals('/home/user/companion', values[2]!.textContent.trim());
  });

  test('opens installation directory when clicked', async () => {
    const handler = TestMock.fromClass(PageHandlerRemote);
    BrowserProxyImpl.getInstance().handler = handler;

    item.version = '1.0.0.0';
    item.installPath = '/home/user/companion';
    document.body.appendChild(item);
    await microtasksFinished();

    const link = item.shadowRoot.querySelector<HTMLAnchorElement>('a.value');
    assertTrue(!!link);
    link.click();

    assertArrayEquals(
        [ShowDirectoryTarget.kEnterpriseCompanionApp],
        handler.getArgs('showDirectory'));
  });
});
