// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/destination_manager.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_DESTINATIONS_CHANGED, DESTINATION_MANAGER_SESSION_INITIALIZED, DESTINATION_MANAGER_STATE_CHANGED, DestinationManager, DestinationManagerState} from 'chrome://os-print/js/data/destination_manager.js';
import {FakeDestinationProvider, GET_LOCAL_DESTINATIONS_METHOD, OBSERVE_DESTINATION_CHANGES_METHOD} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {Destination, PrinterStatusReason, PrinterType} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestDestination} from './test_utils.js';

suite('DestinationManager', () => {
  let instance: DestinationManager;
  let destinationProvider: FakeDestinationProvider;
  let mockController: MockController;
  let mockTimer: MockTimer;

  const testDelay = 1;

  setup(() => {
    mockController = new MockController();
    mockTimer = new MockTimer();
    mockTimer.install();

    DestinationManager.resetInstanceForTesting();
    destinationProvider = new FakeDestinationProvider();
    destinationProvider.setTestDelay(testDelay);
    setDestinationProviderForTesting(destinationProvider);

    instance = DestinationManager.getInstance();
  });

  teardown(() => {
    DestinationManager.resetInstanceForTesting();
    destinationProvider.reset();
    mockTimer.uninstall();
    mockController.reset();
  });

  test('is a singleton', () => {
    const instance1 = DestinationManager.getInstance();
    const instance2 = DestinationManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = DestinationManager.getInstance();
    DestinationManager.resetInstanceForTesting();
    const instance2 = DestinationManager.getInstance();
    assertNotEquals(instance1, instance2, 'Reset clears static instance');
  });

  // Verify `hasLoadedAnInitialDestination` returns false by default.
  test('on create hasLoadedAnInitialDestination is false', () => {
    assertFalse(instance.hasLoadedAnInitialDestination());
  });

  // Verify PDF printer included in destinations.
  test('getDestinations contains PDF printer', () => {
    const destinations: Destination[] = instance.getDestinations();
    const pdfIndex =
        destinations.findIndex((d: Destination) => d.id === PDF_DESTINATION.id);
    const notFoundIndex = -1;
    assertNotEquals(notFoundIndex, pdfIndex, 'PDF destination available');
  });

  // Verify getLocalDestinations is called during initializeSession.
  test('initializeSession calls getLocalDestinations', () => {
    let expectedCallCount = 0;
    assertEquals(
        expectedCallCount,
        destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
        `${GET_LOCAL_DESTINATIONS_METHOD} not called`);

    // Initialize destination manager.
    instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    ++expectedCallCount;
    assertEquals(
        expectedCallCount,
        destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
        `${GET_LOCAL_DESTINATIONS_METHOD} called`);
  });

  // Verify destination manager state updated called when getLocalDestinations
  // resolves.
  test(
      'starting and resolving getLocalDestinations triggers state update',
      async () => {
        let stateChange =
            eventToPromise(DESTINATION_MANAGER_STATE_CHANGED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await stateChange;
        assertEquals(
            DestinationManagerState.FETCHING, instance.getState(),
            'Fetch in progress');

        stateChange =
            eventToPromise(DESTINATION_MANAGER_STATE_CHANGED, instance);
        mockTimer.tick(testDelay);
        await stateChange;

        assertEquals(
            DestinationManagerState.LOADED, instance.getState(),
            'Fetch complete');
      });

  // Verify destination manager sets fallback destination to PDF if no other
  // destinations are returned in local printer fetch and session is
  // initialized.
  test(
      'starting and resolving getLocalDestinations triggers state active' +
          ' destination update',
      async () => {
        assertEquals(
            null, instance.getActiveDestination(),
            'Fallback destination is not set before loading local printers');

        // Resolve local printers fetch and initialize session.
        const activeDestChange = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        mockTimer.tick(testDelay);
        await activeDestChange;

        assertDeepEquals(
            PDF_DESTINATION, instance.getActiveDestination(),
            `Fallback destination is ${PDF_DESTINATION.displayName}`);
      });

  // Verify `isSessionInitialized` returns true and triggers
  // `DESTINATION_MANAGER_SESSION_INITIALIZED` event after `initializeSession`
  // called.
  test(
      'initializeSession updates isSessionInitialized and triggers ' +
          DESTINATION_MANAGER_SESSION_INITIALIZED,
      async () => {
        const instance = DestinationManager.getInstance();
        assertFalse(
            instance.isSessionInitialized(),
            'Before initializeSession, instance should not be initialized');

        // Set initial context.
        const sessionInit =
            eventToPromise(DESTINATION_MANAGER_SESSION_INITIALIZED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await sessionInit;

        assertTrue(
            instance.isSessionInitialized(),
            'After initializeSession, instance should be initialized');
      });

  // Verify observeDestinationChanges is called on construction of manager.
  test('on create observeDestinationChanges is called', () => {
    const expectedCallCount = 1;
    assertEquals(
        expectedCallCount,
        destinationProvider.getCallCount(OBSERVE_DESTINATION_CHANGES_METHOD),
        `${OBSERVE_DESTINATION_CHANGES_METHOD} called in constructor`);
  });

  // Verify DESTINATION_MANAGER_DESTINATIONS_CHANGED event not triggered from
  // onDestinationsChanged when there are no destinations.
  test(
      `${DESTINATION_MANAGER_DESTINATIONS_CHANGED} triggered not from ` +
          'onDestinationsChanged when destinations is empty',
      async () => {
        const dispatchFn =
            mockController.createFunctionMock(instance, 'dispatchEvent');
        // Set destinations returned to empty array to verify dispatch is not
        // called.
        destinationProvider.setDestinationsChangesData([]);
        destinationProvider.triggerOnDestinationsChanged();

        // No calls expected.
        dispatchFn.verifyMock();
      });

  // Verify DESTINATION_MANAGER_DESTINATIONS_CHANGED event triggered from
  // onDestinationsChanged when destinations are updated.
  test(
      `${DESTINATION_MANAGER_DESTINATIONS_CHANGED} triggered from ` +
          'onDestinationsChanged when destinations are updated',
      async () => {
        // Reset mock controller to allow actual call to dispatch.
        mockController.reset();
        const destinationsChanged =
            eventToPromise(DESTINATION_MANAGER_DESTINATIONS_CHANGED, instance);
        const destinations = [createTestDestination(), createTestDestination()];
        destinationProvider.setDestinationsChangesData(destinations);
        destinationProvider.triggerOnDestinationsChanged();

        await destinationsChanged;
      });

  // Verify destinations from onDestinationsChanged are added to managers
  // destination list and cache if new.
  test(
      'onDestinationsChanged with new destinations are added to manager',
      async () => {
        const destinationsChanged =
            eventToPromise(DESTINATION_MANAGER_DESTINATIONS_CHANGED, instance);
        const destinations = [createTestDestination()];
        destinationProvider.setDestinationsChangesData(destinations);
        destinationProvider.triggerOnDestinationsChanged();

        await destinationsChanged;

        const managerDestinations = instance.getDestinations();
        assertEquals(/* expected length*/ 2, managerDestinations.length);
        assertDeepEquals(PDF_DESTINATION, managerDestinations[0]);
        assertDeepEquals(destinations[0], managerDestinations[1]);
      });

  // Verify existing destinations from onDestinationsChanged are merged into
  // managers destination list and cache to avoid losing data set by UI.
  test(
      'onDestinationsChanged existing destinations are merged in manager',
      async () => {
        const destinationsChanged =
            eventToPromise(DESTINATION_MANAGER_DESTINATIONS_CHANGED, instance);
        const testDestination = createTestDestination();
        testDestination.printerManuallySelected = true;
        instance.setDestinationForTesting(testDestination);
        let managerDestinations = instance.getDestinations();
        assertEquals(/* expected length*/ 2, managerDestinations.length);
        assertDeepEquals(testDestination, managerDestinations[1]);

        // Change values on test destination.
        const testDestination2 = createTestDestination(testDestination.id);
        testDestination2.printerType = PrinterType.EXTENSION_PRINTER;
        testDestination2.printerStatusReason = PrinterStatusReason.LOW_ON_INK;
        const destinations = [testDestination2];
        destinationProvider.setDestinationsChangesData(destinations);
        destinationProvider.triggerOnDestinationsChanged();

        await destinationsChanged;

        managerDestinations = instance.getDestinations();
        const mergedDestination = managerDestinations[1]!;
        assertEquals(
            testDestination.id, mergedDestination.id, 'Is merged destination');
        // UI managed values are not updated.
        assertTrue(
            mergedDestination.printerManuallySelected,
            'UI managed field printerManuallySelected not updated');
        // Backend managed fields are updated.
        assertEquals(
            testDestination2.displayName, mergedDestination.displayName,
            'Backend managed field displayName updated');
        assertEquals(
            testDestination2.printerType, mergedDestination.printerType,
            'Backend managed field printerType updated');
        assertEquals(
            testDestination2.printerStatusReason,
            mergedDestination.printerStatusReason,
            'Backend managed field printerStatusReason updated');
      });

  // Verify destinations from getLocalDestinations are added to the manager's
  // destination list and cache if new.
  test(
      'getLocalDestinations with new destinations are added to manager',
      async () => {
        const destinations = [createTestDestination()];
        destinationProvider.setLocalDestinationResult(destinations);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        const stateChanged =
            eventToPromise(DESTINATION_MANAGER_STATE_CHANGED, instance);

        // Wait for getLocalDestinations to resolve.
        mockTimer.tick(testDelay);
        await stateChanged;

        const managerDestinations = instance.getDestinations();
        const expectedDestinations = [PDF_DESTINATION, ...destinations];
        assertDeepEquals(expectedDestinations, managerDestinations);
      });

  // Verify destinations from getLocalDestinations are merged to the manager's
  // destination list and cache if already in cache.
  test(
      'getLocalDestinations with existing destinations are merged into manager',
      async () => {
        // Set an existing destination with UI updated fields to merge.
        const testDestination = createTestDestination();
        testDestination.printerManuallySelected = true;
        instance.setDestinationForTesting(testDestination);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        let managerDestinations = instance.getDestinations();
        let expectedDestinations = [PDF_DESTINATION, testDestination];
        assertDeepEquals(expectedDestinations, managerDestinations);

        // Create destination to merge into testDestination.
        const testDestination2 = createTestDestination(testDestination.id);
        const destinations = [testDestination2];
        destinationProvider.setLocalDestinationResult(destinations);
        const stateChanged =
            eventToPromise(DESTINATION_MANAGER_STATE_CHANGED, instance);

        // Wait for getLocalDestinations to resolve.
        mockTimer.tick(testDelay);
        await stateChanged;

        managerDestinations = instance.getDestinations();
        expectedDestinations = [
          PDF_DESTINATION,
          {...testDestination2, printerManuallySelected: true},
        ];
        assertDeepEquals(expectedDestinations, managerDestinations);
      });
});
