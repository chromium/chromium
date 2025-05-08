// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewDestinationSelectElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, getSelectDropdownBackground, IconsetMap} from 'chrome://print/print_preview.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('DestinationSelectTest', function() {
  let destinationSelect: PrintPreviewDestinationSelectElement;

  const recentDestinationList: Destination[] = [
    new Destination('ID1', DestinationOrigin.LOCAL, 'One'),
    new Destination(
        'ID4', DestinationOrigin.LOCAL, 'Four', {isEnterprisePrinter: true}),
  ];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    destinationSelect =
        document.createElement('print-preview-destination-select');
    destinationSelect.disabled = false;
    destinationSelect.loaded = false;
    destinationSelect.noDestinations = false;
    destinationSelect.recentDestinationList = recentDestinationList;

    document.body.appendChild(destinationSelect);
  });

  function compareIcon(selectEl: HTMLSelectElement, expectedIcon: string) {
    const icon =
        selectEl.style.getPropertyValue('background-image').replace(/ /gi, '');
    const expected = getSelectDropdownBackground(
        IconsetMap.getInstance().get('print-preview')!, expectedIcon,
        destinationSelect);
    assertEquals(expected, icon);
  }

  test('change icon', async function() {
    destinationSelect.recentDestinationList = recentDestinationList;

    const destination = recentDestinationList[0]!;
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    destinationSelect.loaded = true;
    await microtasksFinished();
    const selectEl =
        destinationSelect.shadowRoot.querySelector<HTMLSelectElement>(
            '.md-select')!;
    compareIcon(selectEl, 'print');

    // Select a destination with the enterprise printer icon.
    await selectOption(destinationSelect, `ID4/local/`);
    const enterpriseIcon = 'business';
    compareIcon(selectEl, enterpriseIcon);

    // Update destination.
    destinationSelect.destination = recentDestinationList[1]!;
    await microtasksFinished();
    compareIcon(selectEl, enterpriseIcon);
  });

  test('ShowsSelectedDestination', async function() {
    const select = destinationSelect.shadowRoot.querySelector('select');
    assertTrue(!!select);

    const destination = recentDestinationList[0]!;
    destinationSelect.loaded = true;
    destinationSelect.destination = destination;
    destinationSelect.updateDestination();
    await microtasksFinished();

    assertEquals(destination.key, select.value);
    assertEquals(destination.key, destinationSelect.selectedValue);

    const newDestination =
        new Destination('ID2', DestinationOrigin.LOCAL, 'Two');
    destinationSelect.recentDestinationList = [newDestination];
    destinationSelect.destination = newDestination;
    destinationSelect.updateDestination();
    await microtasksFinished();

    assertEquals(newDestination.key, select.value);
    assertEquals(newDestination.key, destinationSelect.selectedValue);
  });
});
