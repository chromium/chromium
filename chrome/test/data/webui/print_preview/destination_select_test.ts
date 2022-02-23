// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, getSelectDropdownBackground, IronMeta, PrintPreviewDestinationSelectElement} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {getGoogleDriveDestination, selectOption} from './print_preview_test_utils.js';

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

  const account: string = 'foo@chromium.org';

  const cookieOrigin: DestinationOrigin = DestinationOrigin.COOKIES;

  let recentDestinationList: Destination[] = [];

  const meta = new IronMeta({type: 'iconset', value: undefined});

  setup(function() {
    document.body.innerHTML = '';
    destinationSelect =
        document.createElement('print-preview-destination-select');
    destinationSelect.activeUser = account;
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
          'ID1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE),
      getGoogleDriveDestination(account),
      new Destination(
          'ID2', DestinationType.GOOGLE, cookieOrigin, 'Two',
          DestinationConnectionStatus.OFFLINE, {account: account}),
      new Destination(
          'ID3', DestinationType.GOOGLE, cookieOrigin, 'Three',
          DestinationConnectionStatus.ONLINE,
          {account: account, isOwned: true}),
      new Destination(
          'ID4', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'Four',
          DestinationConnectionStatus.ONLINE, {isEnterprisePrinter: true}),
      new Destination(
          'ID5', DestinationType.MOBILE, cookieOrigin, 'Five',
          DestinationConnectionStatus.ONLINE),
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

    return selectOption(destinationSelect, recentDestinationList[1]!.key)
        .then(() => {
          // Icon updates early based on the ID.
          compareIcon(selectEl, 'save-to-drive');

          // Update the destination.
          destinationSelect.destination = recentDestinationList[1]!;

          // Still Save to Drive icon.
          compareIcon(selectEl, 'save-to-drive');

          // Select a destination with the shared printer icon.
          return selectOption(
              destinationSelect, `ID2/${cookieOrigin}/${account}`);
        })
        .then(() => {
          // Should already be updated.
          compareIcon(selectEl, 'printer-shared');

          // Update destination.
          destinationSelect.destination = recentDestinationList[2]!;
          compareIcon(selectEl, 'printer-shared');

          // Select a destination with a standard printer icon.
          return selectOption(
              destinationSelect, `ID3/${cookieOrigin}/${account}`);
        })
        .then(() => {
          compareIcon(selectEl, 'print');

          // Update destination.
          destinationSelect.destination = recentDestinationList[3]!;
          compareIcon(selectEl, 'print');

          // Select a destination with the enterprise printer icon.
          return selectOption(destinationSelect, `ID4/local/`);
        })
        .then(() => {
          const enterpriseIcon = 'business';

          compareIcon(selectEl, enterpriseIcon);

          // Update destination.
          destinationSelect.destination = recentDestinationList[4]!;
          compareIcon(selectEl, enterpriseIcon);

          // Select a destination with the mobile printer icon.
          return selectOption(destinationSelect, `ID5/${cookieOrigin}/`);
        })
        .then(() => {
          const mobileIcon = 'smartphone';

          compareIcon(selectEl, mobileIcon);

          // Update destination.
          destinationSelect.destination = recentDestinationList[5]!;
          compareIcon(selectEl, mobileIcon);
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

    destinationSelect.destination = recentDestinationList[1]!;
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[0]!;
    destinationSelect.updateDestination();
    assertTrue(additionalInfoEl.hidden);
    assertEquals('', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[2]!;
    destinationSelect.updateDestination();
    assertFalse(additionalInfoEl.hidden);
    assertEquals('offline', statusEl.innerHTML);

    destinationSelect.destination = recentDestinationList[3]!;
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
