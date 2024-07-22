// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/reading_list_app.js';

import type {ReadLaterEntriesByStatus} from 'chrome://read-later.top-chrome/reading_list.mojom-webui.js';
import {ReadingListApiProxyImpl} from 'chrome://read-later.top-chrome/reading_list_api_proxy.js';
import type {ReadingListAppElement} from 'chrome://read-later.top-chrome/reading_list_app.js';
import type {ReadingListItemElement} from 'chrome://read-later.top-chrome/reading_list_item.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestReadingListApiProxy} from './test_reading_list_api_proxy.js';

suite('ReadingListAppTest', () => {
  let readingListApp: ReadingListAppElement;
  let testProxy: TestReadingListApiProxy;

  function assertEntryURLs(items: NodeListOf<HTMLElement>, urls: string[]) {
    assertEquals(urls.length, items.length);
    items.forEach((item, index) => {
      assertEquals(urls[index], item.dataset['url']);
    });
  }

  function queryItems() {
    return readingListApp.shadowRoot!.querySelectorAll('reading-list-item');
  }

  function clickItem(url: string) {
    readingListApp.shadowRoot!
        .querySelector<HTMLElement>(`[data-url="${url}"]`)!.click();
  }

  function getSampleData(): ReadLaterEntriesByStatus {
    const entries = {
      unreadEntries: [
        {
          title: 'Google',
          url: {url: 'https://www.google.com'},
          displayUrl: 'google.com',
          updateTime: 0n,
          read: false,
          displayTimeSinceUpdate: '2 minutes ago',
        },
        {
          title: 'Apple',
          url: {url: 'https://www.apple.com'},
          displayUrl: 'apple.com',
          updateTime: 0n,
          read: false,
          displayTimeSinceUpdate: '20 minutes ago',
        },
      ],
      readEntries: [
        {
          title: 'Bing',
          url: {url: 'https://www.bing.com'},
          displayUrl: 'bing.com',
          updateTime: 0n,
          read: true,
          displayTimeSinceUpdate: '5 minutes ago',
        },
        {
          title: 'Yahoo',
          url: {url: 'https://www.yahoo.com'},
          displayUrl: 'yahoo.com',
          updateTime: 0n,
          read: true,
          displayTimeSinceUpdate: '7 minutes ago',
        },
      ],
    };

    return entries;
  }

  setup(async () => {
    testProxy = new TestReadingListApiProxy();
    ReadingListApiProxyImpl.setInstance(testProxy);
    testProxy.setEntries(getSampleData());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    readingListApp = document.createElement('reading-list-app');
    document.body.appendChild(readingListApp);
    await microtasksFinished();
  });

  test('return all entries', async () => {
    const urls = [
      'https://www.google.com',
      'https://www.apple.com',
      'https://www.bing.com',
      'https://www.yahoo.com',
    ];
    assertEntryURLs(queryItems(), urls);
  });

  test('click on item passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    clickItem(expectedUrl);
    const [url, updateReadStatus] = await testProxy.whenCalled('openUrl');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('click on item passes event info', async () => {
    const item = readingListApp.shadowRoot!.querySelector(
        `[data-url="https://www.apple.com"]`)!;
    item.dispatchEvent(new MouseEvent('click'));
    const [, , click] = await testProxy.whenCalled('openUrl');
    assertFalse(
        click.middleButton || click.altKey || click.ctrlKey || click.metaKey ||
        click.shiftKey);
    testProxy.resetResolver('openUrl');

    // Middle mouse button click.
    item.dispatchEvent(new MouseEvent('auxclick', {button: 1}));
    const [, , auxClick] = await testProxy.whenCalled('openUrl');
    assertTrue(auxClick.middleButton);
    assertFalse(
        auxClick.altKey || auxClick.ctrlKey || auxClick.metaKey ||
        auxClick.shiftKey);
    testProxy.resetResolver('openUrl');

    // Modifier keys.
    item.dispatchEvent(new MouseEvent('click', {
      altKey: true,
      ctrlKey: true,
      metaKey: true,
      shiftKey: true,
    }));
    const [, , modifiedClick] = await testProxy.whenCalled('openUrl');
    assertFalse(modifiedClick.middleButton);
    assertTrue(
        modifiedClick.altKey && modifiedClick.ctrlKey &&
        modifiedClick.metaKey && modifiedClick.shiftKey);
  });

  test('Click on item mark as read button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readingListItem =
        readingListApp.shadowRoot!.querySelector<ReadingListItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readingListItemUpdateStatusButton =
        readingListItem.$.updateStatusButton;
    readingListItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertTrue(read);
  });

  test('Click on item mark as unread button triggers actions', async () => {
    const expectedUrl = 'https://www.bing.com';

    const readingListItem =
        readingListApp.shadowRoot!.querySelector<ReadingListItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readingListItemUpdateStatusButton =
        readingListItem.$.updateStatusButton;
    readingListItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertFalse(read);
  });

  test('Click on item delete button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readingListItem =
        readingListApp.shadowRoot!.querySelector<ReadingListItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readingListItemDeleteButton = readingListItem.$.deleteButton;
    readingListItemDeleteButton.click();
    const url = await testProxy.whenCalled('removeEntry');
    assertEquals(expectedUrl, url.url);
  });

  test('Enter key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readingListItem =
        readingListApp.shadowRoot!.querySelector<ReadingListItemElement>(
            `[data-url="${expectedUrl}"]`)!;

    keyDownOn(readingListItem, 0, [], 'Enter');
    const [url, updateReadStatus] = await testProxy.whenCalled('openUrl');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('Space key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readingListItem =
        readingListApp.shadowRoot!.querySelector<ReadingListItemElement>(
            `[data-url="${expectedUrl}"]`)!;

    keyDownOn(readingListItem, 0, [], ' ');
    const [url, updateReadStatus] = await testProxy.whenCalled('openUrl');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    const urls = [
      'https://www.google.com',
      'https://www.apple.com',
      'https://www.bing.com',
      'https://www.yahoo.com',
    ];

    // Select first item.
    readingListApp.selected =
        readingListApp.shadowRoot!.querySelector(
                                      'reading-list-item')!.dataset['url']!;

    keyDownOn(readingListApp.$.readingListList, 0, [], 'ArrowUp');
    assertEquals(urls[3], readingListApp.selected);

    keyDownOn(readingListApp.$.readingListList, 0, [], 'ArrowDown');
    assertEquals(urls[0], readingListApp.selected);

    keyDownOn(readingListApp.$.readingListList, 0, [], 'ArrowDown');
    assertEquals(urls[1], readingListApp.selected);

    keyDownOn(readingListApp.$.readingListList, 0, [], 'ArrowUp');
    assertEquals(urls[0], readingListApp.selected);
  });

  test(
      'Keyboard navigation left/right cycles through list item elements',
      async () => {
        const firstItem =
            readingListApp.shadowRoot!.querySelector('reading-list-item')!;
        // Focus first item.
        firstItem.focus();

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(
            firstItem.$.updateStatusButton,
            firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(
            firstItem.$.deleteButton, firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(firstItem, readingListApp.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.$.deleteButton, firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.$.updateStatusButton,
            firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(firstItem, readingListApp.shadowRoot!.activeElement);
      });

  test('Verify visibilitychange triggers data fetch', async () => {
    assertEquals(1, testProxy.getCallCount('getReadLaterEntries'));

    // When hidden visibilitychange should not trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(1, testProxy.getCallCount('getReadLaterEntries'));

    // When visible visibilitychange should trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(2, testProxy.getCallCount('getReadLaterEntries'));
  });
});
