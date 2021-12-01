// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://read-later.top-chrome/app.js';

import {ReadLaterAppElement} from 'chrome://read-later.top-chrome/app.js';
import {ReadLaterEntriesByStatus} from 'chrome://read-later.top-chrome/read_later.mojom-webui.js';
import {ReadLaterApiProxyImpl} from 'chrome://read-later.top-chrome/read_later_api_proxy.js';
import {ReadLaterItemElement} from 'chrome://read-later.top-chrome/read_later_item.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestReadLaterApiProxy} from './test_read_later_api_proxy.js';

suite('ReadLaterAppTest', () => {
  let readLaterApp: ReadLaterAppElement;
  let testProxy: TestReadLaterApiProxy;

  function assertEntryURLs(items: NodeListOf<HTMLElement>, urls: string[]) {
    assertEquals(urls.length, items.length);
    items.forEach((item, index) => {
      assertEquals(urls[index], item.dataset['url']);
    });
  }

  function queryItems() {
    return readLaterApp.shadowRoot!.querySelectorAll('read-later-item');
  }

  function clickItem(url: string) {
    readLaterApp.shadowRoot!.querySelector<HTMLElement>(
                                `[data-url="${url}"]`)!.click();
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
      ]
    };

    return entries;
  }

  setup(async () => {
    testProxy = new TestReadLaterApiProxy();
    ReadLaterApiProxyImpl.setInstance(testProxy);
    testProxy.setEntries(getSampleData());
    document.body.innerHTML = '';

    readLaterApp = document.createElement('read-later-app');
    document.body.appendChild(readLaterApp);
    await flushTasks();
  });

  test('return all entries', async () => {
    const urls = [
      'https://www.google.com', 'https://www.apple.com', 'https://www.bing.com',
      'https://www.yahoo.com'
    ];
    assertEntryURLs(queryItems(), urls);
  });

  test('click on item passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    clickItem(expectedUrl);
    const [url, updateReadStatus] = await testProxy.whenCalled('openURL');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('click on item passes event info', async () => {
    const item = readLaterApp.shadowRoot!.querySelector(
        `[data-url="https://www.apple.com"]`)!;
    item.dispatchEvent(new MouseEvent('click'));
    const [, , click] = await testProxy.whenCalled('openURL');
    assertFalse(
        click.middleButton || click.altKey || click.ctrlKey || click.metaKey ||
        click.shiftKey);
    testProxy.resetResolver('openURL');

    // Middle mouse button click.
    item.dispatchEvent(new MouseEvent('auxclick', {button: 1}));
    const [, , auxClick] = await testProxy.whenCalled('openURL');
    assertTrue(auxClick.middleButton);
    assertFalse(
        auxClick.altKey || auxClick.ctrlKey || auxClick.metaKey ||
        auxClick.shiftKey);
    testProxy.resetResolver('openURL');

    // Modifier keys.
    item.dispatchEvent(new MouseEvent('click', {
      altKey: true,
      ctrlKey: true,
      metaKey: true,
      shiftKey: true,
    }));
    const [, , modifiedClick] = await testProxy.whenCalled('openURL');
    assertFalse(modifiedClick.middleButton);
    assertTrue(
        modifiedClick.altKey && modifiedClick.ctrlKey &&
        modifiedClick.metaKey && modifiedClick.shiftKey);
  });

  test('Click on item mark as read button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readLaterItem =
        readLaterApp.shadowRoot!.querySelector<ReadLaterItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readLaterItemUpdateStatusButton = readLaterItem.$.updateStatusButton;
    readLaterItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertTrue(read);
  });

  test('Click on item mark as unread button triggers actions', async () => {
    const expectedUrl = 'https://www.bing.com';

    const readLaterItem =
        readLaterApp.shadowRoot!.querySelector<ReadLaterItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readLaterItemUpdateStatusButton = readLaterItem.$.updateStatusButton;
    readLaterItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertFalse(read);
  });

  test('Click on item delete button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readLaterItem =
        readLaterApp.shadowRoot!.querySelector<ReadLaterItemElement>(
            `[data-url="${expectedUrl}"]`)!;
    const readLaterItemDeleteButton = readLaterItem.$.deleteButton;
    readLaterItemDeleteButton.click();
    const url = await testProxy.whenCalled('removeEntry');
    assertEquals(expectedUrl, url.url);
  });

  test('Click on menu button triggers actions', async () => {
    const readLaterCloseButton =
        readLaterApp.shadowRoot!.querySelector<HTMLElement>('#closeButton')!;
    readLaterCloseButton.click();
    await testProxy.whenCalled('closeUI');
  });

  test('Enter key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readLaterItem =
        readLaterApp.shadowRoot!.querySelector<ReadLaterItemElement>(
            `[data-url="${expectedUrl}"]`)!;

    keyDownOn(readLaterItem, 0, [], 'Enter');
    const [url, updateReadStatus] = await testProxy.whenCalled('openURL');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('Space key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readLaterItem =
        readLaterApp.shadowRoot!.querySelector<ReadLaterItemElement>(
            `[data-url="${expectedUrl}"]`)!;

    keyDownOn(readLaterItem, 0, [], ' ');
    const [url, updateReadStatus] = await testProxy.whenCalled('openURL');
    assertEquals(url.url, expectedUrl);
    assertTrue(updateReadStatus);
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    const urls = [
      'https://www.google.com', 'https://www.apple.com', 'https://www.bing.com',
      'https://www.yahoo.com'
    ];
    const selector = readLaterApp.shadowRoot!.querySelector('iron-selector')!;

    // Select first item.
    selector.selected =
        readLaterApp.shadowRoot!.querySelector(
                                    'read-later-item')!.dataset['url']!;

    keyDownOn(selector, 0, [], 'ArrowUp');
    assertEquals(urls[3], selector.selected);

    keyDownOn(selector, 0, [], 'ArrowDown');
    assertEquals(urls[0], selector.selected);

    keyDownOn(selector, 0, [], 'ArrowDown');
    assertEquals(urls[1], selector.selected);

    keyDownOn(selector, 0, [], 'ArrowUp');
    assertEquals(urls[0], selector.selected);
  });

  test(
      'Keyboard navigation left/right cycles through list item elements',
      async () => {
        const firstItem =
            readLaterApp.shadowRoot!.querySelector('read-later-item')!;
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
        assertEquals(firstItem, readLaterApp.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.$.deleteButton, firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.$.updateStatusButton,
            firstItem.shadowRoot!.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(firstItem, readLaterApp.shadowRoot!.activeElement);
      });

  test('Favicons present in the dom', async () => {
    const readLaterItems =
        readLaterApp.shadowRoot!.querySelectorAll('read-later-item');

    readLaterItems.forEach((readLaterItem) => {
      assertTrue(!!readLaterItem.shadowRoot!.querySelector('.favicon'));
    });
  });

  test('Verify visibilitychange triggers data fetch', async () => {
    assertEquals(1, testProxy.getCallCount('getReadLaterEntries'));

    // When hidden visibilitychange should not trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(1, testProxy.getCallCount('getReadLaterEntries'));

    // When visible visibilitychange should trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(2, testProxy.getCallCount('getReadLaterEntries'));
  });
});
