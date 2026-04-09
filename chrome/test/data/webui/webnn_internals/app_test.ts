// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webnn-internals/app.js';

import type {WebnnInternalsAppElement} from 'chrome://webnn-internals/app.js';
import {BrowserProxy} from 'chrome://webnn-internals/browser_proxy.js';
import type {WebnnInternalsContextsViewerElement} from 'chrome://webnn-internals/contexts_viewer.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestWebnnInternalsBrowserProxy} from './test_webnn_internals_browser_proxy.js';

suite('WebnnInternalsUITest', function() {
  let app: WebnnInternalsAppElement;
  let contextsTab: WebnnInternalsContextsViewerElement;
  let testProxy: TestWebnnInternalsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestWebnnInternalsBrowserProxy();
    testProxy.handler.setResultFor(
        'requestExistingContextsDetails', Promise.resolve({
          contextsInfo: [],
        }));
    // <if expr="webnn_enable_graph_dump">
    testProxy.handler.setResultFor('isGraphRecording', Promise.resolve({
      enabled: false,
    }));
    // </if>
    BrowserProxy.setInstance(testProxy);
    app = document.createElement('webnn-internals-app');
    document.body.appendChild(app);
    const viewer =
        app.shadowRoot.querySelector('webnn-internals-contexts-viewer');
    assertTrue(!!viewer);
    contextsTab = viewer;
  });

  test('InitialViewWithNoContexts', function() {
    // Initial status, the list should not exist and the no contexts label
    // visible.
    const gridContainer =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.grid-container');
    assertFalse(!!gridContainer);
    const noContextText =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.no-context');
    assertTrue(!!noContextText);
  });

  test('ShowActiveContexts', async function() {
    // Simulate the browser sending two active contexts.
    testProxy.page.onUpdateExistingContextDetails([
      {contextId: 1, contextBackend: 'Test Backend'},
      {contextId: 2, contextBackend: 'Test Backend 2'},
    ]);
    await microtasksFinished();
    const gridContainer =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.grid-container');
    assertTrue(!!gridContainer);
    // There should be two entries in the list.
    assertEquals(2, gridContainer.children.length);
    const contexts = contextsTab.shadowRoot.querySelectorAll('.context-detail');
    // 2 contexts, 4 labels
    assertEquals(4, contexts.length);
    assertEquals('Context ID: 1', contexts[0]!.textContent.trim());
    assertEquals(
        'Runtime Backend: Test Backend', contexts[1]!.textContent.trim());
    assertEquals('Context ID: 2', contexts[2]!.textContent.trim());
    assertEquals(
        'Runtime Backend: Test Backend 2', contexts[3]!.textContent.trim());
    const noContextText =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.no-context');
    // The no contexts label should not exist.
    assertFalse(!!noContextText);
  });

  test('RemoveActiveContexts', async function() {
    // Simulate the browser removing all contexts.
    testProxy.page.onUpdateExistingContextDetails([]);
    await microtasksFinished();
    const gridContainer =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.grid-container');
    assertFalse(!!gridContainer);
    const noContextText =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.no-context');
    assertTrue(!!noContextText);
  });

  // <if expr="not webnn_enable_graph_dump">
  test('GraphRecordingNotSupported', function() {
    const graphDumpNoSupportedLabel = app.shadowRoot.querySelector('.text');
    assertTrue(!!graphDumpNoSupportedLabel);
  });
  // </if>

  // <if expr="webnn_enable_graph_dump">
  test('TestGraphRecording', async function() {
    const graphTab = app.shadowRoot.querySelector('webnn-internals-graph-dump');
    assertTrue(!!graphTab);
    const toggle = graphTab.shadowRoot.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertFalse(toggle.checked);

    testProxy.page.onGraphRecordEnabledChanged(true);
    await microtasksFinished();
    assertTrue(toggle.checked);
  });
  // </if>
});
