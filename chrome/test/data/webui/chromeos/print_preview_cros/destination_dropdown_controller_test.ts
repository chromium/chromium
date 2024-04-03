// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown_controller.js';

import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('DestinationDropdownController', () => {
  let controller: DestinationDropdownController;
  let destinationManager: DestinationManager;
  let fakeDestinationProvider: FakeDestinationProvider;
  let mockController: MockController;
  let eventTracker: EventTracker;
  let mockTimer: MockTimer;

  const testDelay = 1;

  setup(() => {
    mockController = new MockController();
    eventTracker = new EventTracker();
    mockTimer = new MockTimer();
    mockTimer.install();

    fakeDestinationProvider = new FakeDestinationProvider();
    fakeDestinationProvider.setTestDelay(testDelay);
    setDestinationProviderForTesting(fakeDestinationProvider);
    DestinationManager.resetInstanceForTesting();
    destinationManager = DestinationManager.getInstance();

    controller = new DestinationDropdownController(eventTracker);
  });

  teardown(() => {
    DestinationManager.resetInstanceForTesting();
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
        const testEvent =
            createCustomEvent(DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED);
        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        onStateChangedFn.addExpectation(testEvent);

        // Simulate event being fired.
        destinationManager.dispatchEvent(testEvent);
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
});
