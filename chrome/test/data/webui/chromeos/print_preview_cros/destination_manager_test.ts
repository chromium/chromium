// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/destination_manager.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_STATE_CHANGED, DestinationManager, DestinationManagerState} from 'chrome://os-print/js/data/destination_manager.js';
import {FakeDestinationProvider, GET_LOCAL_DESTINATIONS_METHOD} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {Destination} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('DestinationManager', () => {
  let instance: DestinationManager;
  let destinationProvider: FakeDestinationProvider;
  let mockTimer: MockTimer;

  const testDelay = 1;

  setup(() => {
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

  // Verify getLocalPrinters is called on construction of manager.
  test('on create getLocalPrinters is called', () => {
    const expectedCallCount = 1;
    assertEquals(
        expectedCallCount,
        destinationProvider.getCallCount(GET_LOCAL_DESTINATIONS_METHOD),
        `${GET_LOCAL_DESTINATIONS_METHOD} called in construction of manager`);
  });

  // Verify destination manager state updated called when getLocalPrinters
  // resolves.
  test(
      'starting and resolving getLocalPrinters triggers state update',
      async () => {
        assertEquals(
            DestinationManagerState.FETCHING, instance.getState(),
            'Fetch in progress');

        const stateChanged =
            eventToPromise(DESTINATION_MANAGER_STATE_CHANGED, instance);
        mockTimer.tick(testDelay);
        await stateChanged;

        assertEquals(
            DestinationManagerState.LOADED, instance.getState(),
            'Fetch complete');
      });

  // Verify destination manager sets fallback destination to PDF if no other
  // destinations are returned in local printer fetch.
  test(
      'starting and resolving getLocalPrinters triggers state active' +
          ' destination update',
      async () => {
        assertEquals(
            DestinationManagerState.FETCHING, instance.getState(),
            'Fetch in progress');
        assertEquals(
            null, instance.getActiveDestination(),
            'Fallback destination is not set before loading local printers');

        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, instance);

        // Resolve local printers fetch.
        mockTimer.tick(testDelay);
        await stateChanged;

        assertEquals(
            DestinationManagerState.LOADED, instance.getState(),
            'Fetch complete');
        assertDeepEquals(
            PDF_DESTINATION, instance.getActiveDestination(),
            `Fallback destination is ${PDF_DESTINATION.displayName}`);
      });
});
