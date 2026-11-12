// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {foreignTabsBrowserProxyFactory, ForeignTabsDelegate, ForeignTabsPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {ForeignTab, OrganizerListSectionItemElement} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      signInOnPhone: 'Sign in on your phone',
    });
    mockHandler = TestMock.fromClass(ForeignTabsPageHandlerRemote);
    mockHandler.setResultFor('getForeignTabs', Promise.resolve({
      tabs: [...sampleTabs],
      canShowCrossDeviceSignInPromo: false,
    }));
    foreignTabsBrowserProxyFactory.setInstance({handler: mockHandler});
    delegate = new ForeignTabsDelegate();
  });

  test('returns id and localized header', () => {
    assertEquals('cross-device-tabs', delegate.getId());
    assertEquals('Tabs on other devices', delegate.getHeader());
    assertEquals('CrossDeviceTabs', delegate.getHistogramSectionName());
    assertEquals('OpenCrossDeviceTab', delegate.getItemClickAction());
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

  test('shouldHide defaults to true before getItems', () => {
    assertTrue(delegate.shouldHide());
  });

  test('shouldHide is true when empty and promo disallowed', async () => {
    mockHandler.setResultFor('getForeignTabs', Promise.resolve({
      tabs: [],
      canShowCrossDeviceSignInPromo: false,
    }));
    const items = await delegate.getItems();
    assertEquals(0, items.length);
    assertTrue(delegate.shouldHide());
  });

  test('shouldHide is false when empty and promo allowed', async () => {
    mockHandler.setResultFor('getForeignTabs', Promise.resolve({
      tabs: [],
      canShowCrossDeviceSignInPromo: true,
    }));
    const items = await delegate.getItems();
    assertEquals(0, items.length);
    assertFalse(delegate.shouldHide());
  });

  test('shouldHide is false with tabs and promo disallowed', async () => {
    mockHandler.setResultFor('getForeignTabs', Promise.resolve({
      tabs: [...sampleTabs],
      canShowCrossDeviceSignInPromo: false,
    }));
    const items = await delegate.getItems();
    assertEquals(3, items.length);
    assertFalse(delegate.shouldHide());
  });

  test('shouldHide is false with tabs and promo allowed', async () => {
    mockHandler.setResultFor('getForeignTabs', Promise.resolve({
      tabs: [...sampleTabs],
      canShowCrossDeviceSignInPromo: true,
    }));
    const items = await delegate.getItems();
    assertEquals(3, items.length);
    assertFalse(delegate.shouldHide());
  });

  test(
      'returns zero state and triggers showCrossDeviceSignInPromo on click',
      () => {
        const zeroState = delegate.getZeroState();
        assertTrue(!!zeroState);

        const container = document.createElement('div');
        document.body.appendChild(container);
        render(zeroState, container);

        const item = container.querySelector<OrganizerListSectionItemElement>(
            'organizer-list-section-item');
        assertTrue(!!item);
        assertDeepEquals(['Sign in on your phone'], item.item.title);

        const icon = item.item.prefixIcon?.element;
        assertTrue(!!icon);

        const iconContainer = document.createElement('div');
        document.body.appendChild(iconContainer);
        render(icon, iconContainer);
        const crIcon = iconContainer.querySelector<CrIconElement>('cr-icon');
        assertTrue(!!crIcon);
        assertEquals('organizer-panel:login', crIcon.icon);

        item.click();
        assertEquals(1, mockHandler.getCallCount('showCrossDeviceSignInPromo'));
      });
});
