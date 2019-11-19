// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, getSelectDropdownBackground} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getGoogleDriveDestination, selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.destination_select_test = {};
destination_select_test.suiteName = 'DestinationSelectTest';
/** @enum {string} */
destination_select_test.TestNames = {
  ChangeIcon: 'change icon',
};

suite(destination_select_test.suiteName, function() {
  /** @type {?PrintPreviewDestinationSelectElement} */
  let destinationSelect = null;

  const account = 'foo@chromium.org';

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    destinationSelect =
        document.createElement('print-preview-destination-select');
    destinationSelect.activeUser = account;
    destinationSelect.appKioskMode = false;
    destinationSelect.disabled = false;
    destinationSelect.noDestinations = false;
    destinationSelect.recentDestinationList = [
      // Local printer without stickied icon
      {
        id: 'ID1',
        origin: DestinationOrigin.LOCAL,
        account: '',
        capabilities: null,
        displayName: 'One',
        extensionId: '',
        extensionName: ''
      },
      // Shared cloud printer with stickied icon
      {
        id: 'ID2',
        origin: DestinationOrigin.COOKIES,
        account: account,
        capabilities: null,
        displayName: 'Two',
        extensionId: '',
        extensionName: '',
        icon: 'print-preview:printer-shared'
      },
      // Shared cloud printer without stickied icon
      {
        id: 'ID3',
        origin: DestinationOrigin.COOKIES,
        account: account,
        capabilities: null,
        displayName: 'Three',
        extensionId: '',
        extensionName: ''
      },
    ];

    document.body.appendChild(destinationSelect);
  });

  function compareIcon(selectEl, expectedIcon) {
    const icon = selectEl.style['background-image'].replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        destinationSelect.meta_.byKey('print-preview'), expectedIcon,
        destinationSelect);
    assertEquals(expected, icon);
  }

  test(assert(destination_select_test.TestNames.ChangeIcon), function() {
    const destination = new Destination(
        'ID1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
        DestinationConnectionStatus.ONLINE);
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    const selectEl = destinationSelect.$$('.md-select');
    compareIcon(selectEl, 'print');
    const driveId = Destination.GooglePromotedId.DOCS;
    const cookieOrigin = DestinationOrigin.COOKIES;

    return selectOption(
               destinationSelect, `${driveId}/${cookieOrigin}/${account}`)
        .then(() => {
          // Icon updates early based on the ID.
          compareIcon(selectEl, 'save-to-drive');

          // Update the destination.
          destinationSelect.destination = getGoogleDriveDestination(account);

          // Still Save to Drive icon.
          compareIcon(selectEl, 'save-to-drive');

          // Select a destination that has a sticky icon value.
          return selectOption(
              destinationSelect, `ID2/${cookieOrigin}/${account}`);
        })
        .then(() => {
          // Should already be updated.
          compareIcon(selectEl, 'printer-shared');

          // Update destination.
          destinationSelect.destination = new Destination(
              'ID2', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Two',
              DestinationConnectionStatus.ONLINE, {account: account});
          compareIcon(selectEl, 'printer-shared');

          // Select a destination that doesn't have a sticky icon value.
          return selectOption(
              destinationSelect, `ID3/${cookieOrigin}/${account}`);
        })
        .then(() => {
          // Falls back to normal printer icon.
          compareIcon(selectEl, 'print');

          // Update destination.
          destinationSelect.destination = new Destination(
              'ID3', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Three',
              DestinationConnectionStatus.ONLINE, {account: account});

          // Icon updates based on full destination information.
          compareIcon(selectEl, 'printer-shared');
        });
  });
});
