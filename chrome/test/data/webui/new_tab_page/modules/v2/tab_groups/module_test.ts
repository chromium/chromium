// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabGroupsModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {tabGroupsDescriptor, TabGroupsProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import type {TabGroup} from 'chrome://new-tab-page/tab_groups.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../../test_support.js';

suite('NewTabPageModulesTabGroupsModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    handler = installMock(
        PageHandlerRemote,
        mock => TabGroupsProxyImpl.setInstance(new TabGroupsProxyImpl(mock)));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  async function createModule(tabGroups: TabGroup[]):
      Promise<TabGroupsModuleElement> {
    handler.setResultFor('getTabGroups', Promise.resolve({tabGroups}));
    const module =
        await tabGroupsDescriptor.initialize(0) as TabGroupsModuleElement;
    document.body.append(module);
    await microtasksFinished();
    return module;
  }

  test('No module created if no tab groups data', async () => {
    const module = await createModule([]);
    assertEquals(null, module);
  });

  test('creates module', async () => {
    // Arrange.
    const tabGroups: TabGroup[] = [
      {
        title: 'Tab Group 1',
        url: {url: 'http://www.google.com/'},
      },
      {
        title: 'Tab Group 2',
        url: {url: 'http://www.google.com/'},
      },
    ];
    const module = await createModule(tabGroups);

    // Assert.
    // Verify the module was created and is visible.
    assertTrue(!!module);
    assertTrue(
        isVisible(module.shadowRoot.querySelector('ntp-module-header-v2')));

    // Verify the tab groups info is correct.
    const groups =
        module.shadowRoot.querySelectorAll<HTMLAnchorElement>('.tab-group');
    assertTrue(!!groups);
    assertEquals(tabGroups.length, groups.length);

    assertTrue(!!groups[0]);
    assertEquals(
        'Tab Group 1',
        groups[0].querySelector('.tab-group-title')!.textContent);
    assertEquals('http://www.google.com/', groups[0].href);

    assertTrue(!!groups[1]);
    assertEquals(
        'Tab Group 2',
        groups[1].querySelector('.tab-group-title')!.textContent);
    assertEquals('http://www.google.com/', groups[1].href);
  });
});
