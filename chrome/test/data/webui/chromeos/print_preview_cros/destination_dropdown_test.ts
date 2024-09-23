// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-print/js/destination_dropdown.js';

import {PDF_DESTINATION} from 'chrome://os-print/js/data/destination_constants.js';
import {DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, DESTINATION_MANAGER_DESTINATIONS_CHANGED, DESTINATION_MANAGER_SESSION_INITIALIZED, DestinationManager} from 'chrome://os-print/js/data/destination_manager.js';
import {DestinationProviderComposite} from 'chrome://os-print/js/data/destination_provider_composite.js';
import {DestinationDropdownElement} from 'chrome://os-print/js/destination_dropdown.js';
import {DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED, DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, DestinationDropdownController} from 'chrome://os-print/js/destination_dropdown_controller.js';
import {DestinationRowElement} from 'chrome://os-print/js/destination_row.js';
import {FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL} from 'chrome://os-print/js/fakes/fake_print_preview_page_handler.js';
import {createCustomEvent} from 'chrome://os-print/js/utils/event_utils.js';
import {getDestinationProvider} from 'chrome://os-print/js/utils/mojo_data_providers.js';
import type {Destination} from 'chrome://os-print/js/utils/print_preview_cros_app_types.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {MockController} from 'chrome://webui-test/chromeos/mock_controller.m.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {createTestDestination, resetDataManagersAndProviders, waitForInitialDestinationSet, waitForPrintTicketManagerInitialized} from './test_utils.js';

suite('DestinationDropdown', () => {
  let element: DestinationDropdownElement;
  let controller: DestinationDropdownController;
  let destinationManager: DestinationManager;
  let mockController: MockController;

  const contentSelector = '#content';
  const selectedDestinationSelector = '#selected';
  const initialDestinations = [PDF_DESTINATION];

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockController = new MockController();
    resetDataManagersAndProviders();
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
    resetDataManagersAndProviders();
    mockController.reset();
  });

  function assertExpectedDestinationsDisplayedInContentRows(
      expectedDestinations: Destination[]): void {
    const contentRows =
        getDropdownContent().querySelectorAll(DestinationRowElement.is);
    assertEquals(expectedDestinations.length, contentRows.length);
    contentRows.forEach((element: DestinationRowElement, key: number) => {
      assertDeepEquals(
          expectedDestinations[key], element.destination,
          `Expected DestinationRowElement[${key}] to be ${
              JSON.stringify(expectedDestinations[key])}
              found
              ${JSON.stringify(element.destination)}`);
    });
  }

  function getDropdownContent(): HTMLDivElement {
    return strictQuery<HTMLDivElement>(
        contentSelector, element.shadowRoot, HTMLDivElement);
  }

  function getSelectedDestinationRow(): DestinationRowElement {
    return strictQuery<DestinationRowElement>(
        selectedDestinationSelector, element.shadowRoot, DestinationRowElement);
  }

  function getSelectedDestinationRowLabel(): string {
    const selected = getSelectedDestinationRow();
    const rowLabel =
        strictQuery<HTMLElement>('#label', selected.shadowRoot, HTMLElement);
    return rowLabel.textContent!.trim();
  }

  async function clickDropdownRowFor(destinationId: string): Promise<void> {
    assert(element.shadowRoot);
    const content = strictQuery<HTMLDivElement>(
        '#content', element.shadowRoot, HTMLDivElement);
    assertTrue(isVisible(content), 'Dropdown needs to be open');
    const destinationRow = strictQuery<DestinationRowElement>(
        `#row-${destinationId}`, content, DestinationRowElement);
    assert(destinationRow, 'Only attempt to click existing destination');
    const clickEvent = eventToPromise('click', destinationRow);
    destinationRow.click();
    await clickEvent;
  }

  async function toggleDropdown(): Promise<void> {
    const selected = getSelectedDestinationRow();
    const clickEvent = eventToPromise('click', selected);
    selected.click();
    await clickEvent;
    flush();
  }

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

        const selectedChangedEvent1 = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, controller);
        controller.dispatchEvent(createCustomEvent(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION));
        await selectedChangedEvent1;

        assertEquals(
            '', getSelectedDestinationRowLabel(),
            `Selected destination not display a destination name`);

        // Change result to non-null destination.
        getActiveDestinationFn.returnValue = PDF_DESTINATION;
        const selectedChangedEvent2 = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, controller);
        controller.dispatchEvent(createCustomEvent(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION));
        await selectedChangedEvent2;

        assertEquals(
            PDF_DESTINATION.displayName, getSelectedDestinationRowLabel(),
            `Selected destination should match active destination`);
      });

  // Verify clicking dropdown's selected UI toggles content visibility.
  test('clicking dropdown toggles content visibility', async () => {
    await waitForInitialDestinationSet();
    await waitForPrintTicketManagerInitialized();

    assertFalse(element.disabled);
    assertTrue(isVisible(getSelectedDestinationRow()));
    const content = getDropdownContent();
    assertFalse(isVisible(content), 'Content is not initially displayed');

    // Open dropdown.
    await toggleDropdown();
    assertTrue(isVisible(content), 'Content displayed after click');

    // Close dropdown.
    await toggleDropdown();
    assertFalse(isVisible(content), 'Content closed after click');
  });

  // Verify DESTINATION_DROPDOWN_UPDATE_DESTINATIONS triggers handler in UI.
  test(
      `element handles ${DESTINATION_DROPDOWN_UPDATE_DESTINATIONS} event`,
      async () => {
        const updateHandlerFn = mockController.createFunctionMock(
            element, 'onDestinationDropdownUpdateDestinations');
        const updateContentEvent = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, controller);
        updateHandlerFn.addExpectation();
        controller.dispatchEvent(
            createCustomEvent(DESTINATION_DROPDOWN_UPDATE_DESTINATIONS));
        await updateContentEvent;

        updateHandlerFn.verifyMock();
      });

  // Verify after initializing active destination is selected and dropdown
  // contains destinations added by getLocalDestinations.
  test(
      'dropdown matches destination manager active and destination list',
      async () => {
        // Clear mock from setup and wait until destination manager initialized.
        // Only destination in list should be PDF destination.
        mockController.reset();
        const initializeDestMgrEvent = eventToPromise(
            DESTINATION_MANAGER_SESSION_INITIALIZED, destinationManager);
        const activeDestEvent = eventToPromise(
            DESTINATION_MANAGER_ACTIVE_DESTINATION_CHANGED, destinationManager);
        destinationManager.initializeSession(
            FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await initializeDestMgrEvent;
        await activeDestEvent;
        await toggleDropdown();

        const activeDestination = destinationManager.getActiveDestination();
        assert(activeDestination, 'Active destination should be set');
        assertEquals(
            activeDestination.displayName, getSelectedDestinationRowLabel());
        assertExpectedDestinationsDisplayedInContentRows(initialDestinations);
      });

  // Verify dropdown content updates when DestinationManager observes
  // `onDestinationsChanged` and emits a destinations changed event.
  test(
      'dropdown content updates when observer triggers destinations changed',
      async () => {
        // Clear mock from setup and wait until destination manager initialized.
        // Only destination in list should be PDF destination.
        mockController.reset();
        const initializeDestMgrEvent = eventToPromise(
            DESTINATION_MANAGER_SESSION_INITIALIZED, destinationManager);
        destinationManager.initializeSession(
            FAKE_PRINT_SESSION_CONTEXT_SUCCESSFUL);
        await initializeDestMgrEvent;
        await toggleDropdown();

        assertExpectedDestinationsDisplayedInContentRows(initialDestinations);

        // Simulate update from destination observer appends a new destination
        // to the list of destinations in dropdown.
        const destinationProvider =
            (getDestinationProvider() as DestinationProviderComposite)
                .fakeDestinationProvider;
        const addedDestination = createTestDestination();
        destinationProvider.setDestinationsChangesData([addedDestination]);
        const updateContentEvent = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, controller);
        destinationProvider.triggerOnDestinationsChanged();
        await updateContentEvent;

        assertExpectedDestinationsDisplayedInContentRows([
          PDF_DESTINATION,
          addedDestination,
        ]);
      });

  // Verify clicking a DestinationRowElement in contents calls controller's
  // updateActiveDestination handler with the destination id of the clicked
  // DestinationRowElement, updates selectedDestination, and closes content.
  test(
      'DestinationRowElement immediately closes the dropdown and passes ID ' +
          'of the clicked row to updateActiveDestination',
      async () => {
        mockController.reset();
        const selectedChanged = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_SELECTED_DESTINATION, controller);
        await waitForInitialDestinationSet();
        await waitForPrintTicketManagerInitialized();
        await selectedChanged;
        const updateFn = mockController.createFunctionMock(
            controller, 'updateActiveDestination');
        updateFn.returnValue = true;
        const testDestination = createTestDestination();
        const destChangedEvent = eventToPromise(
            DESTINATION_DROPDOWN_UPDATE_DESTINATIONS, controller);
        destinationManager.setDestinationForTesting(testDestination);
        destinationManager.dispatchEvent(
            createCustomEvent(DESTINATION_MANAGER_DESTINATIONS_CHANGED));
        await destChangedEvent;

        // Simulate clicking a destination row.
        await toggleDropdown();
        updateFn.addExpectation(testDestination.id);
        await clickDropdownRowFor(testDestination.id);

        assertFalse(isVisible(getDropdownContent()), 'Dropdown closed');
        assertEquals(
            testDestination.displayName, getSelectedDestinationRowLabel());
        updateFn.verifyMock();
      });

  // Verify clicking a DestinationRowElement closes content and does not
  // update selectedDestination if provided ID is active destination.
  test(
      'selectedDestination not updated if updateActiveDestination returns ' +
          'false',
      async () => {
        mockController.reset();
        await waitForInitialDestinationSet();
        await waitForPrintTicketManagerInitialized();
        assertEquals(
            PDF_DESTINATION.displayName, getSelectedDestinationRowLabel());

        // Simulate clicking a destination row.
        await toggleDropdown();
        await clickDropdownRowFor(PDF_DESTINATION.id);

        assertFalse(isVisible(getDropdownContent()), 'Dropdown closed');
        assertEquals(
            PDF_DESTINATION.displayName, getSelectedDestinationRowLabel());
      });

  // Verify click ignored if disabled is true.
  test('cannot toggle open content when disabled', async () => {
    await waitForInitialDestinationSet();
    await waitForPrintTicketManagerInitialized();
    assertFalse(element.disabled);

    // Force disabled to true and emit change event to update UI.
    const updateFn =
        mockController.createFunctionMock(controller, 'shouldDisableDropdown');
    updateFn.returnValue = true;
    controller.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED));
    await toggleDropdown();

    assertTrue(element.disabled);
    assertFalse(isVisible(getDropdownContent()), 'Dropdown remains closed');
  });

  // Verify dropdown closed if open when disabled event returns true.
  test('dropdown closed when disabled', async () => {
    await waitForInitialDestinationSet();
    await waitForPrintTicketManagerInitialized();
    // Open dropdown.
    assertFalse(element.disabled);
    await toggleDropdown();
    assertTrue(isVisible(getDropdownContent()), 'Dropdown open');

    // Force disabled to true and emit change event to update UI.
    const updateFn =
        mockController.createFunctionMock(controller, 'shouldDisableDropdown');
    updateFn.returnValue = true;
    controller.dispatchEvent(
        createCustomEvent(DESTINATION_DROPDOWN_DROPDOWN_DISABLED_CHANGED));

    assertTrue(element.disabled);
    assertFalse(isVisible(getDropdownContent()), 'Dropdown closed');
  });
});
