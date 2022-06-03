// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DangerType, DownloadItem, DownloadListElement, DownloadMode, DownloadShelfApiProxyImpl, DownloadState, MixedContentStatus} from 'chrome://download-shelf.top-chrome/download_shelf.js';

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {TestDownloadShelfApiProxy} from './test_download_shelf_api_proxy.js';

suite('DownloadListTest', function() {
  /** @type {!DownloadListElement} */
  let downloadListElement;

  /** @type {!TestDownloadShelfApiProxy} */
  let testProxy;

  /**
   * @param {number} id
   * @param {boolean} isPaused
   * @return {!DownloadItem}
   */
  function testItem(id, isPaused = false) {
    return {
      allowDownloadFeedback: false,
      dangerType: DangerType.kNotDangerous,
      isDangerous: false,
      isMalicious: false,
      id,
      fileNameDisplayString: 'test.exe',
      isPaused,
      mixedContentStatus: MixedContentStatus.kSafe,
      mode: DownloadMode.kNormal,
      originalUrl: {url: ''},
      receivedBytes: BigInt(1),
      shouldOpenWhenComplete: false,
      shouldPromoteOrigin: false,
      showDownloadStartTime: Date.now(),
      state: DownloadState.kComplete,
      statusText: '',
      tooltipText: '',
      totalBytes: BigInt(1),
      warningConfirmButtonText: '',
      warningText: '',
    };
  }

  function verifyDownloadIds(ids) {
    assertDeepEquals(
        [...downloadListElement.$all('download-item')].map(el => el.item.id),
        ids);
  }

  setup(async () => {
    testProxy = new TestDownloadShelfApiProxy();
    testProxy.setDownloadItems([testItem(1)]);
    DownloadShelfApiProxyImpl.setInstance(testProxy);
    downloadListElement = /** @type {!DownloadListElement} */ (
        document.createElement('download-list'));
    document.body.appendChild(downloadListElement);
    await flushTasks();
  });

  test('onLoad', () => {
    verifyDownloadIds([1]);
  });

  test('onEvent', async () => {
    verifyDownloadIds([1]);

    testProxy.getCallbackRouterRemote().onNewDownload(testItem(2));
    testProxy.getCallbackRouterRemote().onNewDownload(testItem(3));
    const listElement = downloadListElement.$('#downloadList');
    await waitAfterNextRender(listElement);
    verifyDownloadIds([3, 2, 1]);

    testProxy.getCallbackRouterRemote().onDownloadErased(3);
    await waitAfterNextRender(listElement);
    verifyDownloadIds([2, 1]);
    assertEquals(false, downloadListElement.$('download-item').item.isPaused);

    testProxy.getCallbackRouterRemote().onDownloadUpdated(testItem(2, true));
    await waitAfterNextRender(listElement);
    assertEquals(true, downloadListElement.$('download-item').item.isPaused);
  });

  test('onResize', async () => {
    const listElement = downloadListElement.$('#download-list');
    listElement.style.width = '847px';
    await waitAfterNextRender(listElement);
    for (let i = 0; i < 10; ++i) {
      testProxy.getCallbackRouterRemote().onNewDownload(testItem(i + 1));
    }
    await waitAfterNextRender(listElement);
    const oldWidth = listElement.offsetWidth;
    assertEquals(4, downloadListElement.$all('download-item').length);
    listElement.style.width = oldWidth * 2 + 'px';
    await waitAfterNextRender(listElement);
    assertEquals(8, downloadListElement.$all('download-item').length);
    listElement.style.width = oldWidth + 'px';
    await waitAfterNextRender(listElement);
    assertEquals(4, downloadListElement.$all('download-item').length);
  });
});
