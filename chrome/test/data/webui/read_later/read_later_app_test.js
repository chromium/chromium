// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadLaterAppElement} from 'chrome://read-later.top-chrome/app.js';
import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from 'chrome://read-later.top-chrome/read_later_api_proxy.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {flushTasks} from '../test_util.m.js';

import {TestReadLaterApiProxy} from './test_read_later_api_proxy.js';

suite('ReadLaterAppTest', () => {
  /** @type {!ReadLaterAppElement} */
  let readLaterApp;
  /** @type {!TestReadLaterApiProxy} */
  let testProxy;

  /**
   * @param {!NodeList<!Element>} items
   * @param {!Array<string>} urls
   */
  function assertEntryURLs(items, urls) {
    assertEquals(urls.length, items.length);
    items.forEach((item, index) => {
      assertEquals(urls[index], item.dataset.url);
    });
  }

  /** @return {!NodeList<!Element>} */
  function queryItems() {
    return readLaterApp.shadowRoot.querySelectorAll('read-later-item');
  }

  /** @param {string} url */
  function clickItem(url) {
    readLaterApp.shadowRoot.querySelector(`[data-url="${url}"]`).click();
  }

  /** @return {!readLater.mojom.ReadLaterEntriesByStatus} */
  function getSampleData() {
    const entries = {
      unreadEntries: [
        {
          title: 'Google',
          url: {url: 'https://www.google.com'},
          displayUrl: 'google.com',
          updateTime: 0,
          read: false,
          displayTimeSinceUpdate: '2 minutes ago',
        },
        {
          title: 'Apple',
          url: {url: 'https://www.apple.com'},
          displayUrl: 'apple.com',
          updateTime: 0,
          read: false,
          displayTimeSinceUpdate: '20 minutes ago',
        },
      ],
      readEntries: [
        {
          title: 'Bing',
          url: {url: 'https://www.bing.com'},
          displayUrl: 'bing.com',
          updateTime: 0,
          read: true,
          displayTimeSinceUpdate: '5 minutes ago',
        },
        {
          title: 'Yahoo',
          url: {url: 'https://www.yahoo.com'},
          displayUrl: 'yahoo.com',
          updateTime: 0,
          read: true,
          displayTimeSinceUpdate: '7 minutes ago',
        },
      ]
    };

    return entries;
  }

  setup(async () => {
    testProxy = new TestReadLaterApiProxy();
    ReadLaterApiProxyImpl.instance_ = testProxy;
    testProxy.setEntries(getSampleData());
    document.body.innerHTML = '';

    readLaterApp = /** @type {!ReadLaterAppElement} */ (
        document.createElement('read-later-app'));
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
    const url = await testProxy.whenCalled('openSavedEntry');
    assertEquals(url.url, expectedUrl);
  });

  test('Click on item mark as read button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readLaterItem =
        readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`);
    const readLaterItemUpdateStatusButton =
        readLaterItem.shadowRoot.querySelector('#updateStatusButton');
    readLaterItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertTrue(read);
  });

  test('Click on item mark as unread button triggers actions', async () => {
    const expectedUrl = 'https://www.bing.com';

    const readLaterItem =
        readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`);
    const readLaterItemUpdateStatusButton =
        readLaterItem.shadowRoot.querySelector('#updateStatusButton');
    readLaterItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url.url);
    assertFalse(read);
  });

  test('Click on item delete button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readLaterItem =
        readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`);
    const readLaterItemDeleteButton =
        readLaterItem.shadowRoot.querySelector('#deleteButton');
    readLaterItemDeleteButton.click();
    const url = await testProxy.whenCalled('removeEntry');
    assertEquals(expectedUrl, url.url);
  });

  test('Click on menu button triggers actions', async () => {
    const readLaterCloseButton =
        readLaterApp.shadowRoot.querySelector('#closeButton');
    readLaterCloseButton.click();
    await testProxy.whenCalled('closeUI');
  });

  test('Enter key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readLaterItem = /** @type {!Element} */
        (readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`));

    keyDownOn(readLaterItem, 0, [], 'Enter');
    const url = await testProxy.whenCalled('openSavedEntry');
    assertEquals(url.url, expectedUrl);
  });

  test('Space key triggers action and passes correct url', async () => {
    const expectedUrl = 'https://www.apple.com';
    const readLaterItem = /** @type {!Element} */
        (readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`));

    keyDownOn(readLaterItem, 0, [], ' ');
    const url = await testProxy.whenCalled('openSavedEntry');
    assertEquals(url.url, expectedUrl);
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    const urls = [
      'https://www.google.com', 'https://www.apple.com', 'https://www.bing.com',
      'https://www.yahoo.com'
    ];
    const selector = readLaterApp.shadowRoot.querySelector('iron-selector');

    // Select first item.
    selector.selected =
        readLaterApp.shadowRoot.querySelector('read-later-item').dataset.url;

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
            readLaterApp.shadowRoot.querySelector('read-later-item');
        // Focus first item.
        firstItem.focus();

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(
            firstItem.shadowRoot.getElementById('updateStatusButton'),
            firstItem.shadowRoot.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(
            firstItem.shadowRoot.getElementById('deleteButton'),
            firstItem.shadowRoot.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowRight');
        assertEquals(firstItem, readLaterApp.shadowRoot.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.shadowRoot.getElementById('deleteButton'),
            firstItem.shadowRoot.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(
            firstItem.shadowRoot.getElementById('updateStatusButton'),
            firstItem.shadowRoot.activeElement);

        keyDownOn(firstItem, 0, [], 'ArrowLeft');
        assertEquals(firstItem, readLaterApp.shadowRoot.activeElement);
      });

  test('Favicons present in the dom', async () => {
    const readLaterItems = /** @type {!NodeList<!Element>} */
        (readLaterApp.shadowRoot.querySelectorAll('read-later-item'));

    readLaterItems.forEach((readLaterItem) => {
      assertTrue(!!readLaterItem.shadowRoot.querySelector('.favicon'));
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
