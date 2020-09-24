// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadLaterAppElement} from 'chrome://read-later/app.js';
import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from 'chrome://read-later/read_later_api_proxy.js';

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
      assertEquals(urls[index], item.data.url);
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
          url: 'https://www.google.com',
          displayUrl: 'google.com',
          updateTime: 0,
          read: false,
          displayTimeSinceUpdate: '2 minutes ago',
        },
        {
          title: 'Apple',
          url: 'https://www.apple.com',
          displayUrl: 'apple.com',
          updateTime: 0,
          read: false,
          displayTimeSinceUpdate: '20 minutes ago',
        },
      ],
      readEntries: [
        {
          title: 'Bing',
          url: 'https://www.bing.com',
          displayUrl: 'bing.com',
          updateTime: 0,
          read: true,
          displayTimeSinceUpdate: '5 minutes ago',
        },
        {
          title: 'Yahoo',
          url: 'https://www.yahoo.com',
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
    assertEquals(url, expectedUrl);
  });

  test('Click on item mark as read button triggers actions', async () => {
    const expectedUrl = 'https://www.apple.com';

    const readLaterItem =
        readLaterApp.shadowRoot.querySelector(`[data-url="${expectedUrl}"]`);
    const readLaterItemUpdateStatusButton =
        readLaterItem.shadowRoot.querySelector('#updateStatusButton');
    readLaterItemUpdateStatusButton.click();
    const [url, read] = await testProxy.whenCalled('updateReadStatus');
    assertEquals(expectedUrl, url);
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
    assertEquals(expectedUrl, url);
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
    assertEquals(expectedUrl, url);
  });
});
