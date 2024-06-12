// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import type {PrintPreviewPageHandlerComposite} from 'chrome://os-print/js/data/print_preview_page_handler_composite.js';
import {PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL, type FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {SummaryPanelElement} from 'chrome://os-print/js/summary_panel.js';
import {PRINT_BUTTON_DISABLED_CHANGED_EVENT, SHEETS_USED_CHANGED_EVENT, SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getPrintPreviewPageHandler} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {resetDataManagersAndProviders} from './test_utils.js';

suite('SummaryPanel', () => {
  const sheetsUsedSelector = '#sheetsUsed';
  const printButtonSelector = '#print';
  const cancelButtonSelector = '#cancel';

  let element: SummaryPanelElement|null = null;
  let controller: SummaryPanelController|null = null;
  let mockController: MockController|null = null;
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let mockTimer: MockTimer;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Configure mocks and fakes.
    mockController = new MockController();
    mockTimer = new MockTimer();
    mockTimer.install();
    resetDataManagersAndProviders();
    printPreviewPageHandler =
        (getPrintPreviewPageHandler() as PrintPreviewPageHandlerComposite)
            .fakePageHandler;
    element =
        document.createElement(SummaryPanelElement.is) as SummaryPanelElement;
    assertTrue(!!element);
    document.body.append(element);
    assert(element);
    controller = element.getControllerForTesting();
    assert(controller);
    await updateSheetsUsed(/*sheetsUsed=*/ 1);

    // CrOS components are async and require flushTasks before they are
    // available.
    flush();
  });

  teardown(() => {
    mockTimer.uninstall();
    if (element) {
      element.remove();
    }
    element = null;
    controller = null;
    mockController?.reset();
    mockController = null;
    resetDataManagersAndProviders();
  });

  // Sets sheets used in controller and wait for UI to update.
  async function updateSheetsUsed(sheetsUsed: number): Promise<void> {
    assert(controller);
    const sheetsUsedEvent =
        eventToPromise(SHEETS_USED_CHANGED_EVENT, controller);
    controller.setSheetsUsedForTesting(sheetsUsed);
    await sheetsUsedEvent;
    flush();
  }

  // Mock the controllers to enable the print button.
  function setPreviewAndCapabiliitesLoaded(): void {
    assert(mockController);
    const capabilitiesManager = CapabilitiesManager.getInstance();
    const capabilitiesLoadedFn = mockController.createFunctionMock(
        capabilitiesManager, 'areActiveDestinationCapabilitiesLoaded');
    capabilitiesLoadedFn.returnValue = true;
    capabilitiesLoadedFn.addExpectation();

    const previewTicketManager = PreviewTicketManager.getInstance();
    assert(mockController);
    const isPreviewLoadedFn = mockController.createFunctionMock(
        previewTicketManager, 'isPreviewLoaded');
    isPreviewLoadedFn.returnValue = true;
    isPreviewLoadedFn.addExpectation();
  }

  // Verify the summary-panel element can be rendered, contains print, cancel,
  // and sheets used elements.
  test('element renders', async () => {
    assert(element);
    assertTrue(isVisible(element));

    assertTrue(
        isChildVisible(element, cancelButtonSelector),
        `Should display ${cancelButtonSelector}`);
    assertTrue(
        isChildVisible(element, printButtonSelector),
        `Should display ${printButtonSelector}`);
    assertTrue(
        isChildVisible(element, sheetsUsedSelector),
        `Should display ${sheetsUsedSelector}`);
  });

  // Verify summary-panel element has a controller configured.
  test('has element controller', async () => {
    assertTrue(
        !!controller,
        `${SummaryPanelElement.is} should have controller configured`);
  });

  // Verify #sheetsUsed updates to the string defined by SummaryPanelController
  // when a `sheets_used_changed` event occurs.
  test('sheets used matches controller getSheetsUsed', async () => {
    assert(element);
    assert(controller);
    const sheetsUsed = strictQuery<HTMLSpanElement>(
        sheetsUsedSelector, element.shadowRoot, HTMLSpanElement);

    updateSheetsUsed(/*sheetsUsed=*/ 0);
    const expectedInitialText = '';
    assertEquals(
        expectedInitialText, sheetsUsed.innerText,
        `${sheetsUsedSelector} text should match ${expectedInitialText}`);
    assertEquals(
        expectedInitialText, controller.getSheetsUsedText(),
        `${SummaryPanelElement.is} controller text should match ${
            expectedInitialText}`);

    // Controller emits `sheets-used-changed`
    updateSheetsUsed(/*sheetsUsed=*/ 1);

    const expectedUpdatedText = '1 used';
    assertEquals(
        expectedUpdatedText, sheetsUsed.innerText,
        `${sheetsUsedSelector} text should match ${expectedUpdatedText}`);
    assertEquals(
        expectedUpdatedText, controller.getSheetsUsedText(),
        `${SummaryPanelElement.is} controller text should match ${
            expectedUpdatedText}`);
  });

  // Verify print button calls controller.handlePrintClicked functionality.
  test('click print triggers PrintPreviewPageHandler', async () => {
    const delay = 1;
    printPreviewPageHandler.setTestDelay(delay);

    assert(mockController);
    const handlePrintClickedMock =
        mockController.createFunctionMock(controller!, 'handlePrintClicked');
    handlePrintClickedMock.addExpectation();

    setPreviewAndCapabiliitesLoaded();
    assert(controller);
    controller.dispatchEvent(
        createCustomEvent(PRINT_BUTTON_DISABLED_CHANGED_EVENT));

    // Click print button.
    const printButton =
        strictQuery<Button>(printButtonSelector, element!.shadowRoot, Button);
    const printButtonEvent = eventToPromise('click', printButton);
    printButton.click();
    await printButtonEvent;

    // Verify controller is listening to click event.
    mockController.verifyMocks();
  });

  // Verify cancel button calls controller.handleCancelClicked functionality.
  test('click cancel triggers PrintPreviewPageHandler', async () => {
    assert(mockController);
    const handleCancelClickedMock =
        mockController.createFunctionMock(controller!, 'handleCancelClicked');
    handleCancelClickedMock.addExpectation();

    // Click print button.
    const cancelButton =
        strictQuery<Button>(cancelButtonSelector, element!.shadowRoot, Button);
    const cancelButtonEvent = eventToPromise('click', cancelButton);
    cancelButton.click();
    await cancelButtonEvent;

    // Verify controller is listening to click event.
    mockController.verifyMocks();
  });

  // Verify print button disabled when print button clicked and re-enabled when
  // PrintPreviewPageHandler.print resolves.
  test('Print button disabled while print request in progress', async () => {
    assert(controller);
    const printDisabledEvent1 =
        eventToPromise(PRINT_BUTTON_DISABLED_CHANGED_EVENT, controller);
    const delay = 10;
    printPreviewPageHandler.setTestDelay(delay);

    setPreviewAndCapabiliitesLoaded();
    assert(controller);
    controller.dispatchEvent(
        createCustomEvent(PRINT_BUTTON_DISABLED_CHANGED_EVENT));

    const printButton =
        strictQuery<Button>(printButtonSelector, element!.shadowRoot, Button);
    assertFalse(
        printButton.disabled, 'Print should be enabled before request sent');

    const printTicketManager = PrintTicketManager.getInstance();
    printTicketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    printButton.click();
    await printDisabledEvent1;

    assertTrue(
        printTicketManager.isPrintRequestInProgress(),
        'Print request in progress');
    assertTrue(
        printButton.disabled,
        'Print should be disabled while request in progress');

    const printDisabledEvent2 =
        eventToPromise(PRINT_BUTTON_DISABLED_CHANGED_EVENT, controller);
    mockTimer.tick(delay);
    await printDisabledEvent2;

    assertFalse(
        printTicketManager.isPrintRequestInProgress(),
        'Print request is complete');
    assertFalse(
        printButton.disabled,
        'Print should be enabled after request completes');
  });
});
