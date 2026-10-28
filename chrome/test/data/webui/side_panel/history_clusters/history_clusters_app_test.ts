// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';

import type {HistoryClustersAppElement} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {historyClustersBrowserProxyFactory, PageHandlerRemote} from 'chrome://history-clusters-side-panel.top-chrome/history_clusters.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HistoryClustersAppTest', () => {
  let app: HistoryClustersAppElement;
  let clustersHandler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    clustersHandler = TestMock.fromClass(PageHandlerRemote);
    const {instance: clustersProxy} =
        historyClustersBrowserProxyFactory.createForTest(clustersHandler);
    historyClustersBrowserProxyFactory.setInstance(clustersProxy);

    app = document.createElement('history-clusters-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('ScrollContainer', () => {
    assertEquals(app.$.historyClusters, app.$.historyClusters.scrollTarget);
    assertEquals(
        'sp-scroller sp-scroller-bottom-of-page',
        app.$.historyClusters.className);
  });
});
