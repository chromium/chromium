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
      {
        contextId: 1,
        contextBackend: 'Test Backend',
        executionProviders: [
          {
            name: 'Test EP 1',
            vendor: 'Vendor 1',
            hardwareType: 'GPU',
            version: '1.0',
            firstSelected: true,
          },
          {
            name: 'Test EP 2',
            vendor: 'Vendor 2',
            hardwareType: 'CPU',
            version: '',
            firstSelected: false,
          },
        ],
      },
      {contextId: 2, contextBackend: 'Test Backend 2', executionProviders: []},
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
    const epTitles = contextsTab.shadowRoot.querySelectorAll('.ep-title');
    // Only the first context has EPs, and it has 2 EPs. If the context has no
    // EPs, the EP title should not be rendered.
    assertEquals(1, epTitles.length);
    const epDetailsList = contextsTab.shadowRoot.querySelectorAll('.ep');
    // The first context has 2 EPs, the second context has no EP, total 2 EPs.
    assertEquals(2, epDetailsList.length);
    // First EP has 4 details (name, vendor, hardware type, version).
    let epDetails = epDetailsList[0]!.querySelectorAll('.ep-detail');
    assertEquals(4, epDetails.length);
    assertEquals('Name: Test EP 1', epDetails[0]!.textContent.trim());
    assertEquals('Vendor: Vendor 1', epDetails[1]!.textContent.trim());
    assertEquals('Hardware Type: GPU', epDetails[2]!.textContent.trim());
    assertEquals('Version: 1.0', epDetails[3]!.textContent.trim());
    // The first EP is marked as first selected, it should have the
    // 'first-selected' class.
    assertTrue(epDetailsList[0]!.classList.contains('first-selected'));
    // Second EP has 3 details, version may be empty so let's make sure
    // we don't render the label.
    epDetails = epDetailsList[1]!.querySelectorAll('.ep-detail');
    assertEquals(3, epDetails.length);
    assertEquals('Name: Test EP 2', epDetails[0]!.textContent.trim());
    assertEquals('Vendor: Vendor 2', epDetails[1]!.textContent.trim());
    assertEquals('Hardware Type: CPU', epDetails[2]!.textContent.trim());
    assertFalse(epDetailsList[1]!.classList.contains('first-selected'));
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
