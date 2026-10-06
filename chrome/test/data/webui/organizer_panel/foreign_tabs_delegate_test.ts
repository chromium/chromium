// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {foreignTabsBrowserProxyFactory, ForeignTabsDelegate, ForeignTabsPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {ForeignTab} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('ForeignTabsDelegateTest', () => {
  let delegate: ForeignTabsDelegate;
  let mockHandler: TestMock<ForeignTabsPageHandlerRemote>&
      ForeignTabsPageHandlerRemote;

  const sampleTabs: ForeignTab[] = [
    {
      title: 'Google',
      url: 'https://www.google.com',
      lastActiveElapsedText: '5 mins ago',
      deviceName: 'Pixel 8',
    },
    {
      title: 'YouTube',
      url: 'https://www.youtube.com',
      lastActiveElapsedText: '1 hour ago',
      deviceName: 'Chromebook',
    },
    {
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
          },
          {
            title: 'YouTube',
            elapsed: '1 hour ago',
            device: 'Chromebook',
          },
          {
            title: 'GitHub',
            elapsed: 'Yesterday',
            device: 'MacBook Pro',
          },
        ];

        for (let i = 0; i < 3; i++) {
          const item = items[i]!;
          const expected = expectedTabs[i]!;
          assertEquals(expected.title, item.title[0]);
          assertTrue(!!item.description);
          assertEquals(2, item.description.length);
          assertEquals(expected.elapsed, item.description[0]!.text);
          assertEquals(expected.device, item.description[1]!.text);
          assertEquals(sampleTabs[i]!.url, item.prefixIcon?.url);
        }
        assertEquals(1, mockHandler.getCallCount('getForeignTabs'));
      });
});
