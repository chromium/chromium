// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Color, TabGroupDotSize, tabGroupsBrowserProxyFactory, TabGroupsDelegate, TabGroupsOrganizerPageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, TabGroup, TabGroupsOrganizerPageRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestClient implements OrganizerListSectionClient {
  items: Array<OrganizerListSectionItem<unknown>> = [];

  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items = items;
  }
}

suite('TabGroupsDelegateTest', () => {
  let delegate: TabGroupsDelegate;
  let mockHandler: TestMock<TabGroupsOrganizerPageHandlerRemote>&
      TabGroupsOrganizerPageHandlerRemote;
  let remotePage: TabGroupsOrganizerPageRemote;

  const sampleGroups: TabGroup[] = [
    {
      id: '1',
      color: Color.kBlue,
      title: 'Sample Group 1',
      isOpen: true,
    },
    {
      id: '2',
      color: Color.kRed,
      title: 'Sample Group 2',
      isOpen: false,
    },
    {
      id: '3',
      color: Color.kGreen,
      title: 'Sample Group 3',
      isOpen: true,
    },
  ];

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      tabGroupMoreOptions: 'More options',
      tabGroups: 'Tab Groups',
    });
    mockHandler = TestMock.fromClass(TabGroupsOrganizerPageHandlerRemote);
    mockHandler.setResultFor(
        'getTabGroups', Promise.resolve({tabGroups: [...sampleGroups]}));
    mockHandler.setResultFor('showContextMenu', Promise.resolve());
    const {instance, remote} =
        tabGroupsBrowserProxyFactory.createForTest(mockHandler);
    tabGroupsBrowserProxyFactory.setInstance(instance);
    remotePage = remote;
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

    assertTrue(!!items[0]!.hoveredActionButton);
    assertEquals('cr:more-vert', items[0]!.hoveredActionButton.icon);
    assertEquals('More options', items[0]!.hoveredActionButton.ariaLabel);

    const container = document.createElement('div');
    document.body.appendChild(container);
    render(items[0]!.prefixIcon!.element!, container);
    let dot = container.querySelector('tab-group-dot');
    assertTrue(!!dot);
    assertEquals(Color.kBlue, dot.color);
    assertTrue(dot.filled);
    assertEquals(TabGroupDotSize.LARGE, dot.size);

    render(items[1]!.prefixIcon!.element!, container);
    dot = container.querySelector('tab-group-dot');
    assertTrue(!!dot);
    assertEquals(Color.kRed, dot.color);
    assertFalse(dot.filled);
    assertEquals(TabGroupDotSize.LARGE, dot.size);
  });

  test('opens tab group when item is clicked', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);

    delegate.onItemClick(items[1]!);

    assertEquals(1, mockHandler.getCallCount('openTabGroup'));
    assertEquals(sampleGroups[1]!.id, mockHandler.getArgs('openTabGroup')[0]);
  });

  test('adds tab group to the top of the list', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    const newGroup: TabGroup = {
      id: '4',
      color: Color.kYellow,
      title: 'Sample Group 4',
      isOpen: true,
    };

    remotePage.tabGroupAdded(newGroup);
    await microtasksFinished();

    assertEquals(4, client.items.length);
    assertEquals('Sample Group 4', client.items[0]!.title[0]);
    assertEquals('Sample Group 1', client.items[1]!.title[0]);
    assertEquals('Sample Group 2', client.items[2]!.title[0]);
    assertEquals('Sample Group 3', client.items[3]!.title[0]);
  });

  test('removes tab group from list', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    remotePage.tabGroupRemoved('2');
    await microtasksFinished();

    assertEquals(2, client.items.length);
    assertEquals('Sample Group 1', client.items[0]!.title[0]);
    assertEquals('Sample Group 3', client.items[1]!.title[0]);
  });

  test('updates tab group without changing order', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    const updatedGroup: TabGroup = {
      id: '2',
      color: Color.kCyan,
      title: 'Updated Group 2',
      isOpen: true,
    };

    remotePage.tabGroupUpdated(updatedGroup);
    await microtasksFinished();

    assertEquals(3, client.items.length);
    assertEquals('Sample Group 1', client.items[0]!.title[0]);
    assertEquals('Updated Group 2', client.items[1]!.title[0]);
    assertEquals('Sample Group 3', client.items[2]!.title[0]);

    const container = document.createElement('div');
    document.body.appendChild(container);
    render(client.items[1]!.prefixIcon!.element!, container);
    const dot = container.querySelector('tab-group-dot');
    assertTrue(!!dot);
    assertEquals(Color.kCyan, dot.color);
    assertTrue(dot.filled);
  });

  test('calls showContextMenu on action button click', async () => {
    const items = await delegate.getItems();
    const button = document.createElement('button');
    button.getBoundingClientRect = () => ({
      x: 10,
      y: 20,
      width: 30,
      height: 40,
      top: 20,
      right: 40,
      bottom: 60,
      left: 10,
      toJSON: () => {},
    });

    delegate.onItemActionButtonClicked(items[0]!, button);

    const args = await mockHandler.whenCalled('showContextMenu');
    assertDeepEquals(sampleGroups[0]!.id, args[0]);
    assertDeepEquals({x: 10, y: 20, width: 30, height: 40}, args[1]);
  });

  test('calls showContextMenu on context menu click', async () => {
    const items = await delegate.getItems();

    delegate.onItemContextMenuClicked(items[0]!, 15, 25);

    const args = await mockHandler.whenCalled('showContextMenu');
    assertDeepEquals(sampleGroups[0]!.id, args[0]);
    assertDeepEquals({x: 15, y: 25, width: 0, height: 0}, args[1]);
  });
});
