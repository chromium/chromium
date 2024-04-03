// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationDropdownElement} from 'chrome://os-print/js/destination_dropdown.js';
import {DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {DestinationRowElement} from 'chrome://os-print/js/destination_row.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('DestinationDropdown', () => {
  let element: DestinationDropdownElement;
  let controller: DestinationDropdownController;
  let destinationManager: DestinationManager;
  let mockController: MockController;

  const selectedDestinationSelector = '#selected';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockController = new MockController();
    DestinationManager.resetInstanceForTesting();
    destinationManager = DestinationManager.getInstance();

    // Set PDF as default active destination for testing UI.
    const getActiveDestinationFn = mockController.createFunctionMock(
        destinationManager, 'getActiveDestination');
    getActiveDestinationFn.returnValue = PDF_DESTINATION;

    element = document.createElement(DestinationDropdownElement.is) as
        DestinationDropdownElement;
    assertTrue(!!element);
    document.body.append(element);

    controller = element.getControllerForTesting();
  });

  teardown(() => {
    element.remove();
    DestinationManager.resetInstanceForTesting();
    mockController.reset();
  });

  // Verify the dropdown can be added to UI.
  test('element renders', () => {
    assertTrue(
        isVisible(element), `Should display ${DestinationDropdownElement.is}`);
  });

  // Verify dropdown element has a controller configured.
  test('has element controller', async () => {
    assertTrue(
        !!controller,
        `${DestinationDropdownElement.is} should have controller configured`);
  });

  // Verify destination used in UI updates on
  // DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION.
  test(
      `${DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION} updates
    destination display name`,
      async () => {
        const getActiveDestinationFn = mockController.createFunctionMock(
            destinationManager, 'getActiveDestination');
        getActiveDestinationFn.returnValue = null;

        const testEvent =
            createCustomEvent(DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION);
        const selectedChangedEvent1 = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, controller);
        controller.dispatchEvent(testEvent);
        await selectedChangedEvent1;

        const selected = strictQuery<DestinationRowElement>(
            selectedDestinationSelector, element.shadowRoot,
            DestinationRowElement);
        const rowLabel = strictQuery<HTMLElement>(
            '#label', selected.shadowRoot, HTMLElement);
        assertEquals(
            '', rowLabel.textContent!.trim(),
            `Selected destination not display a destination name`);

        // Change result to non-null destination.
        getActiveDestinationFn.returnValue = PDF_DESTINATION;
        const selectedChangedEvent2 = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, controller);
        controller.dispatchEvent(testEvent);
        await selectedChangedEvent2;

        assertEquals(
            PDF_DESTINATION.displayName, rowLabel.textContent!.trim(),
            `Selected destination should match active destination`);
      });
});
