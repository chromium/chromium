// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {foreignTabsBrowserProxyFactory, ForeignTabsDelegate, ForeignTabsPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {ForeignTab} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('ForeignTabsDelegateTest', () => {
  let delegate: ForeignTabsDelegate;
  let mockHandler: TestMock<ForeignTabsPageHandlerRemote>&
      ForeignTabsPageHandlerRemote;

  const sampleTabs: ForeignTab[] = [
    {
      sessionTag: 'session_phone',
      sessionTabId: 1,
      title: 'Google',
      url: 'https://www.google.com',
      lastActiveElapsedText: '5 mins ago',
      deviceName: 'Pixel 8',
    },
    {
      sessionTag: 'session_chromebook',
      sessionTabId: 2,
      title: 'YouTube',
      url: 'https://www.youtube.com',
      lastActiveElapsedText: '1 hour ago',
      deviceName: 'Chromebook',
    },
    {
      sessionTag: 'session_macbook',
      sessionTabId: 3,
      title: 'GitHub',
      url: 'https://www.github.com',
      lastActiveElapsedText: 'Yesterday',
      deviceName: 'MacBook Pro',
    },
  ];

  setup(() => {
    loadTimeData.resetForTesting({
      tabsOnOtherDevices: 'Tabs on other devices',
    });
    mockHandler = TestMock.fromClass(ForeignTabsPageHandlerRemote);
    mockHandler.setResultFor(
        'getForeignTabs', Promise.resolve({tabs: [...sampleTabs]}));
    foreignTabsBrowserProxyFactory.setInstance({handler: mockHandler});
    delegate = new ForeignTabsDelegate();
  });

  test('returns id and localized header', () => {
    assertEquals('cross-device-tabs', delegate.getId());
    assertEquals('Tabs on other devices', delegate.getHeader());
  });

  test(
      'getItems returns tabs with elapsed time and device descriptions',
      async () => {
        const items = await delegate.getItems();
        assertEquals(3, items.length);

        const expectedTabs = [
          {
            title: 'Google',
            elapsed: '5 mins ago',
            device: 'Pixel 8',
            hostname: 'www.google.com',
          },
          {
            title: 'YouTube',
            elapsed: '1 hour ago',
            device: 'Chromebook',
            hostname: 'www.youtube.com',
          },
          {
            title: 'GitHub',
            elapsed: 'Yesterday',
            device: 'MacBook Pro',
            hostname: 'www.github.com',
          },
        ];

        for (let i = 0; i < 3; i++) {
          const item = items[i]!;
          const expected = expectedTabs[i]!;
          assertEquals(expected.title, item.title[0]);
          assertTrue(!!item.description);
          assertEquals(2, item.description.length);
          assertDeepEquals(
              {text: expected.elapsed, matchable: false}, item.description[0]);
          assertDeepEquals({text: expected.device}, item.description[1]);
          assertDeepEquals([expected.hostname], item.extraSearchTerms);
          assertEquals(sampleTabs[i]!.url, item.prefixIcon?.url);
        }
        assertEquals(1, mockHandler.getCallCount('getForeignTabs'));
      });

  test('opens tab when an item is clicked', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);

    delegate.onItemClick(items[1]!);

    assertEquals(1, mockHandler.getCallCount('openTab'));
    const [sessionTag, sessionTabId] = mockHandler.getArgs('openTab')[0];
    assertEquals(sampleTabs[1]!.sessionTag, sessionTag);
    assertEquals(sampleTabs[1]!.sessionTabId, sessionTabId);
  });
});
