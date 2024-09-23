// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/data/capabilities_manager.js';

import {CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING, CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY, CAPABILITIES_MANAGER_SESSION_INITIALIZED, CapabilitiesManager} from 'chrome://os-print/js/data/capabilities_manager.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationProviderComposite} from 'chrome://os-print/js/data/destination_provider_composite.js';
import {FakeDestinationProvider, getFakeCapabilitiesResponse} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getDestinationProvider} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders} from './test_utils.js';

suite('CapabilitiesManager', () => {
  let destinationProvider: FakeDestinationProvider;
  let mockController: MockController;

  setup(() => {
    // Setup fakes for testing.
    mockController = new MockController();
    resetDataManagersAndProviders();
    destinationProvider =
        (getDestinationProvider() as DestinationProviderComposite)
            .fakeDestinationProvider;
    assert(destinationProvider);
  });

  teardown(() => {
    mockController.reset();
    resetDataManagersAndProviders();
  });

  // Initialize the DestinationManager and wait for it to send all events.
  async function waitForDestinationManagerLoad(): Promise<void> {
    const destinationManager = DestinationManager.getInstance();
    destinationManager.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
    const activeDestinationChangedEvent = eventToPromise(
        DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
    return activeDestinationChangedEvent;
  }


  test('is a singleton', () => {
    const instance1 = CapabilitiesManager.getInstance();
    const instance2 = CapabilitiesManager.getInstance();
    assertEquals(instance1, instance2);
  });

  test('can clear singleton', () => {
    const instance1 = CapabilitiesManager.getInstance();
    CapabilitiesManager.resetInstanceForTesting();
    const instance2 = CapabilitiesManager.getInstance();
    assertTrue(instance1 !== instance2);
  });

  // Verify `isSessionInitialized` returns true and triggers
  // CAPABILITIES_MANAGER_SESSION_INITIALIZED event after `initializeSession`
  // called.
  test(
      'initializeSession updates isSessionInitialized and triggers ' +
          CAPABILITIES_MANAGER_SESSION_INITIALIZED,
      async () => {
        await waitForDestinationManagerLoad();

        const instance = CapabilitiesManager.getInstance();
        assertFalse(
            instance.isSessionInitialized(),
            'Before initializeSession, instance should not be initialized');

        // Set initial context.
        const sessionInit =
            eventToPromise(CAPABILITIES_MANAGER_SESSION_INITIALIZED, instance);
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await sessionInit;

        assertTrue(
            instance.isSessionInitialized(),
            'After initializeSession, instance should be initialized');
      });

  // Verify when Destination Manager signals the active destination changed, the
  // Capabilities Manager fetches capabilities.
  test(
      'fetch capabilities on active destination changed and cache response',
      async () => {
        // Set the active destination.
        const activeCapabilities = getFakeCapabilitiesResponse().capabilities;
        const activeDestination =
            createTestDestination(activeCapabilities.destinationId);
        destinationProvider.setCapabilities(activeCapabilities);

        await waitForDestinationManagerLoad();

        const destinationManager = DestinationManager.getInstance();
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = activeDestination;

        const instance = CapabilitiesManager.getInstance();
        instance.initializeSession(FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        let providerCallCount = 0;
        assertEquals(
            providerCallCount,
            destinationProvider.getCallCount('fetchCapabilities'));

        // Simulate the active destination change and wait for the capabilities
        // manager to signal capabilities are ready.
        let capsLoadingEvent = eventToPromise(
            CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING, instance);
        let capsReadyEvent = eventToPromise(
            CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY, instance);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        ++providerCallCount;
        await capsLoadingEvent;
        await capsReadyEvent;

        assertEquals(
            providerCallCount,
            destinationProvider.getCallCount('fetchCapabilities'));
        assertDeepEquals(
            activeCapabilities, instance.getActiveDestinationCapabilities());

        // Simulate the active destination changing again except this time the
        // cached capabilities result is returned.
        capsLoadingEvent = eventToPromise(
            CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_LOADING, instance);
        capsReadyEvent = eventToPromise(
            CAPABILITIES_MANAGER_ACTIVE_DESTINATION_CAPS_READY, instance);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED));
        await capsLoadingEvent;
        await capsReadyEvent;

        assertEquals(
            providerCallCount,
            destinationProvider.getCallCount('fetchCapabilities'));
        assertDeepEquals(
            activeCapabilities, instance.getActiveDestinationCapabilities());
      });
});
