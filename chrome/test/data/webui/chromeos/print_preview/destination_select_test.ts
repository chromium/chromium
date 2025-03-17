// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationSelectElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, getSelectDropdownBackground, IconsetMap} from 'chrome://print/print_preview.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('DestinationSelectTest', function() {
  let destinationSelect: PrintPreviewDestinationSelectElement;

  let recentDestinationList: Destination[] = [];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
      new Destination('ID1', DestinationOrigin.LOCAL, 'One'),
      new Destination(
          'ID4', DestinationOrigin.LOCAL, 'Four', {isEnterprisePrinter: true}),
    ];
  }

  function compareIcon(selectEl: HTMLSelectElement, expectedIcon: string) {
    const icon =
        selectEl.style.getPropertyValue('background-image').replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        IconsetMap.getInstance().get('print-preview')!, expectedIcon,
        destinationSelect);
    assertEquals(expected, icon);
  }

  test('change icon', function() {
    populateRecentDestinationList();
    destinationSelect.recentDestinationList = recentDestinationList;

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
      destinationSelect.destination = recentDestinationList[1]!;
      compareIcon(selectEl, enterpriseIcon);
    });
  });
});
