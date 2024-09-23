// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {resetDataManagersAndProviders} from './test_utils.js';

suite('SummaryPanelController', () => {
  let controller: SummaryPanelController|null = null;
  let mockController: MockController;
  let capabilitiesManager: CapabilitiesManager;
  let previewTicketManager: PreviewTicketManager;
  let printTicketManager: PrintTicketManager;
  let eventTracker: EventTracker;

  setup(() => {
    mockController = new MockController();
    eventTracker = new EventTracker();

    resetDataManagersAndProviders();
    // Setup fakes.
    capabilitiesManager = CapabilitiesManager.getInstance();
    previewTicketManager = PreviewTicketManager.getInstance();
    printTicketManager = PrintTicketManager.getInstance();

    controller = new SummaryPanelController(eventTracker);
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
    eventTracker.removeAll();
    resetDataManagersAndProviders();
    controller = null;
  });

  // Verify getSheetsUsedText returns empty string when sheet count equal to
  // zero.
  test('when zero sheets used then return an empty string', () => {
    assert(controller);
    controller.setSheetsUsedForTesting(0);
    assertEquals('', controller.getSheetsUsedText());
  });

  // Verify getSheetsUsedText returns expected string when sheet count is
  // greater than zero.
  test('when zero sheets used then return an empty string', () => {
    assert(controller);
    controller.setSheetsUsedForTesting(1);
    assertEquals(`1 used`, controller.getSheetsUsedText());
    controller.setSheetsUsedForTesting(2);
    assertEquals(`2 used`, controller.getSheetsUsedText());
  });

  // Verify startPrintRequest calls PrintTicketManager.
  test(
      'calls PrintTicketManager.sendPrintRequest from handlePrintClicked',
      () => {
        const manager = PrintTicketManager.getInstance();
        const sendPrintRequestFn =
            mockController.createFunctionMock(manager, 'sendPrintRequest');
        sendPrintRequestFn.addExpectation();
        controller!.handlePrintClicked();
        mockController.verifyMocks();
      });

  // Verify cancelPrintRequest calls PrintTicketManager.
  test(
      'calls PrintTicketManager.cancelPrintRequest from handleCancelClicked',
      () => {
        const manager = PrintTicketManager.getInstance();
        const cancelPrintRequestFn =
            mockController.createFunctionMock(manager, 'cancelPrintRequest');
        cancelPrintRequestFn.addExpectation();
        controller!.handleCancelClicked();
        mockController.verifyMocks();
      });

  // Verify handler called when PRINT_REQUEST_STARTED_EVENT triggered.
  test('PRINT_REQUEST_STARTED_EVENT calls dispatch event', async () => {
    assert(controller);
    const handlerFn = mockController.createFunctionMock(
        controller, 'dispatchPrintButtonDisabledChangedEvent');
    const startRequest =
        eventToPromise(PRINT_REQUEST_STARTED_EVENT, printTicketManager);
    handlerFn.addExpectation();

    printTicketManager.dispatchEvent(
        createCustomEvent(PRINT_REQUEST_STARTED_EVENT));
    await startRequest;

    mockController.verifyMocks();
  });

  // Verify handler called when PRINT_REQUEST_FINISHED_EVENT triggered.
  test('PRINT_REQUEST_FINISHED_EVENT calls dispatch event', async () => {
    assert(controller);
    const handlerFn = mockController.createFunctionMock(
        controller, 'dispatchPrintButtonDisabledChangedEvent');
    const finishRequest =
        eventToPromise(PRINT_REQUEST_FINISHED_EVENT, printTicketManager);
    handlerFn.addExpectation();

    printTicketManager.dispatchEvent(
        createCustomEvent(PRINT_REQUEST_FINISHED_EVENT));
    await finishRequest;

    mockController.verifyMocks();
  });

  // Verify shouldDisablePrintButton is true when preview is loaded but print
  // request is in progress.
  test(
      'shouldDisablePrintButton true while print request is in progress',
      () => {
        // Set preview loaded to true since that can also disable the print
        // button.
        const previewRequestInProgressFn = mockController.createFunctionMock(
            previewTicketManager, 'isPreviewLoaded');
        previewRequestInProgressFn.returnValue = true;

        const printRequestInProgressFn = mockController.createFunctionMock(
            printTicketManager, 'isPrintRequestInProgress');
        printRequestInProgressFn.returnValue = true;
        assertTrue(controller!.shouldDisablePrintButton());
      });

  // Verify shouldDisablePrintButton is false when preview is loaded and print
  // request is not in progress.
  test(
      'shouldDisablePrintButton false while print request is not in progress',
      () => {
        // Set preview loaded to true since that can also disable the print
        // button.
        const previewRequestInProgressFn = mockController.createFunctionMock(
            previewTicketManager, 'isPreviewLoaded');
        previewRequestInProgressFn.returnValue = true;

        // Set capabilities to loaded since that can also disable the print
        // button.
        const activeDestinationCapsLoadedFn = mockController.createFunctionMock(
            capabilitiesManager, 'areActiveDestinationCapabilitiesLoaded');
        activeDestinationCapsLoadedFn.returnValue = true;

        const printRequestInProgressFn = mockController.createFunctionMock(
            printTicketManager, 'isPrintRequestInProgress');
        printRequestInProgressFn.returnValue = false;
        assertFalse(controller!.shouldDisablePrintButton());
      });

  // Verify shouldDisablePrintButton is true when preview isn't loaded.
  test('shouldDisablePrintButton true while preview is not loaded', () => {
    // Set capabilities to loaded since that can also disable the print
    // button.
    const activeDestinationCapsLoadedFn = mockController.createFunctionMock(
        capabilitiesManager, 'areActiveDestinationCapabilitiesLoaded');
    activeDestinationCapsLoadedFn.returnValue = true;

    const previewRequestInProgressFn = mockController.createFunctionMock(
        previewTicketManager, 'isPreviewLoaded');
    previewRequestInProgressFn.returnValue = false;
    assertTrue(controller!.shouldDisablePrintButton());
  });

  // Verify shouldDisablePrintButton is true when capabilities aren't loaded.
  test(
      'shouldDisablePrintButton true while capabilities are not loaded', () => {
        // Set preview loaded to true since that can also disable the print
        // button.
        const previewRequestInProgressFn = mockController.createFunctionMock(
            previewTicketManager, 'isPreviewLoaded');
        previewRequestInProgressFn.returnValue = true;

        const activeDestinationCapsLoadedFn = mockController.createFunctionMock(
            capabilitiesManager, 'areActiveDestinationCapabilitiesLoaded');
        activeDestinationCapsLoadedFn.returnValue = false;
        assertTrue(controller!.shouldDisablePrintButton());
      });
});
