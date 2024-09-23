// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/print_ticket_manager.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationProviderComposite} from 'chrome://os-print/js/data/destination_provider_composite.js';
import type {PrintPreviewPageHandlerComposite} from 'chrome://os-print/js/data/print_preview_page_handler_composite.js';
import {PRINT_REQUEST_FINISHED_EVENT, PRINT_REQUEST_STARTED_EVENT, PRINT_TICKET_MANAGER_SESSION_INITIALIZED, PRINT_TICKET_MANAGER_TICKET_CHANGED, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {DEFAULT_PARTIAL_PRINT_TICKET} from 'chrome://os-print/js/data/ticket_constants.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL, type FakePrintPreviewPageHandler} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getDestinationProvider, getPrintPreviewPageHandler} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {PrinterStatusReason, PrintTicket} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders} from './test_utils.js';

suite('PrintTicketManager', () => {
  let printPreviewPageHandler: FakePrintPreviewPageHandler;
  let destinationProvider: FakeDestinationProvider;
  let mockTimer: MockTimer;
  let mockController: MockController;

  const partialTicket: Partial<PrintTicket> = {
    ...DEFAULT_PARTIAL_PRINT_TICKET,
    destinationId: '',
    previewModifiable: true,  // Default to HTML document.
    shouldPrintSelectionOnly: false,
    printerManuallySelected: false,
  };

  setup(() => {
    resetDataManagersAndProviders();

    // Setup fakes for testing.
    mockController = new MockController();
    mockTimer = new MockTimer();
    mockTimer.install();
    printPreviewPageHandler =
        (getPrintPreviewPageHandler() as PrintPreviewPageHandlerComposite)
            .fakePageHandler;
    destinationProvider =
        (getDestinationProvider() as DestinationProviderComposite)
            .fakeDestinationProvider;
  });

  teardown(() => {
    mockController.reset();
    mockTimer.uninstall();
    resetDataManagersAndProviders();
  });

  async function waitForInitialDestinationReady(): Promise<void> {
    const delay = 1;
    destinationProvider.setTestDelay(delay);
    // Wait for active destination set to have non-empty value in ticket.
    const destinationManager = DestinationManager.getInstance();
    const activeDestEvent = eventToPromise(
        DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
    destinationManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    mockTimer.tick(delay);
    await activeDestEvent;
    assert(destinationManager.getActiveDestination());
  }

  async function waitForPrintTicketManagerInitialized(): Promise<void> {
    // Ensure ticket manager is configured.
    const ticketManager = PrintTicketManager.getInstance();
    const initEvent =
        eventToPromise(PRINT_TICKET_MANAGER_SESSION_INITIALIZED, ticketManager);
    ticketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    await initEvent;
  }

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
    instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    instance.sendPrintRequest();
    assertEquals(1, printPreviewPageHandler.getCallCount('print'));
  });

  // Verify PrintPreviewPageHandler called when cancelPrintRequest triggered.
  test('sendPrintRequest calls PrintPreviewPageHandler.cancel', () => {
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
        printPreviewPageHandler.setTestDelay(delay);
        const instance = PrintTicketManager.getInstance();
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
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
        printPreviewPageHandler.setTestDelay(delay);
        const instance = PrintTicketManager.getInstance();
        const startEvent =
            eventToPromise(PRINT_REQUEST_STARTED_EVENT, instance);
        const finishEvent =
            eventToPromise(PRINT_REQUEST_FINISHED_EVENT, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        assertFalse(instance.isPrintRequestInProgress(), 'Request not started');

        instance.sendPrintRequest();

        await startEvent;

        assertTrue(instance.isPrintRequestInProgress(), 'Request started');

        mockTimer.tick(delay);
        await finishEvent;

        assertFalse(instance.isPrintRequestInProgress(), 'Request finished');
      });

  // Verify PrintTicketManager ensures that PrintPreviewPageHandler.print is
  // only called if print request is not in progress.
  test('ensure only one print request triggered at a time', async () => {
    const delay = 1;
    printPreviewPageHandler.setTestDelay(delay);
    const instance = PrintTicketManager.getInstance();
    instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
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

  // Verify `isSessionInitialized` returns true and triggers
  // PRINT_TICKET_MANAGER_SESSION_INITIALIZED event after `initializeSession`
  // called.
  test(
      'initializeSession updates isSessionInitialized and triggers ' +
          PRINT_TICKET_MANAGER_SESSION_INITIALIZED,
      async () => {
        const instance = PrintTicketManager.getInstance();
        assertFalse(
            instance.isSessionInitialized(),
            'Before initializeSession, instance should not be initialized');

        // Set initial context.
        const sessionInit =
            eventToPromise(PRINT_TICKET_MANAGER_SESSION_INITIALIZED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await sessionInit;

        assertTrue(
            instance.isSessionInitialized(),
            'After initializeSession, instance should be initialized');
      });

  // Verify print ticket created when session initialized using SessionContext.
  test(
      'initializeSession creates print ticket based on session context', () => {
        const instance = PrintTicketManager.getInstance();
        let expectedTicket: PrintTicket|null = null;
        assertEquals(expectedTicket, instance.getPrintTicket());

        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        expectedTicket = {
          ...partialTicket,
          printPreviewId:
              FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL.printPreviewToken,
          shouldPrintSelectionOnly:
              FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL.hasSelection,
        } as PrintTicket;
        const ticket = instance.getPrintTicket() as PrintTicket;
        assertDeepEquals(expectedTicket, ticket);
      });

  // Verify `sendPrintRequest` requires a valid print ticket.
  test(
      'sendPrintRequest returns early if the print ticket is not valid', () => {
        const delay = 1;
        printPreviewPageHandler.setTestDelay(delay);
        const instance = PrintTicketManager.getInstance();
        assertEquals(null, instance.getPrintTicket());

        // Attempt sending while ticket is null to verify print is not called.
        instance.sendPrintRequest();

        let expectedPrintCallCount = 0;
        assertEquals(
            expectedPrintCallCount,
            printPreviewPageHandler.getCallCount('print'));

        // Initialize session will setup the print ticket.
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        instance.sendPrintRequest();
        ++expectedPrintCallCount;

        assertEquals(
            expectedPrintCallCount,
            printPreviewPageHandler.getCallCount('print'),
            'Print request can be sent');
      });

  // Verify PrintTicket destination values set based on active destination from
  // destination manager.
  test(
      'PrintTicket destination set to DestinationManager active' +
          ' destination ID',
      () => {
        const ticketManager = PrintTicketManager.getInstance();
        const destinationManager = DestinationManager.getInstance();
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = PDF_DESTINATION;
        assertEquals(null, ticketManager.getPrintTicket());

        ticketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        const ticket = ticketManager.getPrintTicket();
        assertNotEquals(null, ticket, 'Ticket configured');
        assertEquals(
            PDF_DESTINATION.id, ticket!.destinationId,
            'destination set from DestinationManager active destination');
        assertEquals(
            PDF_DESTINATION.printerType, ticket!.printerType,
            'printerType set from DestinationManager active destination');
        assertFalse(ticket!.printerManuallySelected, 'not manually selected');
        assertEquals(
            PrinterStatusReason.UNKNOWN_REASON, ticket!.printerStatusReason,
            'printerStatusReason fallback to UNKNOWN_REASON if null');
      });

  // Verify PrintTicket destination set to empty string if no active
  // destination available.
  test(
      'PrintTicket destination set to empty string when DestinationManager' +
          ' active destination is null',
      () => {
        const ticketManager = PrintTicketManager.getInstance();
        const destinationManager = DestinationManager.getInstance();
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = null;
        assertEquals(null, ticketManager.getPrintTicket());

        ticketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        const ticket = ticketManager.getPrintTicket();
        assertNotEquals(null, ticket, 'Ticket configured');
        assertEquals('', ticket!.destinationId, 'destination should be empty');
      });

  // Verify default setting for previewModifiable is based on session context.
  test('PrintTicket previewModifiable is set from session context', () => {
    const ticketManager = PrintTicketManager.getInstance();
    const destinationManager = DestinationManager.getInstance();
    const getActiveDestinationFn = mockController.createFunctionMock(
        destinationManager, 'getActiveDestination');
    getActiveDestinationFn.returnValue = null;
    assertEquals(null, ticketManager.getPrintTicket());

    // Force isModifiable to false, isModifiable true is tested in prior
    // test.
    const sessionContextNotModifiable = {
      ...FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL,
      isModifiable: false,
    };
    ticketManager.initializeSession(sessionContextNotModifiable);

    const ticket = ticketManager.getPrintTicket();
    assertNotEquals(null, ticket, 'Ticket configured');
    assertEquals(
        sessionContextNotModifiable.isModifiable, ticket!.previewModifiable,
        'previewModifiable should match session context');
  });

  // Verify default setting for shouldPrintSelectionOnly is based on session
  // context.
  test(
      'PrintTicket shouldPrintSelectionOnly is set from session context',
      () => {
        const ticketManager = PrintTicketManager.getInstance();
        const destinationManager = DestinationManager.getInstance();
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = null;
        assertEquals(null, ticketManager.getPrintTicket());
        // Force hasSelection to false, hasSelection true is tested in prior
        // test.
        const sessionContextNoSelection = {
          ...FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL,
          hasSelection: false,
        };
        ticketManager.initializeSession(sessionContextNoSelection);

        const ticket = ticketManager.getPrintTicket();
        assertNotEquals(null, ticket, 'Ticket configured');
        assertEquals(
            sessionContextNoSelection.hasSelection,
            ticket!.shouldPrintSelectionOnly,
            'shouldPrintSelectionOnly should default to match session context');
      });

  // Verify PrintTicket destination values update on first active destination
  // change event if currently empty string and stops listening to active
  // destination events after change.
  test(
      'PrintTicket listens to active destination change until ' +
          'print ticket destination set and updates ticket',
      async () => {
        const ticketManager = PrintTicketManager.getInstance();
        const destinationManager = DestinationManager.getInstance();
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = null;
        assertEquals(null, ticketManager.getPrintTicket());
        ticketManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        const ticket = ticketManager.getPrintTicket();
        assertEquals('', ticket!.destinationId, 'destination should be empty');

        getActiveDestinationFn.returnValue = PDF_DESTINATION;
        const changeEvent1 = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        await changeEvent1;

        assertEquals(
            PDF_DESTINATION.id, ticket!.destinationId,
            `destination should be ${PDF_DESTINATION.id}`);
        assertEquals(
            PDF_DESTINATION.printerType, ticket!.printerType,
            `printerType should be ${PDF_DESTINATION.printerType}`);
        assertFalse(ticket!.printerManuallySelected, 'not manually selected');
        assertEquals(
            PrinterStatusReason.UNKNOWN_REASON, ticket!.printerStatusReason,
            `printerStatusReason should fall back to UNKNOWN_REASON when null`);
        getActiveDestinationFn.returnValue = {
          id: 'fake_id',
          displayName: 'Fake Destination',
        };
        const changeEvent2 = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        await changeEvent2;
        assertEquals(
            PDF_DESTINATION.id, ticket!.destinationId,
            `destination should remain ${PDF_DESTINATION.id}`);
      });

  // Verify print ticket created uses default partial ticket for initialization
  // of settings not configured by session context.
  test(
      'initializeSession creates print ticket using default value for' +
          ' settings not configured by session context',
      () => {
        const instance = PrintTicketManager.getInstance();
        let expectedTicket: PrintTicket|null = null;
        assertEquals(expectedTicket, instance.getPrintTicket());

        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);

        expectedTicket = {
          ...partialTicket,
          printPreviewId:
              FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL.printPreviewToken,
          shouldPrintSelectionOnly:
              FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL.hasSelection,
        } as PrintTicket;
        const ticket = instance.getPrintTicket() as PrintTicket;
        assertEquals(
            undefined, ticket.advancedSettings,
            'Ticket advancedSettings optional property should not be set');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.borderless, ticket.borderless,
            'Ticket borderless should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.collate, ticket.collate,
            'Ticket collate should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.color, ticket.color,
            'Ticket color should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.copies, ticket.copies,
            'Ticket copies should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.dpiHorizontal, ticket.dpiHorizontal,
            'Ticket dpiHorizontal should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.dpiVertical, ticket.dpiVertical,
            'Ticket dpiVertical should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.dpiDefault, ticket.dpiDefault,
            'Ticket dpiDefault should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.duplex, ticket.duplex,
            'Ticket duplex should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.headerFooterEnabled,
            ticket.headerFooterEnabled,
            'Ticket landscape should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.landscape, ticket.landscape,
            'Ticket landscape should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.marginsType, ticket.marginsType,
            'Ticket marginsType should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertDeepEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.marginsCustom, ticket.marginsCustom,
            'Ticket marginsCustom should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            undefined, ticket.marginsCustom,
            'Ticket marginsCustom optional property should not be set');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.mediaSize, ticket.mediaSize,
            'Ticket mediaSize should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.mediaType, ticket.mediaType,
            'Ticket mediaType should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.pageCount, ticket.pageCount,
            'Ticket pageCount should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.pagesPerSheet, ticket.pagesPerSheet,
            'Ticket pagesPerSheet should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.pageHeight, ticket.pageHeight,
            'Ticket pageHeight should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.pageWidth, ticket.pageWidth,
            'Ticket pageWidth should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.pinValue, ticket.pinValue,
            'Ticket pinValue should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            undefined, ticket.pinValue,
            'Ticket pinValue optional property should not be set');
        assertEquals(
            PrinterStatusReason.UNKNOWN_REASON, ticket.printerStatusReason,
            'Ticket printerStatusReason should match ' +
                'DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            PrinterStatusReason.UNKNOWN_REASON, ticket.printerStatusReason,
            'Ticket printerStatusReason should be ' +
                'PrinterStatusReason.UNKNOWN_REASON');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.printerType, ticket.printerType,
            'Ticket printerType should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertFalse(
            ticket.printerManuallySelected,
            'Ticket printerManuallySelected not manually selected');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.rasterizePDF, ticket.rasterizePDF,
            'Ticket rasterizePDF should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.scaleFactor, ticket.scaleFactor,
            'Ticket scaleFactor should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.scalingType, ticket.scalingType,
            'Ticket scalingType should match DEFAULT_PARTIAL_PRINT_TICKET');
        assertEquals(
            DEFAULT_PARTIAL_PRINT_TICKET.shouldPrintBackgrounds,
            ticket.shouldPrintBackgrounds,
            'Ticket shouldPrintBackgrounds should match ' +
                'DEFAULT_PARTIAL_PRINT_TICKET');
      });

  // Verify setPrintTicketDestination validates the provided ID, updates
  // the ticket, and dispatches an event.
  test(
      'setPrintTicketDestination validates destination id before ' +
          'updating ticket and triggering event',
      async () => {
        await waitForInitialDestinationReady();
        await waitForPrintTicketManagerInitialized();
        const testDestination = createTestDestination();
        const destinationManager = DestinationManager.getInstance();
        destinationManager.setDestinationForTesting(testDestination);
        const ticketManager = PrintTicketManager.getInstance();

        // Simulate request to invalid destinations returns false.
        assertFalse(
            ticketManager.setPrintTicketDestination(PDF_DESTINATION.id));
        assertFalse(
            ticketManager.setPrintTicketDestination('unknownDestinationId'));

        // Simulate request to valid destination returns true.
        const ticketChanged =
            eventToPromise(PRINT_TICKET_MANAGER_TICKET_CHANGED, ticketManager);
        assertTrue(ticketManager.setPrintTicketDestination(testDestination.id));

        // Verify event triggered and expected fields updated.
        await ticketChanged;
        const ticket = ticketManager.getPrintTicket();
        assertEquals(
            testDestination.id, ticket!.destinationId,
            'ticket destination should be updated');
        assertEquals(
            testDestination.printerType, ticket!.printerType,
            'ticket printerType should be updated');
        assertEquals(
            testDestination.printerStatusReason, ticket!.printerStatusReason,
            'ticket printerStatusReason should be updated');
      });
});
