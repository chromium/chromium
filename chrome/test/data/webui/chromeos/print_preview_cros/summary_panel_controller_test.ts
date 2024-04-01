// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/summary_panel.js';

import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {SummaryPanelController} from 'chrome://os-print/js/summary_panel_controller.js';
import {setPrintPreviewPageHandlerForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SummaryPanelController', () => {
  let controller: SummaryPanelController|null = null;
  let mockController: MockController;
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let printTicketManger: PrintTicketManager;
  let eventTracker: EventTracker;

  setup(() => {
    mockController = new MockController();
    eventTracker = new EventTracker();

    PrintTicketManager.resetInstanceForTesting();
    // Setup fakes.
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    setPrintPreviewPageHandlerForTesting(printPreviewPageHandler);
    printTicketManger = PrintTicketManager.getInstance();

    controller = new SummaryPanelController(eventTracker);
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
    eventTracker.removeAll();
    PrintTicketManager.resetInstanceForTesting();
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
  test('PRINT_REQUEST_STARTED_EVENT calls onPrintRequestStarted', async () => {
    assert(controller);
    const handlerFn =
        mockController.createFunctionMock(controller, 'onPrintRequestStarted');
    const testEvent = new CustomEvent<void>(
        PRINT_REQUEST_STARTED_EVENT, {bubbles: true, composed: true});
    const startRequest =
        eventToPromise(PRINT_REQUEST_STARTED_EVENT, printTicketManger);
    handlerFn.addExpectation(testEvent);

    printTicketManger.dispatchEvent(testEvent);
    await startRequest;

    mockController.verifyMocks();
  });

  // Verify handler called when PRINT_REQUEST_FINISHED_EVENT triggered.
  test(
      'PRINT_REQUEST_FINISHED_EVENT calls onPrintRequestFinished', async () => {
        assert(controller);
        const handlerFn = mockController.createFunctionMock(
            controller, 'onPrintRequestFinished');
        const testEvent = new CustomEvent<void>(
            PRINT_REQUEST_FINISHED_EVENT, {bubbles: true, composed: true});
        const finishRequest =
            eventToPromise(PRINT_REQUEST_FINISHED_EVENT, printTicketManger);
        handlerFn.addExpectation(testEvent);

        printTicketManger.dispatchEvent(testEvent);
        await finishRequest;

        mockController.verifyMocks();
      });

  // Verify shouldDisablePrintButton is true when print request is in progress.
  test(
      'shouldDisablePrintButton true while print request is in progress',
      () => {
        const printRequestInProgressFn = mockController.createFunctionMock(
            printTicketManger, 'isPrintRequestInProgress');
        printRequestInProgressFn.returnValue = true;
        assertTrue(controller!.shouldDisablePrintButton());
      });

  // Verify shouldDisablePrintButton is false when print request is not in
  // progress.
  test(
      'shouldDisablePrintButton true while print request is in progress',
      () => {
        const printRequestInProgressFn = mockController.createFunctionMock(
            printTicketManger, 'isPrintRequestInProgress');
        printRequestInProgressFn.returnValue = false;
        assertFalse(controller!.shouldDisablePrintButton());
      });
});
