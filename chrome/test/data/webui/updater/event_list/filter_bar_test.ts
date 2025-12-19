// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {expect} from '//webui-test/chai.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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
    const filterType = filterBar.shadowRoot.querySelector('type-dialog');
    expect(filterType).to.not.be.null;

    expect(filterType!.shadowRoot.querySelectorAll('.filter-menu-item').length)
        .to.equal(4);
  });

  test('closes filter menu when clicking outside', async () => {
    const addFilterButton =
        filterBar.shadowRoot.getElementById('add-filter-button')!;
    addFilterButton.click();
    await microtasksFinished();

    const filterType = filterBar.shadowRoot.querySelector('type-dialog');
    expect(filterType).to.not.be.null;

    // Click outside of the dialog.
    filterBar.click();

    await microtasksFinished();
    expect(filterBar.shadowRoot.querySelector('type-dialog')).to.be.null;
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
        'app-dialog, event-dialog, outcome-dialog, date-dialog')!;
    expect(content).to.not.be.null;

    await itemSelection(content);
    const footerElement =
        content.shadowRoot?.querySelector('filter-dialog-footer');
    expect(footerElement).to.not.be.null;
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
    expect(filterBar.shadowRoot.querySelectorAll('.chip').length).to.equal(0);

    let capturedEvent: Event|null = null;
    const onFiltersChanged = (e: Event) => {
      capturedEvent = e;
    };
    filterBar.addEventListener('filters-changed', onFiltersChanged);

    await addFilter(0, async (content) => {
      expect(content.tagName).to.equal('APP-DIALOG');
      content.shadowRoot!.querySelector<HTMLElement>('cr-checkbox')!.click();
      await microtasksFinished();
    });

    expect(capturedEvent).to.not.be.null;
    expect(filterBar.filterSettings.apps.size).to.equal(1);
    expect(capturedEvent!.bubbles).to.be.true;
    expect(capturedEvent!.composed).to.be.true;

    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector<HTMLElement>('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    expect(filterBar.filterSettings.apps.size).to.equal(0);
  });

  test('can add and remove event type filter', async () => {
    await clearFilters();
    await addFilter(1, async (content) => {
      expect(content.tagName).to.equal('EVENT-DIALOG');
      const checkbox =
          content.shadowRoot!.querySelector<CrCheckboxElement>('cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.eventTypes.size).to.equal(1);
    expect(
        filterBar.filterSettings.eventTypes.has('UPDATE'),
        )
        .to.be.true;
  });

  test('can add and remove update outcome filter', async () => {
    await clearFilters();
    await addFilter(2, async (content) => {
      expect(content.tagName).to.equal('OUTCOME-DIALOG');
      const checkbox =
          content.shadowRoot!.querySelector<CrCheckboxElement>('cr-checkbox');
      checkbox!.click();
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.updateOutcomes.size).to.equal(1);
    expect(filterBar.filterSettings.updateOutcomes.has('UPDATED')).to.be.true;
  });

  test('can add and remove date filter', async () => {
    await clearFilters();
    await addFilter(3, async (content) => {
      expect(content.tagName).to.equal('DATE-DIALOG');
      const startDateInput =
          content.shadowRoot!.querySelector<HTMLInputElement>('#start-date');
      startDateInput!.valueAsNumber = new Date('2025-01-01T00:00').getTime();
      startDateInput!.dispatchEvent(new Event('input'));
      await microtasksFinished();
    });
    expect(filterBar.filterSettings.startDate)
        .to.deep.equal(
            new Date('2025-01-01T00:00'),
        );
  });

  test('clears all filters when clear button is clicked', async () => {
    filterBar.filterSettings = {
      apps: new Set(['Google Chrome']),
      eventTypes: new Set(['INSTALL']),
      updateOutcomes: new Set(['UPDATED']),
      startDate: new Date(),
      endDate: new Date(),
    };
    await microtasksFinished();
    await clearFilters();
    expect(filterBar.filterSettings.apps.size).to.equal(0);
    expect(filterBar.filterSettings.eventTypes.size).to.equal(0);
    expect(filterBar.filterSettings.updateOutcomes.size).to.equal(0);
    expect(filterBar.filterSettings.startDate).to.be.null;
    expect(filterBar.filterSettings.endDate).to.be.null;
    expect(filterBar.shadowRoot.getElementById('clear-filters-button'))
        .to.be.null;
  });

  test('renders filter chips correctly', async () => {
    filterBar.filterSettings = {
      apps: new Set(['App1', 'App2']),
      eventTypes: new Set(['INSTALL', 'UPDATE']),
      updateOutcomes: new Set(['UPDATED']),
      startDate: new Date('2025-01-01T00:00'),
      endDate: new Date('2025-01-02T00:00'),
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
      apps: new Set(),
      eventTypes: new Set(),
      updateOutcomes: new Set(),
      startDate: new Date('2025-01-01T00:00'),
      endDate: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector<HTMLElement>('.chip')!;
    chip.click();
    await microtasksFinished();
    const filterDate = filterBar.shadowRoot.querySelector('date-dialog');
    expect(filterDate).to.not.be.null;
    expect(
        filterDate!.shadowRoot.querySelector<HTMLInputElement>('#start-date'),
        )
        .to.not.be.null;
  });

  test('removes date filter when remove button is clicked', async () => {
    filterBar.filterSettings = {
      apps: new Set(),
      eventTypes: new Set(),
      updateOutcomes: new Set(),
      startDate: new Date('2025-01-01T00:00'),
      endDate: null,
    };
    await microtasksFinished();
    const chip = filterBar.shadowRoot.querySelector('.chip')!;
    const removeButton = chip.querySelector<HTMLElement>('cr-icon-button')!;
    removeButton.click();
    await microtasksFinished();
    expect(filterBar.filterSettings.startDate).to.be.null;
  });

  test('cancel button closes dialog and does not apply changes', async () => {
    await clearFilters();
    expect(filterBar.filterSettings.apps.size).to.equal(0);

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
    expect(footerElement).to.not.be.null;
    const cancelButton =
        footerElement!.shadowRoot?.querySelector<CrButtonElement>(
            '.cancel-button');
    cancelButton!.click();
    await microtasksFinished();

    // Expect dialog to be closed and no changes applied to filterSettings
    expect(filterBar.shadowRoot.querySelector('app-dialog')).to.be.null;
    expect(filterBar.filterSettings.apps.size).to.equal(0);
  });
});
