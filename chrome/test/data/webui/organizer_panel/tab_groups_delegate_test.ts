// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Color, TabGroupDotSize, tabGroupsBrowserProxyFactory, TabGroupsDelegate, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {TabGroup} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('TabGroupsDelegateTest', () => {
  let delegate: TabGroupsDelegate;
  let mockHandler: TestMock<TabGroupsOrganizerPageHandlerRemote>&
      TabGroupsOrganizerPageHandlerRemote;

  const sampleGroups: TabGroup[] = [
    {
      id: {value: '00000000-0000-0000-0000-000000000001'},
      color: Color.kBlue,
      title: 'Sample Group 1',
    },
    {
      id: {value: '00000000-0000-0000-0000-000000000002'},
      color: Color.kRed,
      title: 'Sample Group 2',
    },
    {
      id: {value: '00000000-0000-0000-0000-000000000003'},
      color: Color.kGreen,
      title: 'Sample Group 3',
    },
  ];

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      tabGroups: 'Tab Groups',
    });
    mockHandler = TestMock.fromClass(TabGroupsOrganizerPageHandlerRemote);
    mockHandler.setResultFor(
        'getTabGroups', Promise.resolve({tabGroups: sampleGroups}));
    tabGroupsBrowserProxyFactory.setInstance({handler: mockHandler});
    delegate = new TabGroupsDelegate();
  });

  test('returns header', () => {
    assertEquals('Tab Groups', delegate.getHeader());
  });

  test('returns tab groups', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);
    assertEquals('Sample Group 1', items[0]!.title[0]);
    assertEquals('Sample Group 2', items[1]!.title[0]);
    assertEquals('Sample Group 3', items[2]!.title[0]);
    assertEquals(1, mockHandler.getCallCount('getTabGroups'));

    const container = document.createElement('div');
    document.body.appendChild(container);
    render(items[0]!.prefixIcon!.element!, container);
    const dot = container.querySelector('tab-group-dot');
    assertTrue(!!dot);
    assertEquals(Color.kBlue, dot.color);
    assertEquals(TabGroupDotSize.LARGE, dot.size);
  });
});
