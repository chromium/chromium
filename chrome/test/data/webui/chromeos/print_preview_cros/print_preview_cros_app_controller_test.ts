// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/print_preview_cros_app_controller.js';

import {DESTINATION_MANAGER_SESSION_INITIALIZED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {PRINT_TICKET_MANAGER_SESSION_INITIALIZED, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {PrintPreviewCrosAppController} from 'chrome://os-print/js/print_preview_cros_app_controller.js';
import {setPrintPreviewPageHandlerForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('PrintPreviewCrosAppController', () => {
  let controller: PrintPreviewCrosAppController;
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let mockTimer: MockTimer;

  const testDelay = 1;

  setup(() => {
    mockTimer = new MockTimer();
    mockTimer.install();

    DestinationManager.resetInstanceForTesting();
    PrintTicketManager.resetInstanceForTesting();
    printPreviewPageHandler = new FakePrintPreviewPageHandler();
    setPrintPreviewPageHandlerForTesting(printPreviewPageHandler);

    controller = new PrintPreviewCrosAppController();
  });

  teardown(() => {
    printPreviewPageHandler.reset();
    DestinationManager.resetInstanceForTesting();
    PrintTicketManager.resetInstanceForTesting();
    mockTimer.uninstall();
  });

  // Verify controller is an event target.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });

  // Verify `PrintPreviewPageHandler.startSession` is called when controller is
  // created.
  test('triggers PrintPreviewPageHandler startSession', async () => {
    // Reset call counts before creating controller.
    printPreviewPageHandler.reset();

    const method = 'startSession';
    let expectedCalls = 0;
    assertEquals(
        expectedCalls, printPreviewPageHandler.getCallCount(method),
        `No calls to start session`);

    // Controller will connect to PrintPreviewPageHandler and trigger
    // `startSession`.
    controller = new PrintPreviewCrosAppController();
    assertTrue(!!controller);
    ++expectedCalls;

    assertEquals(
        expectedCalls, printPreviewPageHandler.getCallCount(method),
        `Start session should be called once`);
  });

  // Verify destination manager is initialized after start session resolves.
  test(
      'when startSession resolves the destination manager is initialized',
      async () => {
        const delay = 1;
        printPreviewPageHandler.setTestDelay(delay);
        const controller = new PrintPreviewCrosAppController();
        assertTrue(!!controller, 'Unable to create controller');
        const destinationManager = DestinationManager.getInstance();
        assertFalse(
            destinationManager.isSessionInitialized(),
            'Before initializeSession destination manager instance should ' +
                'not be initialized');

        // Move timer forward to resolve startSession.
        mockTimer.tick(testDelay);
        await eventToPromise(
            DESTINATION_MANAGER_SESSION_INITIALIZED, destinationManager);

        assertTrue(
            destinationManager.isSessionInitialized(),
            'After initializeSession destination manager instance should be ' +
                'initialized');
      });

  // Verify print ticket manager is initialized after start session resolves.
  test(
      'on resolve of startSession calls PrintTicketManager.initializeSession',
      async () => {
        printPreviewPageHandler.setTestDelay(testDelay);

        const controller = new PrintPreviewCrosAppController();
        assertTrue(!!controller, 'Unable to create controller');
        const printTicketManager = PrintTicketManager.getInstance();
        assertFalse(
            printTicketManager.isSessionInitialized(),
            'Before initializeSession PrintTicketManager instance should ' +
                'not be initialized');

        // Move timer forward to resolve startSession.
        mockTimer.tick(testDelay);
        await eventToPromise(
            PRINT_TICKET_MANAGER_SESSION_INITIALIZED, printTicketManager);

        assertTrue(
            printTicketManager.isSessionInitialized(),
            'After initializeSession PrintTicketManager instance should be ' +
                'initialized');
      });
});
