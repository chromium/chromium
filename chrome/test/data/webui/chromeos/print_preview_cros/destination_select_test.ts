// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_select.js';

import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationDropdownElement} from 'chrome://os-print/js/destination_dropdown.js';
import {DestinationSelectElement} from 'chrome://os-print/js/destination_select.js';
import {DESTINATION_SELECT_SHOW_LOADING_CHANGED, DestinationSelectController} from 'chrome://os-print/js/destination_select_controller.js';
import {FakeDestinationProvider} from 'chrome://os-print/js/fakes/fake_destination_provider.js';
import {setDestinationProviderForTesting} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationSelect', () => {
  let element: DestinationSelectElement;
  let controller: DestinationSelectController;
  let destinationManager: DestinationManager;
  let destinationProvider: FakeDestinationProvider;
  let mockController: MockController;
  let mockTimer: MockTimer;

  const loadingSelector = '#loading';
  const testDelay = 1;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockController = new MockController();
    mockTimer = new MockTimer();
    mockTimer.install();

    DestinationManager.resetInstanceForTesting();
    destinationProvider = new FakeDestinationProvider();
    destinationProvider.setTestDelay(testDelay);
    setDestinationProviderForTesting(destinationProvider);

    DestinationManager.resetInstanceForTesting();
    destinationManager = DestinationManager.getInstance();

    element = document.createElement(DestinationSelectElement.is) as
        DestinationSelectElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
    DestinationManager.resetInstanceForTesting();
    mockTimer.uninstall();
    mockController.reset();
  });

  // Verify the print-preview-cros-app element can be rendered.
  test('element renders', () => {
    assertTrue(
        isVisible(element), `Should display ${DestinationSelectElement.is}`);
  });

  // Verify destination-select element has a controller configured.
  test('has element controller', () => {
    assertTrue(
        !!controller,
        `${DestinationSelectElement.is} should have controller configured`);
  });

  // Verify expected elements display while `controller.shouldShowLoading` is
  // true.
  test('displays expected elements when showLoading is true', async () => {
    const hasInitialDestinationsFn = mockController.createFunctionMock(
        destinationManager, 'hasLoadedAnInitialDestination');
    hasInitialDestinationsFn.returnValue = false;

    // Move time forward to resolve getLocalDestinations in manager.
    const changeEvent =
        eventToPromise(DESTINATION_SELECT_SHOW_LOADING_CHANGED, controller);
    mockTimer.tick(testDelay);
    await changeEvent;

    assertTrue(
        isChildVisible(element, loadingSelector),
        `Loading UX should be visible`);
    assertFalse(
        isChildVisible(element, DestinationDropdownElement.is),
        `${DestinationDropdownElement.is} should not be visible`);
  });

  // Verify expected elements display while `controller.shouldShowLoading` is
  // false.
  test('displays expected loading UX', async () => {
    const hasInitialDestinationsFn = mockController.createFunctionMock(
        destinationManager, 'hasLoadedAnInitialDestination');
    hasInitialDestinationsFn.returnValue = true;

    // Move time forward to resolve getLocalDestinations in manager.
    const changeEvent =
        eventToPromise(DESTINATION_SELECT_SHOW_LOADING_CHANGED, controller);
    mockTimer.tick(testDelay);
    await changeEvent;

    assertFalse(
        isChildVisible(element, loadingSelector),
        `Loading UX should not be visible`);
    assertTrue(
        isChildVisible(element, DestinationDropdownElement.is),
        `${DestinationDropdownElement.is} should be visible`);
  });

  // Verify loading and dropdown visibility update after
  // DESTINATION_SELECT_SHOW_LOADING_CHANGED event.
  test(
      `${DESTINATION_SELECT_SHOW_LOADING_CHANGED} updates loading and ` +
          'dropdown visibility',
      async () => {
        assertFalse(
            isChildVisible(element, DestinationDropdownElement.is),
            `${DestinationDropdownElement.is} should not be visible`);
        assertTrue(
            isChildVisible(element, loadingSelector),
            `${loadingSelector} should be visible`);

        // Move time forward to resolve getLocalDestinations in manager.
        const changeEvent =
            eventToPromise(DESTINATION_SELECT_SHOW_LOADING_CHANGED, controller);
        mockTimer.tick(testDelay);
        await changeEvent;

        assertTrue(
            isChildVisible(element, DestinationDropdownElement.is),
            `${DestinationDropdownElement.is} should be visible`);
        assertFalse(
            isChildVisible(element, loadingSelector),
            `${loadingSelector} should not be visible`);
      });
});
