// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilterBarElement} from 'chrome://updater/event_list/filter_bar.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FilterBarElement', () => {
  let filterBar: FilterBarElement;

  setup(() => {
    loadTimeData.overrideValues({
      'filterChipApp': 'App: $1',
      'filterChipDate': 'Date: $1',
      'filterChipEventType': 'Event Type: $1',
      'filterChipUpdateOutcome': 'Update Outcome: $1',
      'dateFilterRange': '$1 to $2',
      'dateFilterAfter': 'After $1',
      'dateFilterBefore': 'Before $1',
      'eventTypeINSTALL': 'Install',
      'eventTypeUPDATE': 'Update',
      'eventTypeUNINSTALL': 'Uninstall',
      'updateOutcomeUPDATED': 'Updated',
      'updateOutcomeUPDATE_ERROR': 'Update Error',
      'numKnownApps': 2,
      'knownAppName0': 'Google Chrome',
      'knownAppIds0': 'COM.GOOGLE.CHROME"',
      'knownAppName1': 'Google Chrome Beta',
      'knownAppIds1': 'COM.GOOGLE.CHROME.BETA"',
      'defaultAppFilters': 'Google Chrome,Google Chrome Beta',
    });

    filterBar = new FilterBarElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterBar);
  });

  test('renders correctly', () => {
    assertTrue(filterBar instanceof HTMLElement);
    assertEquals('FILTER-BAR', filterBar.tagName);
  });

  test('initializes with default filters', () => {
    const chips = filterBar.shadowRoot.querySelectorAll('.chip');
    assertEquals(3, chips.length);
    assertStringContains(
        chips[0]!.textContent, 'App: Google Chrome, Google Chrome Beta');
    assertStringContains(
        chips[1]!.textContent, 'Event Type: Install, Update, Uninstall');
    assertStringContains(
        chips[2]!.textContent, 'Update Outcome: Updated, Update Error');
  });

  test('opens filter menu when add filter button is clicked', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    const filterType = filterBar.shadowRoot.querySelector('type-dialog');
    assertNotEquals(null, filterType);

    assertEquals(
        5, filterType!.shadowRoot.querySelectorAll('.filter-menu-item').length);
  });

  test('closes filter menu when clicking outside', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();

    const filterType = filterBar.shadowRoot.querySelector('type-dialog');
    assertNotEquals(null, filterType);

    // Click outside of the dialog.
    filterBar.click();

    await microtasksFinished();
    assertEquals(null, filterBar.shadowRoot.querySelector('type-dialog'));
  });

  async function addFilter(
      filterTypeIndex: number,
      itemSelection: (content: HTMLElement) => Promise<void>,
  ) {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    const filterType = filterBar.shadowRoot.querySelector('type-dialog')!;
    const filterMenuItems = filterType.shadowRoot.querySelectorAll<HTMLElement>(
        '.filter-menu-item');
    filterMenuItems[filterTypeIndex]!.click();
    await microtasksFinished();

    const content = filterBar.shadowRoot.querySelector<HTMLElement>(
        'app-dialog, event-dialog, outcome-dialog, scope-dialog, date-dialog')!;
    assertNotEquals(null, content);

    await itemSelection(content);
    const footerElement =
        content.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(undefined, footerElement);
    assertNotEquals(null, footerElement);
    const applyButton =
        footerElement!.shadowRoot?.querySelector<CrButtonElement>(
            '.action-button');
    applyButton!.click();
    await microtasksFinished();
  }

  async function clearFilters() {
    const clearButton =
        filterBar.shadowRoot.getElementById('clear-filters-button');
    if (clearButton) {
      clearButton.click();
      await microtasksFinished();
    }
  }

  test('can add and remove app filter', async () => {
    await clearFilters();
    assertEquals(0, filterBar.shadowRoot.querySelectorAll('.chip').length);

    let capturedEvent: Event|null = null;
    const onFiltersChanged = (e: Event) => {
      capturedEvent = e;
    };
    filterBar.addEventListener('filters-changed', onFiltersChanged);

    await addFilter(0, async (content) => {
      assertEquals('APP-DIALOG', content.tagName);
      content.shadowRoot!.querySelector<HTMLElement>('cr-checkbox')!.click();
      await microtasksFinished();
    });

    assertNotEquals(null, capturedEvent);
    assertEquals(1, filterBar.filterSettings.apps.size);
    assertTrue(capturedEvent!.bubbles);
    assertTrue(capturedEvent!.composed);

    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector<HTMLElement>('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    assertEquals(0, filterBar.filterSettings.apps.size);
  });

  test('can add and remove event type filter', async () => {
    await clearFilters();
    await addFilter(1, async (content) => {
      assertEquals('EVENT-DIALOG', content.tagName);
      const checkbox =
          content.shadowRoot!.querySelector<CrCheckboxElement>('cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
    });
    assertEquals(1, filterBar.filterSettings.eventTypes.size);
    assertTrue(filterBar.filterSettings.eventTypes.has('UPDATE'));
  });

  test('can add and remove update outcome filter', async () => {
    await clearFilters();
    await addFilter(2, async (content) => {
      assertEquals('OUTCOME-DIALOG', content.tagName);
      const checkbox =
          content.shadowRoot!.querySelector<CrCheckboxElement>('cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
    });
    assertEquals(1, filterBar.filterSettings.updateOutcomes.size);
    assertTrue(filterBar.filterSettings.updateOutcomes.has('UPDATED'));
  });

  test('can add and remove scope filter', async () => {
    await clearFilters();
    await addFilter(3, async (content) => {
      assertEquals('SCOPE-DIALOG', content.tagName);
      const checkbox =
          content.shadowRoot!.querySelector<CrCheckboxElement>('cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
    });
    assertEquals(1, filterBar.filterSettings.scopes.size);
    assertTrue(filterBar.filterSettings.scopes.has('USER'));
  });

  test('can add and remove date filter', async () => {
    await clearFilters();
    await addFilter(4, async (content) => {
      assertEquals('DATE-DIALOG', content.tagName);
      const startDateInput =
          content.shadowRoot!.querySelector<HTMLInputElement>('#start-date');
      startDateInput!.valueAsNumber = new Date('2025-01-01T00:00').getTime();
      startDateInput!.dispatchEvent(new Event('input'));
      await microtasksFinished();
    });
    assertDeepEquals(
        new Date('2025-01-01T00:00'), filterBar.filterSettings.startDate);
  });

  test('clears all filters when clear button is clicked', async () => {
    filterBar.filterSettings = {
      apps: new Set(['Google Chrome']),
      eventTypes: new Set(['INSTALL']),
      updateOutcomes: new Set(['UPDATED']),
      scopes: new Set(['USER']),
      startDate: new Date(),
      endDate: new Date(),
    };
    await microtasksFinished();
    await clearFilters();
    assertEquals(0, filterBar.filterSettings.apps.size);
    assertEquals(0, filterBar.filterSettings.eventTypes.size);
    assertEquals(0, filterBar.filterSettings.updateOutcomes.size);
    assertEquals(0, filterBar.filterSettings.scopes.size);
    assertEquals(null, filterBar.filterSettings.startDate);
    assertEquals(null, filterBar.filterSettings.endDate);
    assertEquals(
        null, filterBar.shadowRoot.getElementById('clear-filters-button'));
  });

  test('renders filter chips correctly', async () => {
    filterBar.filterSettings = {
      apps: new Set(['App1', 'App2']),
      eventTypes: new Set(['INSTALL', 'UPDATE']),
      updateOutcomes: new Set(['UPDATED']),
      scopes: new Set(['USER']),
      startDate: new Date('2025-01-01T00:00'),
      endDate: new Date('2025-01-02T00:00'),
    };
    await microtasksFinished();
    const chips = filterBar.shadowRoot.querySelectorAll('.chip');
    assertEquals(5, chips.length);
    assertStringContains(chips[0]!.textContent, 'App: App1, App2');
    assertStringContains(chips[1]!.textContent, 'Event Type: Install, Update');
    assertStringContains(chips[2]!.textContent, 'Update Outcome: Updated');
    assertStringContains(
        chips[3]!.textContent, 'Updater Scope: Per-user updater');
    assertStringContains(chips[4]!.textContent, '01/01/2025');
  });

  test('edits date filter when date chip is clicked', async () => {
    filterBar.filterSettings = {
      apps: new Set(),
      eventTypes: new Set(),
      updateOutcomes: new Set(),
      scopes: new Set(),
      startDate: new Date('2025-01-01T00:00'),
      endDate: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector<HTMLElement>('.chip')!;
    chip.click();
    await microtasksFinished();
    const filterDate = filterBar.shadowRoot.querySelector('date-dialog');
    assertNotEquals(null, filterDate);
    assertNotEquals(
        null,
        filterDate!.shadowRoot.querySelector<HTMLInputElement>('#start-date'));
  });

  test('removes date filter when remove button is clicked', async () => {
    filterBar.filterSettings = {
      apps: new Set(),
      eventTypes: new Set(),
      updateOutcomes: new Set(),
      scopes: new Set(),
      startDate: new Date('2025-01-01T00:00'),
      endDate: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector<HTMLElement>('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    assertEquals(null, filterBar.filterSettings.startDate);
  });

  test('cancel button closes dialog and does not apply changes', async () => {
    await clearFilters();
    assertEquals(0, filterBar.filterSettings.apps.size);

    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();

    const filterType = filterBar.shadowRoot.querySelector('type-dialog')!;
    const filterMenuItems = filterType.shadowRoot.querySelectorAll<HTMLElement>(
        '.filter-menu-item');
    filterMenuItems[0]!.click();  // Open app filter
    await microtasksFinished();

    const filterApp = filterBar.shadowRoot.querySelector('app-dialog')!;
    // Select an app
    filterApp.shadowRoot.querySelector<HTMLElement>('cr-checkbox')!.click();
    await microtasksFinished();

    // Click cancel button
    const footerElement =
        filterApp.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(undefined, footerElement);
    assertNotEquals(null, footerElement);
    const cancelButton =
        footerElement!.shadowRoot?.querySelector<CrButtonElement>(
            '.cancel-button');
    cancelButton!.click();
    await microtasksFinished();

    // Expect dialog to be closed and no changes applied to filterSettings
    assertEquals(null, filterBar.shadowRoot.querySelector('app-dialog'));
    assertEquals(0, filterBar.filterSettings.apps.size);
  });
});
