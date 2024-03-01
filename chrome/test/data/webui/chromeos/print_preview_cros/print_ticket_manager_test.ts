// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/print_ticket_manager.js';

import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {setPrintPreviewPageHandlerForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('PrintTicketManager', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let mockTimer: MockTimer;

  setup(() => {
    PrintTicketManager.resetInstanceForTesting();

    // Setup fakes for testing.
    mockTimer = new MockTimer();
    mockTimer.install();
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    setPrintPreviewPageHandlerForTesting(printPreviewPageHandler);
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('is a singleton', () => {
    const instance1 = PrintTicketManager.getInstance();
    const instance2 = PrintTicketManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = PrintTicketManager.getInstance();
    PrintTicketManager.resetInstanceForTesting();
    const instance2 = PrintTicketManager.getInstance();
    assertTrue(instance1 !== instance2);
  });

  // Verify PrintPreviewPageHandler called when sentPrintRequest triggered.
  test('sendPrintRequest calls PrintPreviewPageHandler.print', () => {
    const instance = PrintTicketManager.getInstance();
    assertEquals(0, printPreviewPageHandler.getCallCount('print'));
    instance.sendPrintRequest();
    assertEquals(1, printPreviewPageHandler.getCallCount('print'));
  });

  // Verify PrintPreviewPageHandler called when cancelPrintRequest triggered.
  test('sendPrintRequest calls PrintPreviewPageHandler.print', () => {
    const instance = PrintTicketManager.getInstance();
    const method = 'cancel';
    assertEquals(0, printPreviewPageHandler.getCallCount(method));
    instance.cancelPrintRequest();
    assertEquals(1, printPreviewPageHandler.getCallCount(method));
  });

  // Verify PRINT_REQUEST_STARTED_EVENT is dispatched when sentPrintRequest is
  // called and PRINT_REQUEST_FINISHED_EVENT is called when
  // PrintPreviewPageHandler.print resolves.
  test(
      'PRINT_REQUEST_STARTED_EVENT and PRINT_REQUEST_FINISHED_EVENT are ' +
          ' invoked when sentPrintRequest called',
      async () => {
        const delay = 1;
        printPreviewPageHandler.useTestDelay(delay);
        const instance = PrintTicketManager.getInstance();
        let startCount = 0;
        instance.addEventListener(PRINT_REQUEST_STARTED_EVENT, () => {
          ++startCount;
        });
        let finishCount = 0;
        instance.addEventListener(PRINT_REQUEST_FINISHED_EVENT, () => {
          ++finishCount;
        });
        const startEvent =
            eventToPromise(PRINT_REQUEST_STARTED_EVENT, instance);
        const finishEvent =
            eventToPromise(PRINT_REQUEST_FINISHED_EVENT, instance);

        assertEquals(0, startCount, 'Start should have zero calls');
        assertEquals(0, finishCount, 'Finish should have zero calls');

        instance.sendPrintRequest();

        await startEvent;

        assertEquals(1, startCount, 'Start should have one call');
        assertEquals(0, finishCount, 'Finish should have zero calls');

        // Advance time by test delay to trigger method resolver.
        mockTimer.tick(delay);
        await finishEvent;

        assertEquals(1, startCount, 'Start should have one call');
        assertEquals(1, finishCount, 'Finish should have one call');
      });

  // Verify isPrintRequestInProgress is false until sentPrintRequest is called
  // and returns to false when PrintPreviewPageHandler.print resolves.
  test(
      'isPrintRequestInProgress updates based on sendPrintRequest progress',
      async () => {
        const delay = 1;
        printPreviewPageHandler.useTestDelay(delay);
        const instance = PrintTicketManager.getInstance();
        const startEvent =
            eventToPromise(PRINT_REQUEST_STARTED_EVENT, instance);
        const finishEvent =
            eventToPromise(PRINT_REQUEST_FINISHED_EVENT, instance);

        assertFalse(instance.isPrintRequestInProgress(), 'Request not started');

        instance.sendPrintRequest();

        await startEvent;

        assertTrue(instance.isPrintRequestInProgress(), 'Request started');

        mockTimer.tick(delay);
        await finishEvent;

        assertFalse(instance.isPrintRequestInProgress(), 'Request finished');
      });

  // Verify PrintTicketManger ensures that PrintPreviewPageHandler.print is only
  // called if print request is not in progress.
  test('ensure only one print request triggered at a time', async () => {
    const delay = 1;
    printPreviewPageHandler.useTestDelay(delay);
    const instance = PrintTicketManager.getInstance();
    const startEvent = eventToPromise(PRINT_REQUEST_STARTED_EVENT, instance);
    const finishEvent = eventToPromise(PRINT_REQUEST_FINISHED_EVENT, instance);

    const method = 'print';
    let expectedCallCount = 0;
    assertEquals(
        expectedCallCount, printPreviewPageHandler.getCallCount(method),
        'No request sent');
    assertFalse(instance.isPrintRequestInProgress(), 'Request not started');

    instance.sendPrintRequest();

    // After calling `sendPrintRequest` the call count should increment.
    ++expectedCallCount;
    await startEvent;

    assertEquals(
        expectedCallCount, printPreviewPageHandler.getCallCount(method),
        'One request sent');
    assertTrue(instance.isPrintRequestInProgress(), 'Request started');

    // While request is in progress additional `sendPrintRequest` should not
    // call `PrintPreviewPageHandler.print`.
    instance.sendPrintRequest();
    instance.sendPrintRequest();
    instance.sendPrintRequest();
    assertEquals(
        expectedCallCount, printPreviewPageHandler.getCallCount(method),
        'One request sent');

    mockTimer.tick(delay);
    await finishEvent;

    assertEquals(
        expectedCallCount, printPreviewPageHandler.getCallCount(method),
        'One request sent');
    assertFalse(instance.isPrintRequestInProgress(), 'Request finished');
  });
});
