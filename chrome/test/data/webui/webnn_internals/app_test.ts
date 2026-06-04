// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webnn-internals/app.js';

import type {WebnnInternalsAppElement} from 'chrome://webnn-internals/app.js';
import type {WebnnInternalsContextsViewerElement} from 'chrome://webnn-internals/contexts_viewer.js';
import {browserProxyFactory} from 'chrome://webnn-internals/webnn_internals.mojom-webui.js';
import type {PageRemote} from 'chrome://webnn-internals/webnn_internals.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://webnn-internals/webnn_internals.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('WebnnInternalsUITest', function() {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let page: PageRemote;
  let app: WebnnInternalsAppElement;
  let contextsTab: WebnnInternalsContextsViewerElement;
  const testExecutionProviders = [
    {
      name: 'Test EP 1',
      vendor: 'Vendor 1',
      vendorId: '0x5678',
      deviceId: '0x1234',
      hardwareType: 'GPU',
      version: '1.0',
      firstSelected: true,
    },
    {
      name: 'Test EP 2',
      vendor: 'Vendor 2',
      vendorId: '0x1234',
      deviceId: '0x5678',
      version: '',
      hardwareType: 'CPU',
      firstSelected: false,
    },
  ];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    page = remote;

    handler.setResultFor('requestExistingContextsDetails', Promise.resolve({
      contextsInfo: [],
    }));
    // <if expr="webnn_enable_graph_dump">
    handler.setResultFor('isGraphRecording', Promise.resolve({
      enabled: false,
    }));
    // </if>
    // <if expr="is_win">
    handler.setResultFor(
        'forceOrtEnvironmentCreationForIntrospection', Promise.resolve({
          availableExecutionProviders: testExecutionProviders,
        }));
    // </if>
    handler.setResultFor(
        'requestAvailableExecutionProvidersDetails', Promise.resolve({
          availableExecutionProviders: testExecutionProviders,
        }));
    app = document.createElement('webnn-internals-app');
    document.body.appendChild(app);
    const viewer =
        app.shadowRoot.querySelector('webnn-internals-contexts-viewer');
    assertTrue(!!viewer);
    contextsTab = viewer;
    return microtasksFinished();
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
    page.onUpdateExistingContextDetails([
      {
        contextId: 1,
        contextBackend: 'Test Backend',
        executionProviders: testExecutionProviders,
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
    // First EP has 6 details (name, vendor, hardware vendor ID, hardware device
    // ID, hardware type, version).
    let epDetails = epDetailsList[0]!.querySelectorAll('.ep-detail');
    assertEquals(6, epDetails.length);
    assertEquals('Name: Test EP 1', epDetails[0]!.textContent.trim());
    assertEquals('EP Vendor: Vendor 1', epDetails[1]!.textContent.trim());
    assertEquals(
        'Hardware Vendor ID: 0x5678', epDetails[2]!.textContent.trim());
    assertEquals(
        'Hardware Device ID: 0x1234', epDetails[3]!.textContent.trim());
    assertEquals('Hardware Type: GPU', epDetails[4]!.textContent.trim());
    assertEquals('Version: 1.0', epDetails[5]!.textContent.trim());
    // The first EP is marked as first selected, it should have the
    // 'first-selected' class.
    assertTrue(epDetailsList[0]!.classList.contains('first-selected'));
    // Second EP has 5 details, version may be empty so let's make sure
    // we don't render the label.
    epDetails = epDetailsList[1]!.querySelectorAll('.ep-detail');
    assertEquals(5, epDetails.length);
    assertEquals('Name: Test EP 2', epDetails[0]!.textContent.trim());
    assertEquals('EP Vendor: Vendor 2', epDetails[1]!.textContent.trim());
    assertEquals(
        'Hardware Vendor ID: 0x1234', epDetails[2]!.textContent.trim());
    assertEquals(
        'Hardware Device ID: 0x5678', epDetails[3]!.textContent.trim());
    assertEquals('Hardware Type: CPU', epDetails[4]!.textContent.trim());
    assertFalse(epDetailsList[1]!.classList.contains('first-selected'));
    const noContextText =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.no-context');
    // The no contexts label should not exist.
    assertFalse(!!noContextText);
  });

  test('RemoveActiveContexts', async function() {
    // Simulate the browser removing all contexts.
    page.onUpdateExistingContextDetails([]);
    await microtasksFinished();
    const gridContainer =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.grid-container');
    assertFalse(!!gridContainer);
    const noContextText =
        contextsTab.shadowRoot.querySelector<HTMLElement>('.no-context');
    assertTrue(!!noContextText);
  });

  function assertEpItemContent(gridItem: Element, expectedValues: string[]) {
    const elements = gridItem.querySelectorAll('div');
    assertEquals(expectedValues.length, elements.length);

    expectedValues.forEach((expectedValue, index) => {
      assertEquals(expectedValue, elements[index]!.textContent.trim());
    });
  }

  test('AvailableExecutionProvidersDetails', function() {
    const infoTab = app.shadowRoot.querySelector('webnn-internals-info-page');
    assertTrue(!!infoTab);
    const gridContainer = infoTab.shadowRoot.querySelector<HTMLElement>(
        '.grid-container-available-eps');
    assertTrue(!!gridContainer);
    const epItems = gridContainer.querySelectorAll('.item');
    assertEquals(2, epItems.length);
    assertEpItemContent(epItems[0]!, [
      'Name:',
      'Test EP 1',
      'EP Vendor:',
      'Vendor 1',
      'Hardware Vendor ID:',
      '0x5678',
      'Hardware Device ID:',
      '0x1234',
      'Hardware Type:',
      'GPU',
      'Version:',
      '1.0',
    ]);
    // Test EP 2 has no version so it should not be rendered.
    assertEpItemContent(epItems[1]!, [
      'Name:',
      'Test EP 2',
      'EP Vendor:',
      'Vendor 2',
      'Hardware Vendor ID:',
      '0x1234',
      'Hardware Device ID:',
      '0x5678',
      'Hardware Type:',
      'CPU',
    ]);
  });

  test('UpdateAvailableExecutionProvidersDetails', async function() {
    // Simulate updating the available execution providers details.
    page.onUpdateAvailableExecutionProvidersDetails([
      {
        name: 'Test EP 3',
        vendor: 'Vendor 3',
        version: '1.3',
        vendorId: '0x1234',
        deviceId: '0x9abc',
        hardwareType: 'NPU',
        firstSelected: false,
      },
    ]);
    await microtasksFinished();
    const infoTab = app.shadowRoot.querySelector('webnn-internals-info-page');
    assertTrue(!!infoTab);
    const gridContainer = infoTab.shadowRoot.querySelector<HTMLElement>(
        '.grid-container-available-eps');
    assertTrue(!!gridContainer);
    const epItems = gridContainer.querySelectorAll('.item');
    assertEquals(1, epItems.length);
    assertEpItemContent(epItems[0]!, [
      'Name:',
      'Test EP 3',
      'EP Vendor:',
      'Vendor 3',
      'Hardware Vendor ID:',
      '0x1234',
      'Hardware Device ID:',
      '0x9abc',
      'Hardware Type:',
      'NPU',
      'Version:',
      '1.3',
    ]);
  });

  test('RemoveAllAvailableExecutionProvidersDetails', async function() {
    // Simulate that there are no EPs available.
    page.onUpdateAvailableExecutionProvidersDetails([]);
    await microtasksFinished();
    const infoTab = app.shadowRoot.querySelector('webnn-internals-info-page');
    assertTrue(!!infoTab);
    const gridContainer = infoTab.shadowRoot.querySelector<HTMLElement>(
        '.grid-container-available-eps');
    assertFalse(!!gridContainer);
    const noep = infoTab.shadowRoot.querySelectorAll('.noep');
    assertEquals(1, noep.length);
  });

  // <if expr="is_win">
  test('ForceOrtEnvironmentCreationForIntrospection', async function() {
    page.onUpdateAvailableExecutionProvidersDetails([]);
    await microtasksFinished();
    const infoTab = app.shadowRoot.querySelector('webnn-internals-info-page');
    assertTrue(!!infoTab);
    let gridContainer = infoTab.shadowRoot.querySelector<HTMLElement>(
        '.grid-container-available-eps');
    assertFalse(!!gridContainer);
    const forceButton = infoTab.shadowRoot.querySelector('cr-button');
    assertTrue(!!forceButton);
    forceButton.click();
    await microtasksFinished();
    gridContainer = infoTab.shadowRoot.querySelector<HTMLElement>(
        '.grid-container-available-eps');
    assertTrue(!!gridContainer);
  });
  // </if>

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

    page.onGraphRecordEnabledChanged(true);
    await microtasksFinished();
    assertTrue(toggle.checked);
  });
  // </if>
});
