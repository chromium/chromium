// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/print_preview_cros_app_controller.js';

import {CAPABILITIES_MANAGER_SESSION_INITIALIZED, CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {DESTINATION_MANAGER_SESSION_INITIALIZED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED, PreviewTicketManager} from 'chrome://os-print/js/data/preview_ticket_manager.js';
import type {PrintPreviewPageHandlerComposite} from 'chrome://os-print/js/data/print_preview_page_handler_composite.js';
import {PRINT_TICKET_MANAGER_SESSION_INITIALIZED, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import type {FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {DIALOG_ARG_PROPERTY_KEY, PrintPreviewCrosAppController} from 'chrome://os-print/js/print_preview_cros_app_controller.js';
import {getPrintPreviewPageHandler} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController, type MockMethod} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {resetDataManagersAndProviders} from './test_utils.js';

suite('PrintPreviewCrosAppController', () => {
  let controller: PrintPreviewCrosAppController;
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let mockController: MockController;
  let mockTimer: MockTimer;
  let getVariableValueFn: MockMethod;

  const testDelay = 1;
  const dialogArgs = 'fake-token';

  setup(() => {
    mockController = new MockController();
    // Mock chrome function for looking up dialog args.
    getVariableValueFn =
        mockController.createFunctionMock(chrome, 'getVariableValue');
    getVariableValueFn.returnValue = dialogArgs;
    getVariableValueFn.addExpectation(DIALOG_ARG_PROPERTY_KEY);

    mockTimer = new MockTimer();
    mockTimer.install();

    resetDataManagersAndProviders();
    printPreviewPageHandler =
        (getPrintPreviewPageHandler() as PrintPreviewPageHandlerComposite)
            .fakePageHandler;

    controller = new PrintPreviewCrosAppController();
  });

  teardown(() => {
    printPreviewPageHandler.reset();
    resetDataManagersAndProviders();
    mockTimer.uninstall();
    mockController.reset();
  });

  // Verify controller is an event target.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });

  // Verify getVariableValue is called.
  test('controller retrieves token from dialog arguments', () => {
    mockController.verifyMocks();
    assertEquals(dialogArgs, controller.getDialogArgsForTesting());
  });

  // Verify `PrintPreviewPageHandler.startSession` is called when controller is
  // created.
  test('triggers PrintPreviewPageHandlerComposite startSession', async () => {
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
    assertEquals(
        dialogArgs, printPreviewPageHandler.dialogArgs,
        'Start session called with dialog args');
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

  // Verify preview ticket manager is initialized after start session resolves.
  test(
      'on resolve of startSession calls PreviewTicketManager.initializeSession',
      async () => {
        printPreviewPageHandler.setTestDelay(testDelay);

        const controller = new PrintPreviewCrosAppController();
        assertTrue(!!controller, 'Unable to create controller');
        const previewTicketManager = PreviewTicketManager.getInstance();
        assertFalse(
            previewTicketManager.isSessionInitialized(),
            'Before initializeSession PreviewTicketManager instance should ' +
                'not be initialized');

        // Move timer forward to resolve startSession.
        mockTimer.tick(testDelay);
        await eventToPromise(
            PREVIEW_TICKET_MANAGER_SESSION_INITIALIZED, previewTicketManager);

        assertTrue(
            previewTicketManager.isSessionInitialized(),
            'After initializeSession PreviewTicketManager instance should be ' +
                'initialized');
      });

  // Verify capabilities manager is initialized after start session resolves.
  test(
      'on resolve of startSession calls CapabilitiesManager.initializeSession',
      async () => {
        printPreviewPageHandler.setTestDelay(testDelay);

        const controller = new PrintPreviewCrosAppController();
        assertTrue(!!controller, 'Unable to create controller');
        const capabilitiesManager = CapabilitiesManager.getInstance();
        assertFalse(
            capabilitiesManager.isSessionInitialized(),
            'Before initializeSession CapabilitiesManager instance should ' +
                'not be initialized');

        // Move timer forward to resolve startSession.
        mockTimer.tick(testDelay);
        await eventToPromise(
            CAPABILITIES_MANAGER_SESSION_INITIALIZED, capabilitiesManager);

        assertTrue(
            capabilitiesManager.isSessionInitialized(),
            'After initializeSession CapabilitiesManager instance should be ' +
                'initialized');
      });
});
