// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DownloadListElement} from 'chrome://download-shelf.top-chrome/download_list.js';
import {DownloadShelfApiProxyImpl} from 'chrome://download-shelf.top-chrome/download_shelf_api_proxy.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {TestDownloadShelfApiProxy} from './test_download_shelf_api_proxy.js';

suite('DownloadListTest', function() {
  /** @type {!DownloadListElement} */
  let downloadListElement;

  /** @type {!TestDownloadShelfApiProxy} */
  let testProxy;

  function testItem(id) {
    return Object.assign(
        {
          id: 1,
          filename: 'test.exe',
          state: 'complete',
          paused: false,
        },
        {id});
  }

  function verifyDownloadIds(ids) {
    assertDeepEquals(
        [...downloadListElement.$all('download-item')].map(el => el.item.id),
        ids);
  }

  setup(async () => {
    testProxy = new TestDownloadShelfApiProxy();
    testProxy.setDownloadItems([testItem(1)]);
    DownloadShelfApiProxyImpl.instance_ = testProxy;
    downloadListElement = /** @type {!DownloadListElement} */ (
        document.createElement('download-list'));
    document.body.appendChild(downloadListElement);
    await flushTasks();
  });

  test('onLoad', () => {
    verifyDownloadIds([1]);
  });

  test('onEvent', () => {
    verifyDownloadIds([1]);
    testProxy.create(testItem(2));
    testProxy.create(testItem(3));
    verifyDownloadIds([3, 2, 1]);
    testProxy.erase(3);
    verifyDownloadIds([2, 1]);
    assertEquals(false, downloadListElement.$('download-item').item.paused);
    testProxy.change({id: 2, paused: {previous: false, current: true}});
    assertEquals(true, downloadListElement.$('download-item').item.paused);
  });

  test('onResize', async () => {
    const listElement = downloadListElement.$('#downloadList');
    listElement.style.width = '847px';
    await waitAfterNextRender(listElement);
    for (let i = 0; i < 10; ++i) {
      testProxy.create(testItem(i + 1));
    }
    const oldWidth = listElement.offsetWidth;
    assertEquals(4, downloadListElement.$all('download-item').length);
    listElement.style.width = oldWidth * 2 + 'px';
    await waitAfterNextRender(listElement);
    assertEquals(7, downloadListElement.$all('download-item').length);
    listElement.style.width = oldWidth + 'px';
    await waitAfterNextRender(listElement);
    assertEquals(4, downloadListElement.$all('download-item').length);
  });
});
