// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select_controller.js';

import {DESTINATION_MANAGER_SESSION_INITIALIZED, DESTINATION_MANAGER_STATE_CHANGED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED, DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {resetDataManagersAndProviders} from './test_utils.js';

suite('DestinationSelectController', () => {
  let controller: DestinationSelectController;
  let destinationManager: DestinationManager;
  let mockController: MockController;
  let eventTracker: EventTracker;

  setup(() => {
    resetDataManagersAndProviders();
    destinationManager = DestinationManager.getInstance();

    mockController = new MockController();
    eventTracker = new EventTracker();

    controller = new DestinationSelectController(eventTracker);
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
    eventTracker.removeAll();
    resetDataManagersAndProviders();
  });

  // Verify controller is event target.
  test('is event target', () => {
    assertTrue(controller instanceof EventTarget, 'Is event target');
  });

  // Verify shouldShowLoadingUi returns true by default.
  test('shouldShowLoadingUi returns true by default', () => {
    assertTrue(controller.shouldShowLoadingUi());
  });

  // Verify shouldShowLoadingUi returns true if
  // DestinationManager's `hasAnyDestinations` call is false.
  test(
      'shouldShowLoadingUi returns true when destination manager has not ' +
          'received initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasAnyDestinations');
        hasDestinationsFn.returnValue = false;
        const initializedFn = mockController.createFunctionMock(
            destinationManager, 'isSessionInitialized');
        initializedFn.returnValue = true;

        assertTrue(
            controller.shouldShowLoadingUi(), 'Is fetching destinations');
      });

  // Verify shouldShowLoadingUi returns true if
  // DestinationManager's `isSessionInitialized` call is false.
  test(
      'shouldShowLoadingUi returns true when destination manager has not ' +
          'received initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasAnyDestinations');
        hasDestinationsFn.returnValue = true;
        const initializedFn = mockController.createFunctionMock(
            destinationManager, 'isSessionInitialized');
        initializedFn.returnValue = false;

        assertTrue(controller.shouldShowLoadingUi(), 'Is initializing manager');
      });

  // Verify shouldShowLoadingUi returns false if
  // DestinationManager's `hasAnyDestinations` and
  // `isSessionInitialized` call is true.
  test(
      'shouldShowLoadingUi returns false when destination manager has ' +
          'received initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasAnyDestinations');
        hasDestinationsFn.returnValue = true;
        const initializedFn = mockController.createFunctionMock(
            destinationManager, 'isSessionInitialized');
        initializedFn.returnValue = true;

        assertFalse(
            controller.shouldShowLoadingUi(), 'Has fetched destinations');
      });

  // Verify controller is listening to DESTINATION_MANAGER_STATE_CHANGED event.
  test(
      'onDestinationManagerStateChanged called on ' +
          DESTINATION_MANAGER_STATE_CHANGED,
      async () => {
        const onStateChangedFn = mockController.createFunctionMock(
            controller, 'onDestinationManagerStateChanged');
        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_STATE_CHANGED, destinationManager);
        onStateChangedFn.addExpectation();

        // Simulate event being fired.
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED));
        await stateChanged;

        mockController.verifyMocks();
      });

  // Verify DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED emits when destination
  // manager state changes.
  test(
      `emit ${DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED} ` +
          'on destination manager state changed',
      async () => {
        const showLoadingChanged = eventToPromise(
            DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED, controller);

        // Simulate event being fired.
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_STATE_CHANGED));
        await showLoadingChanged;
      });

  // Verify controller is listening to DESTINATION_MANAGER_SESSION_INITIALIZED
  // event.
  test(
      'onDestinationManagerSessionInitialized called on ' +
          DESTINATION_MANAGER_SESSION_INITIALIZED,
      async () => {
        const onStateChangedFn = mockController.createFunctionMock(
            controller, 'onDestinationManagerSessionInitialized');
        const stateChanged = eventToPromise(
            DESTINATION_MANAGER_SESSION_INITIALIZED, destinationManager);
        onStateChangedFn.addExpectation();

        // Simulate event being fired.
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_SESSION_INITIALIZED));
        await stateChanged;

        mockController.verifyMocks();
      });

  // Verify DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED emits when destination
  // manager initialized.
  test(
      `emit ${DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED} ` +
          'on destination manager initialized',
      async () => {
        const testEvent =
            createCustomEvent(DESTINATION_MANAGER_SESSION_INITIALIZED);
        const showLoadingChanged = eventToPromise(
            DESTINATION_SELECT_SHOW_LOADING_UI_CHANGED, controller);

        // Simulate event being fired.
        destinationManager.dispatchEvent(testEvent);
        await showLoadingChanged;
      });
});
