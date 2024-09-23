// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown_controller.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_DESTINATIONS_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationProviderComposite} from 'chrome://os-print/js/data/destination_provider_composite.js';
import {PRINT_REQUEST_FINISHED_EVENT, PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED, DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getDestinationProvider} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders, waitForInitialDestinationSet, waitForPrintTicketManagerInitialized, waitForSendPrintRequestFinished} from './test_utils.js';

suite('DestinationDropdownController', () => {
  let controller: DestinationDropdownController;
  let destinationManager: DestinationManager;
  let fakeDestinationProvider: FakeDestinationProvider;
  let printTicketManager: PrintTicketManager;
  let mockController: MockController;
  let eventTracker: EventTracker;
  let mockTimer: MockTimer;

  const testDelay = 1;

  setup(() => {
    mockController = new MockController();
    eventTracker = new EventTracker();
    mockTimer = new MockTimer();
    mockTimer.install();

    resetDataManagersAndProviders();
    fakeDestinationProvider =
        (getDestinationProvider() as DestinationProviderComposite)
            .fakeDestinationProvider;
    fakeDestinationProvider.setTestDelay(testDelay);
    destinationManager = DestinationManager.getInstance();
    printTicketManager = PrintTicketManager.getInstance();

    controller = new DestinationDropdownController(eventTracker);
  });

  teardown(() => {
    resetDataManagersAndProviders();
    eventTracker.removeAll();
    mockTimer.uninstall();
    mockController.reset();
  });

  // Verify controller can be constructed.
  test('controller is an event target', () => {
    assertTrue(
        controller instanceof EventTarget, 'Controller is an event target');
  });

  // Verify controller is listening to
  // DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED event.
  test(
      'onDestinationManagerActiveDestinationChanged called on ' +
          DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED,
      async () => {
        const onStateChangedFn = mockController.createFunctionMock(
            controller, 'onDestinationManagerActiveDestinationChanged');
        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        onStateChangedFn.addExpectation();

        // Simulate event being fired.
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        await stateChanged;

        mockController.verifyMocks();
      });

  // Verify DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED notifies the UI to
  // update.
  test(
      `${DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED} triggers UI updates
      events`,
      async () => {
        let callCount = 0;
        let expectedCallCount = 0;
        controller.addEventListener(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, () => {
              ++callCount;
            });
        assertEquals(
            expectedCallCount, callCount,
            `${DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION} not emitted`);

        // Simulate event being fired.
        const activeChanged = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        ++expectedCallCount;
        await activeChanged;

        assertEquals(
            expectedCallCount, callCount,
            `${DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION} emitted`);
      });

  // Verify getDestinations returns expected list of destinations.
  test('getDestinations returns expected list of destinations', () => {
    const getDestinationsFn = mockController.createFunctionMock(
        destinationManager, 'getDestinations');
    const expectedDestinations = [createTestDestination()];
    getDestinationsFn.returnValue = expectedDestinations;

    assertDeepEquals(expectedDestinations, controller.getDestinations());
  });

  // Verify controller is listening to
  // DESTINATION_MANAGER_DESTINATIONS_CHANGED event.
  test(
      'onDestinationManagerDestinationsChanged called on ' +
          DESTINATION_MANAGER_DESTINATIONS_CHANGED,
      async () => {
        const onDestinationsChangedFn = mockController.createFunctionMock(
            controller, 'onDestinationManagerDestinationsChanged');
        const destinationsChanged = eventToPromise(
            DESTINATION_MANAGER_DESTINATIONS_CHANGED, destinationManager);
        onDestinationsChangedFn.addExpectation();

        // Simulate event being fired.
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_DESTINATIONS_CHANGED));
        await destinationsChanged;

        mockController.verifyMocks();
      });

  // Verify DESTINATION_MANAGER_DESTINATIONS_CHANGED notifies the UI to
  // update.
  test(
      `${DESTINATION_MANAGER_DESTINATIONS_CHANGED} triggers UI updated event`,
      async () => {
        let callCount = 0;
        let expectedCallCount = 0;
        controller.addEventListener(
            DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, () => {
              ++callCount;
            });
        assertEquals(
            expectedCallCount, callCount,
            `${DESTINATION_DROPDOWN_UPDATE_DESTINATIONS} not emitted`);

        // Simulate event being fired.
        const destinationsChanged = eventToPromise(
            DESTINATION_MANAGER_DESTINATIONS_CHANGED, destinationManager);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_DESTINATIONS_CHANGED));
        ++expectedCallCount;
        await destinationsChanged;

        assertEquals(
            expectedCallCount, callCount,
            `${DESTINATION_DROPDOWN_UPDATE_DESTINATIONS} emitted`);
      });

  // Verify updateActiveDestination calls PrintTicketManager
  // setPrintTicketDestination with provided destination ID.
  test(
      'updateActiveDestination calls setPrintTicketDestination with ' +
          'destination ID',
      async () => {
        await waitForInitialDestinationSet(mockTimer, testDelay);
        const testDestination = createTestDestination();
        destinationManager.setDestinationForTesting(testDestination);
        const updatePrintTicketFn = mockController.createFunctionMock(
            printTicketManager, 'setPrintTicketDestination');
        updatePrintTicketFn.returnValue = true;
        updatePrintTicketFn.addExpectation(testDestination.id);
        controller.updateActiveDestination(testDestination.id);
        updatePrintTicketFn.verifyMock();
      });

  // Verify updateActiveDestination returns false if ID provided is already
  // active or an invalid ID. Also does not call setPrintTicketDestination.
  test(`updateActiveDestination returns false`, async () => {
    await waitForInitialDestinationSet(mockTimer, testDelay);

    assertFalse(
        controller.updateActiveDestination('unknownDestinationId'),
        'Update fails for unknown ID');
    assertFalse(
        controller.updateActiveDestination(PDF_DESTINATION.id),
        'Update fails for current ID');
  });

  // Verify controller emits DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED
  // if disabled state should be re-evaluated, including:
  // - Destination manager has destinations loaded state.
  // - Print Request started or finished.
  test(
      'controller calls dispatchDropdownDisabled and emits event', async () => {
        const dispatchDropdownDisabledFn = mockController.createFunctionMock(
            controller, 'dispatchDropdownDisabled');
        // Dispatch called for destination manger twice for state changes.
        dispatchDropdownDisabledFn.addExpectation();
        dispatchDropdownDisabledFn.addExpectation();
        await waitForInitialDestinationSet(mockTimer, testDelay);

        // Dispatch called for print request started and print request finished.
        dispatchDropdownDisabledFn.addExpectation();
        dispatchDropdownDisabledFn.addExpectation();
        await waitForPrintTicketManagerInitialized();
        await waitForSendPrintRequestFinished(mockTimer, testDelay);

        // Dispatch should be called a total of 4 times.
        dispatchDropdownDisabledFn.verifyMock();

        // Reset mock and simulate request finished to verify emitted event.
        mockController.reset();
        const disabledChanged = eventToPromise(
            DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED, controller);
        printTicketManager.dispatchEvent(
            createCustomEvent(PRINT_REQUEST_FINISHED_EVENT));
        await disabledChanged;
      });

  // Verify shouldDisableDropdown returns true if print request is in progress.
  test(
      'shouldDisableSelect returns true if print request in progress',
      async () => {
        // Initialize manager and force print request to be in progress.
        await waitForInitialDestinationSet(mockTimer, testDelay);
        await waitForPrintTicketManagerInitialized();
        const inProgressFn = mockController.createFunctionMock(
            printTicketManager, 'isPrintRequestInProgress');
        inProgressFn.returnValue = true;
        assertTrue(
            controller.shouldDisableDropdown(),
            'Disabled if print request is in progress');
      });

  // Verify shouldDisableDropdown returns true if destination manager is not
  // initialized.
  test(
      'shouldDisableSelect returns true if initial destinations are not ' +
          'loaded',
      async () => {
        // Initialize manager and force hasAnyDestinations to false.
        await waitForInitialDestinationSet(mockTimer, testDelay);
        await waitForPrintTicketManagerInitialized();
        const hasInitialDestFn = mockController.createFunctionMock(
            destinationManager, 'hasAnyDestinations');
        hasInitialDestFn.returnValue = false;
        assertTrue(
            controller.shouldDisableDropdown(),
            'Disabled if initial destinations are not loaded');
      });

  // Verify shouldDisableDropdown returns false if PrintTicketManager and
  // DestinationManager are in a state ready to receive destination changes.
  test('shouldDisableSelect returns false', async () => {
    await waitForInitialDestinationSet(mockTimer, testDelay);
    await waitForPrintTicketManagerInitialized();
    assertFalse(
        controller.shouldDisableDropdown(),
        'Enabled if active destination can be updated');
  });
});
