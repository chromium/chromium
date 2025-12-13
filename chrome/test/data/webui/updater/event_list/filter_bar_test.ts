// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {expect} from '//webui-test/chai.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {FilterSettings} from 'chrome://updater/event_list/filter_bar.js';
import {FilterBarElement} from 'chrome://updater/event_list/filter_bar.js';
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
    expect(filterBar instanceof HTMLElement).to.be.true;
    expect(filterBar.tagName).to.equal('FILTER-BAR');
  });

  test('initializes with default filters', () => {
    const chips = filterBar.shadowRoot.querySelectorAll('.chip');
    expect(chips.length).to.equal(3);
    expect(chips[0]!.textContent)
        .to.contain('App: Google Chrome, Google Chrome Beta');
    expect(chips[1]!.textContent)
        .to.contain('Event Type: Install, Update, Uninstall');
    expect(chips[2]!.textContent)
        .to.contain('Update Outcome: Updated, Update Error');
  });

  test('opens filter menu when add filter button is clicked', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog');
    expect(filterDialog).to.not.be.null;
    expect(
        filterDialog!.shadowRoot.querySelectorAll('.filter-menu-item').length)
        .to.equal(4);
  });

  test('closes filter menu when Escape is pressed', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!
                             .shadowRoot.querySelector('div')!;
    filterDialog.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    await microtasksFinished();
    expect(filterBar.shadowRoot.querySelector('filter-dialog')).to.be.null;
  });

  test('closes filter menu when clicking outside', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    expect(filterBar.shadowRoot.querySelector('filter-dialog')).to.not.be.null;
    document.body.click();
    await microtasksFinished();
    expect(filterBar.shadowRoot.querySelector('filter-dialog')).to.be.null;
  });

  async function addFilter(
      filterTypeIndex: number,
      itemSelection: () => Promise<void>,
  ) {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();
    const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
    const filterMenuItems =
        filterDialog.shadowRoot.querySelectorAll<HTMLElement>(
            '.filter-menu-item');
    filterMenuItems[filterTypeIndex]!.click();
    await microtasksFinished();
    await itemSelection();
    const applyButton = filterDialog.shadowRoot.querySelector<CrButtonElement>(
        '.filter-menu-footer .action-button');
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
    expect(filterBar.shadowRoot.querySelectorAll('.chip').length).to.equal(0);

    let capturedEvent: CustomEvent<FilterSettings>|null = null;
    const onFiltersChanged = (e: Event) => {
      capturedEvent = e as CustomEvent<FilterSettings>;
    };
    filterBar.addEventListener('filters-changed', onFiltersChanged);

    await addFilter(0, async () => {
      const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
      filterDialog.shadowRoot.querySelector('cr-checkbox')!.click();
      await microtasksFinished();
      await microtasksFinished();
    });

    expect(filterBar.filterSettings.activeAppFilters.size).to.equal(1);

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail).to.deep.equal(filterBar.filterSettings);
    expect(capturedEvent!.bubbles).to.be.true;
    expect(capturedEvent!.composed).to.be.true;

    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    expect(filterBar.filterSettings.activeAppFilters.size).to.equal(0);
  });

  test('can add and remove event type filter', async () => {
    await clearFilters();
    await addFilter(1, async () => {
      const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
      const checkbox = filterDialog.shadowRoot.querySelector<CrCheckboxElement>(
          'cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.activeEventTypeFilters.size).to.equal(1);
    expect(
        filterBar.filterSettings.activeEventTypeFilters.has('UPDATE'),
        )
        .to.be.true;
  });

  test('can add and remove update outcome filter', async () => {
    await clearFilters();
    await addFilter(2, async () => {
      const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
      const checkbox = filterDialog.shadowRoot.querySelector<CrCheckboxElement>(
          'cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.activeUpdateOutcomeFilters.size)
        .to.equal(1);
    expect(filterBar.filterSettings.activeUpdateOutcomeFilters.has('UPDATED'))
        .to.be.true;
  });

  test('can add and remove date filter', async () => {
    await clearFilters();
    await addFilter(3, async () => {
      const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
      const startDateInput =
          filterDialog.shadowRoot.querySelector<HTMLInputElement>(
              '#start-date');
      startDateInput!.valueAsNumber = new Date('2025-01-01T00:00').getTime();
      startDateInput!.dispatchEvent(new Event('input'));
      await microtasksFinished();
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.startDateFilter)
        .to.deep.equal(
            new Date('2025-01-01T00:00'),
        );
  });

  test('clears all filters when clear button is clicked', async () => {
    filterBar.filterSettings = {
      activeAppFilters: new Set(['Google Chrome']),
      activeEventTypeFilters: new Set(['INSTALL']),
      activeUpdateOutcomeFilters: new Set(['UPDATED']),
      startDateFilter: new Date(),
      endDateFilter: new Date(),
    };
    await microtasksFinished();
    await clearFilters();
    expect(filterBar.filterSettings.activeAppFilters.size).to.equal(0);
    expect(filterBar.filterSettings.activeEventTypeFilters.size).to.equal(0);
    expect(filterBar.filterSettings.activeUpdateOutcomeFilters.size)
        .to.equal(0);
    expect(filterBar.filterSettings.startDateFilter).to.be.null;
    expect(filterBar.filterSettings.endDateFilter).to.be.null;
    expect(filterBar.shadowRoot.getElementById('clear-filters-button'))
        .to.be.null;
  });

  test('renders filter chips correctly', async () => {
    filterBar.filterSettings = {
      activeAppFilters: new Set(['App1', 'App2']),
      activeEventTypeFilters: new Set(['INSTALL', 'UPDATE']),
      activeUpdateOutcomeFilters: new Set(['UPDATED']),
      startDateFilter: new Date('2025-01-01T00:00'),
      endDateFilter: new Date('2025-01-02T00:00'),
    };
    await microtasksFinished();
    const chips = filterBar.shadowRoot.querySelectorAll('.chip');
    expect(chips.length).to.equal(4);
    expect(chips[0]!.textContent).to.contain('App: App1, App2');
    expect(chips[1]!.textContent).to.contain('Event Type: Install, Update');
    expect(chips[2]!.textContent).to.contain('Update Outcome: Updated');
    expect(chips[3]!.textContent).to.contain('01/01/2025');
  });

  test('edits date filter when date chip is clicked', async () => {
    filterBar.filterSettings = {
      activeAppFilters: new Set(),
      activeEventTypeFilters: new Set(),
      activeUpdateOutcomeFilters: new Set(),
      startDateFilter: new Date('2025-01-01T00:00'),
      endDateFilter: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector<HTMLElement>('.chip')!;
    chip.click();
    await microtasksFinished();
    const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
    expect(filterDialog).to.not.be.null;
    expect(
        filterDialog.shadowRoot.querySelector<HTMLInputElement>('#start-date'),
        )
        .to.not.be.null;
  });

  test('removes date filter when remove button is clicked', async () => {
    filterBar.filterSettings = {
      activeAppFilters: new Set(),
      activeEventTypeFilters: new Set(),
      activeUpdateOutcomeFilters: new Set(),
      startDateFilter: new Date('2025-01-01T00:00'),
      endDateFilter: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    expect(filterBar.filterSettings.startDateFilter).to.be.null;
  });

  test('cancel button closes dialog and does not apply changes', async () => {
    await clearFilters();
    expect(filterBar.filterSettings.activeAppFilters.size).to.equal(0);

    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();

    const filterDialog = filterBar.shadowRoot.querySelector('filter-dialog')!;
    const filterMenuItems =
        filterDialog.shadowRoot.querySelectorAll<HTMLElement>(
            '.filter-menu-item');
    filterMenuItems[0]!.click();  // Open app filter
    await microtasksFinished();

    // Select an app
    filterDialog.shadowRoot.querySelector<CrCheckboxElement>(
                               'cr-checkbox')!.click();
    await microtasksFinished();

    // Click cancel button
    const cancelButton = filterDialog.shadowRoot.querySelector<CrButtonElement>(
        '.filter-menu-footer .cancel-button');
    cancelButton!.click();
    await microtasksFinished();

    // Expect dialog to be closed and no changes applied to filterSettings
    expect(filterBar.shadowRoot.querySelector('filter-dialog')).to.be.null;
    expect(filterBar.filterSettings.activeAppFilters.size).to.equal(0);
  });

  test(
      'pressing Enter on App filter option opens app filter dialog',
      async () => {
        const addFilterButton =
            filterBar.shadowRoot.getElementById('add-filter-button')!;
        addFilterButton.click();
        await microtasksFinished();

        const filterDialog =
            filterBar.shadowRoot.querySelector('filter-dialog')!;
        const filterMenuItems =
            filterDialog.shadowRoot.querySelectorAll<HTMLElement>(
                '.filter-menu-item');

        // The first item is 'App' based on filterMenuItems order
        filterMenuItems[0]!.focus();
        filterMenuItems[0]!.dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Enter'}));
        await microtasksFinished();

        expect(filterDialog).to.not.be.null;
        // Assuming 'app' is the correct type value for the app filter dialog
        expect(filterDialog.type).to.equal('app');
      });
});
