// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, getSelectDropdownBackground, IronMeta, PrintPreviewDestinationSelectElement} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

const destination_select_test = {
  suiteName: 'DestinationSelectTest',
  TestNames: {
    UpdateStatus: 'update status',
    ChangeIcon: 'change icon',
  },
};

Object.assign(window, {destination_select_test: destination_select_test});

suite(destination_select_test.suiteName, function() {
  let destinationSelect: PrintPreviewDestinationSelectElement;

  let recentDestinationList: Destination[] = [];

  const meta = new IronMeta({type: 'iconset', value: undefined});

  setup(function() {
    document.body.innerHTML = '';
    destinationSelect =
        document.createElement('print-preview-destination-select');
    destinationSelect.disabled = false;
    destinationSelect.loaded = false;
    destinationSelect.noDestinations = false;
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    document.body.appendChild(destinationSelect);
    return waitAfterNextRender(destinationSelect);
  });

  // Create three different destinations and use them to populate
  // |recentDestinationList|.
  function populateRecentDestinationList() {
    recentDestinationList = [
      new Destination(
          'ID1', DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE),
      new Destination(
          'ID2', DestinationOrigin.EXTENSION, 'Two',
          DestinationConnectionStatus.OFFLINE,
          {extensionId: '222', extensionName: 'Extension2'}),
      new Destination(
          'ID4', DestinationOrigin.LOCAL, 'Four',
          DestinationConnectionStatus.ONLINE, {isEnterprisePrinter: true}),
    ];
  }

  function compareIcon(selectEl: HTMLSelectElement, expectedIcon: string) {
    const icon =
        selectEl.style.getPropertyValue('background-image').replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        meta.byKey('print-preview'), expectedIcon, destinationSelect);
    assertEquals(expected, icon);
  }

  /**
   * Test that changing different destinations results in the correct icon being
   * shown.
   * @return Promise that resolves when the test finishes.
   */
  function testChangeIcon(): Promise<void> {
    const destination = recentDestinationList[0]!;
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    destinationSelect.loaded = true;
    const selectEl =
        destinationSelect.shadowRoot!.querySelector<HTMLSelectElement>(
            '.md-select')!;
    compareIcon(selectEl, 'print');

    // Select a destination with the enterprise printer icon.
    return selectOption(destinationSelect, `ID4/local/`).then(() => {
      const enterpriseIcon = 'business';

      compareIcon(selectEl, enterpriseIcon);

      // Update destination.
      destinationSelect.destination = recentDestinationList[2]!;
      compareIcon(selectEl, enterpriseIcon);
    });
  }

  /**
   * Test that changing different destinations results in the correct status
   * being shown.
   */
  function testUpdateStatus() {
    loadTimeData.overrideValues({
      offline: 'offline',
    });

    assertFalse(destinationSelect.shadowRoot!
                    .querySelector<HTMLElement>('.throbber-container')!.hidden);
    assertTrue(destinationSelect.shadowRoot!
                   .querySelector<HTMLSelectElement>('.md-select')!.hidden);

    destinationSelect.loaded = true;
    assertTrue(destinationSelect.shadowRoot!
                   .querySelector<HTMLElement>('.throbber-container')!.hidden);
    assertFalse(destinationSelect.shadowRoot!
                    .querySelector<HTMLSelectElement>('.md-select')!.hidden);

    const additionalInfoEl =
        destinationSelect.shadowRoot!.querySelector<HTMLElement>(
            '.destination-additional-info')!;
    const statusEl = destinationSelect.shadowRoot!.querySelector<HTMLElement>(
        '.destination-status')!;

    destinationSelect.destination = recentDestinationList[0]!;
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[1]!;
    destinationSelect.updateDestination();
    assertFalse(additionalInfoEl.hidden);
    assertEquals('offline', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[2]!;
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);
  }

  test(assert(destination_select_test.TestNames.UpdateStatus), function() {
    populateRecentDestinationList();
    return testUpdateStatus();
  });

  test(assert(destination_select_test.TestNames.ChangeIcon), function() {
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

    return testChangeIcon();
  });
});
