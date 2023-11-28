// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Session, Tab, Window} from 'chrome://new-tab-page/history_types.mojom-webui.js';
import {tabResumptionDescriptor, TabResumptionModuleElement, TabResumptionProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/tab_resumption.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from '../../../test_support.js';

function createSampleSessions(count: number): Session[] {
  return new Array(count).fill(0).map((_, i) => createSampleSession(i, 3));
}

function createSampleSession(
    sessionId: number,
    numWindows: number,
    overrides?: Partial<Session>,
    ): Session {
  const session: Session = Object.assign(
      {
        tag: 'Sample Tag',
        name: 'Sample Name',
        modifiedTime: 1000,
        timestamp: new Date(2023, 0, 1),
        collapsed: false,
        windows: new Array(numWindows)
                     .fill(0)
                     .map((_, i) => createSampleWindow(sessionId, i, 3)),
      },
      overrides);

  return session;
}

function createSampleWindow(
    sessionId: number,
    windowId: number,
    numTabs: number,
    overrides?: Partial<Window>,
    ): Window {
  const window: Window = Object.assign(
      {
        timestamp: new Date(2023, 0, 1),
        sessionId: sessionId,
        tabs: new Array(numTabs).fill(0).map(() => createSampleTab(windowId)),
      },
      overrides);

  return window;
}

function createSampleTab(
    windowId: number,
    overrides?: Partial<Tab>,
    ): Tab {
  const tab: Tab = Object.assign(
      {
        windowId: windowId,
        url: {url: 'https://www.foo.com'},
        title: 'Test Tab Title',
        timestamp: new Date(2023, 0, 1),
      },
      overrides);

  return tab;
}

suite('NewTabPageModulesTabResumptionModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => TabResumptionProxyImpl.setInstance(
            new TabResumptionProxyImpl(mock)));
  });

  async function initializeModule(sessions: Session[]):
      Promise<TabResumptionModuleElement> {
    handler.setResultFor('getTabs', Promise.resolve({sessions}));
    const moduleElement = await tabResumptionDescriptor.initialize(0) as
        TabResumptionModuleElement;
    document.body.append(moduleElement);

    await waitAfterNextRender(document.body);
    return moduleElement;
  }

  suite('Core', () => {
    test('No module created if no tab resumption data', async () => {
      // Arrange.
      const moduleElement = await initializeModule([]);

      // Assert.
      assertEquals(null, moduleElement);
    });

    test('Module instance created successfully', async () => {
      const moduleElement = await initializeModule(createSampleSessions(1));
      assertTrue(!!moduleElement);
    });
  });
});
