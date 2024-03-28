// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select_controller.js';

import {DESTINATION_MANAGER_STATE_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DESTINATION_SELECT_SHOW_LOADING_CHANGED, DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('DestinationSelectController', () => {
  let controller: DestinationSelectController;
  let destinationManager: DestinationManager;
  let destinationProvider: FakeDestinationProvider;
  let mockController: MockController;
  let eventTracker: EventTracker;

  setup(() => {
    DestinationManager.resetInstanceForTesting();
    destinationProvider = new FakeDestinationProvider();
    setDestinationProviderForTesting(destinationProvider);
    destinationManager = DestinationManager.getInstance();

    mockController = new MockController();
    eventTracker = new EventTracker();

    controller = new DestinationSelectController(eventTracker);
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
    DestinationManager.resetInstanceForTesting();
    eventTracker.removeAll();
  });

  // Verify controller is event target.
  test('is event target', () => {
    assertTrue(controller instanceof EventTarget, 'Is event target');
  });

  // Verify shouldShowLoading returns true by default.
  test('shouldShowLoading returns true by default', () => {
    assertTrue(controller.shouldShowLoading());
  });

  // Verify shouldShowLoading returns true if
  // DestinationManager's `hasLoadedAnInitialDestination` call is false.
  test(
      'shouldShowLoading returns true when destination manager has not ' +
          'received initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasLoadedAnInitialDestination');
        hasDestinationsFn.returnValue = false;

        assertTrue(controller.shouldShowLoading(), 'Is fetching destinations');
      });

  // Verify shouldShowLoading returns false if
  // DestinationManager's `hasLoadedAnInitialDestination` call is true.
  test(
      'shouldShowLoading returns false when destination manager has received ' +
          'initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasLoadedAnInitialDestination');
        hasDestinationsFn.returnValue = true;

        assertFalse(controller.shouldShowLoading(), 'Has fetched destinations');
      });


  // Verify controller is listening to DESTINATION_MANAGER_STATE_CHANGED event.
  test(
      'onDestinationManagerStateChanged called on ' +
          DESTINATION_MANAGER_STATE_CHANGED,
      async () => {
        const onStateChangedFn = mockController.createFunctionMock(
            controller, 'onDestinationManagerStateChanged');
        const testEvent = createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED);
        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_STATE_CHANGED, destinationManager);
        onStateChangedFn.addExpectation(testEvent);

        // Simulate event being fired.
        destinationManager.dispatchEvent(testEvent);
        await stateChanged;

        mockController.verifyMocks();
      });

  // Verify DESTINATION_SELECT_SHOW_LOADING_CHANGED emits when destination
  // manager state changes.
  test(
      `emit ${DESTINATION_SELECT_SHOW_LOADING_CHANGED} ` +
          'on destination manager state changed',
      async () => {
        const testEvent = createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED);
        const showLoadingChanged =
            eventToPromise(DESTINATION_SELECT_SHOW_LOADING_CHANGED, controller);

        // Simulate event being fired.
        destinationManager.dispatchEvent(testEvent);
        await showLoadingChanged;
      });
});
