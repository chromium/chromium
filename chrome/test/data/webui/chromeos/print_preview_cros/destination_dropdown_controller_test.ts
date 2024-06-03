// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown_controller.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_DESTINATIONS_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {PrintTicketManager} from 'chrome://os-print/js/data/print_ticket_manager.js';
import {DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getDestinationProvider} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders, waitForInitialDestinationSet} from './test_utils.js';

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
        getDestinationProvider() as FakeDestinationProvider;
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
});
