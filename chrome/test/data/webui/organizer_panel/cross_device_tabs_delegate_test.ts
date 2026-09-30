// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrossDeviceTabsDelegate} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrossDeviceTabsDelegateTest', () => {
  let delegate: CrossDeviceTabsDelegate;

  setup(() => {
    loadTimeData.resetForTesting({
      tabsOnOtherDevices: 'Tabs on other devices',
    });
    delegate = new CrossDeviceTabsDelegate();
  });

  test('getHeader returns localized string', () => {
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
        }
      });
});
