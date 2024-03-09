// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select_controller.js';

import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';

suite('DestinationSelectController', () => {
  let controller: DestinationSelectController;
  let destinationManager: DestinationManager;
  let mockController: MockController;

  setup(() => {
    DestinationManager.resetInstanceForTesting();
    destinationManager = DestinationManager.getInstance();
    mockController = new MockController();

    controller = new DestinationSelectController();
    assertTrue(!!controller);
  });

  teardown(() => {
    mockController.reset();
    DestinationManager.resetInstanceForTesting();
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
  // DestinationManager's `hasInitialDestinationsLoaded` call is false.
  test(
      'shouldShowLoading returns true when destination manager has not ' +
          'received initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasInitialDestinationsLoaded');
        hasDestinationsFn.returnValue = false;

        assertTrue(controller.shouldShowLoading(), 'Is fetching destinations');
      });

  // Verify shouldShowLoading returns false if
  // DestinationManager's `hasInitialDestinationsLoaded` call is true.
  test(
      'shouldShowLoading returns false when destination manager has received ' +
          'initial destinations',
      () => {
        const hasDestinationsFn = mockController.createFunctionMock(
            destinationManager, 'hasInitialDestinationsLoaded');
        hasDestinationsFn.returnValue = true;

        assertFalse(controller.shouldShowLoading(), 'Has fetched destinations');
      });
});
