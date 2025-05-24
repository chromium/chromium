// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('DestinationItemTest', function() {
  let item: PrintPreviewDestinationListItemElement;

  const printerId: string = 'FooDevice';

  const printerName: string = 'FooName';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('print-preview-destination-list-item');

    // Create destination
    item.destination = new Destination(
        printerId, DestinationOrigin.EXTENSION, printerName,
        {extensionId: 'aaa111', extensionName: 'myPrinterExtension'});
    item.searchQuery = null;
    document.body.appendChild(item);
  });

  // Test that the destination is displayed correctly for the basic case of a
  // destination with no search query.
  test('NoQuery', function() {
    const name = item.shadowRoot!.querySelector('.name')!;
    assertEquals(printerName, name.textContent);
    assertEquals('1', window.getComputedStyle(name).opacity);
    assertEquals(
        '',
        item.shadowRoot!.querySelector('.search-hint')!.textContent!.trim());
    assertFalse(item.shadowRoot!
                    .querySelector<HTMLElement>(
                        '.extension-controlled-indicator')!.hidden);
  });

  // Test that the destination is displayed correctly when the search query
  // matches its display name.
  test('QueryName', function() {
    item.searchQuery = /(Foo)/ig;

    const name = item.shadowRoot!.querySelector('.name')!;
    assertEquals(printerName + printerName, name.textContent);

    // Name should be highlighted.
    const searchHits = name.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('Foo', searchHits[0]!.textContent);

    // No hints.
    assertEquals(
        '',
        item.shadowRoot!.querySelector('.search-hint')!.textContent!.trim());
  });

  // Test that the destination is displayed correctly when the search query
  // matches its description.
  test('QueryDescription', function() {
    const params = {
      description: 'ABCPrinterBrand Model 123',
      location: 'Building 789 Floor 6',
      extensionId: 'aaa111',
      extensionName: 'myPrinterExtension',
    };
    item.destination = new Destination(
        printerId, DestinationOrigin.EXTENSION, printerName, params);
    item.searchQuery = /(ABC)/ig;

    // No highlighting on name.
    const name = item.shadowRoot!.querySelector('.name')!;
    assertEquals(printerName, name.textContent);
    assertEquals(0, name.querySelectorAll('.search-highlight-hit').length);

    // Search hint should be have the description and be highlighted.
    const hint = item.shadowRoot!.querySelector('.search-hint')!;
    assertTrue(hint.textContent!.includes(params.description));
    assertFalse(hint.textContent!.includes(params.location));
    const searchHits = hint.querySelectorAll('.search-highlight-hit');
    assertEquals(1, searchHits.length);
    assertEquals('ABC', searchHits[0]!.textContent);
  });
});
