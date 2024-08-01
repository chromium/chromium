// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationDropdownCrosElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {move} from 'chrome://webui-test/mouse_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getGoogleDriveDestination, getSaveAsPdfDestination} from './print_preview_test_utils.js';

suite('DestinationDropdownCrosTest', function() {
  let dropdown: PrintPreviewDestinationDropdownCrosElement;

  function setItemList(items: Destination[]) {
    dropdown.itemList = items;
    flush();
  }

  function getList(): HTMLButtonElement[] {
    return Array.from(
        dropdown.shadowRoot!.querySelectorAll<HTMLButtonElement>('.list-item'));
  }

  function clickDropdown() {
    dropdown.$.destinationDropdown.click();
  }

  function clickDropdownFocus() {
    dropdown.$.destinationDropdown.click();
    dropdown.$.destinationDropdown.focus();
  }

  function clickOutsideDropdown() {
    document.body.click();
    dropdown.$.destinationDropdown.blur();
  }

  function down() {
    keyDownOn(dropdown.$.destinationDropdown, 40, [], 'ArrowDown');
  }

  function up() {
    keyDownOn(dropdown.$.destinationDropdown, 38, [], 'ArrowUp');
  }

  function enter() {
    keyDownOn(dropdown.$.destinationDropdown, 13, [], 'Enter');
  }

  function getHighlightedElement(): (HTMLElement|null) {
    return dropdown.shadowRoot!.querySelector('.highlighted');
  }

  function getHighlightedElementText(): string {
    return getHighlightedElement()!.textContent!.trim();
  }

  function createDestination(
      displayName: string, destinationOrigin: DestinationOrigin): Destination {
    return new Destination(displayName, destinationOrigin, displayName);
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    dropdown =
        document.createElement('print-preview-destination-dropdown-cros');
    document.body.appendChild(dropdown);
    dropdown.noDestinations = false;
    dropdown.driveDestinationKey = getGoogleDriveDestination().key;
    dropdown.pdfDestinationKey = getSaveAsPdfDestination().key;
  });

  test(
      'CorrectListItems', function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS),
        ]);

        const itemList = getList();
        assertEquals(7, itemList.length);
        assertEquals('One', itemList[0]!.textContent!.trim());
        assertEquals('Two', itemList[1]!.textContent!.trim());
        assertEquals('Three', itemList[2]!.textContent!.trim());
      });

  test('ClickCloses', function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    const ironDropdown = dropdown.shadowRoot!.querySelector('iron-dropdown')!;

    clickDropdownFocus();
    assertTrue(ironDropdown.opened);

    getList()[0]!.click();
    assertFalse(ironDropdown.opened);

    clickDropdownFocus();
    assertTrue(ironDropdown.opened);

    // Clicking outside the dropdown will cause it to lose focus and close.
    // This will verify on-blur closes the dropdown.
    clickOutsideDropdown();
    assertFalse(ironDropdown.opened);
  });

  test('HighlightedAfterUpDown', function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    clickDropdown();

    assertEquals('One', getHighlightedElementText());
    down();
    assertEquals('Save as PDF', getHighlightedElementText());
    down();
    assertEquals('Save to Google Drive', getHighlightedElementText());
    down();
    assertEquals('See more…', getHighlightedElementText());
    down();
    assertEquals('See more…', getHighlightedElementText());

    up();
    assertEquals('Save to Google Drive', getHighlightedElementText());
    up();
    assertEquals('Save as PDF', getHighlightedElementText());
    up();
    assertEquals('One', getHighlightedElementText());
    up();
    assertEquals('One', getHighlightedElementText());
  });

  test(
      'DestinationChangeAfterUpDown', function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        const pdfDestination = getSaveAsPdfDestination();
        setItemList([destinationOne]);
        dropdown.value = pdfDestination;

        // Verify an up press sends |destinationOne| as the next value selected.
        const whenSelectedAfterUpPress =
            eventToPromise('dropdown-value-selected', dropdown);
        up();
        whenSelectedAfterUpPress.then(event => {
          assertEquals(destinationOne.key, event.detail.value);
        });

        // Key press does not directly update |value| so it is expected for the
        // |value| to not change here in this test.
        assertEquals(dropdown.value, pdfDestination);

        // Verify a down press sends the Save to Google Drive destination as the
        // next value selected.
        const whenSelectedAfterDownPress =
            eventToPromise('dropdown-value-selected', dropdown);
        down();
        whenSelectedAfterDownPress.then(event => {
          assertEquals(getGoogleDriveDestination().key, event.detail.value);
        });
      });

  test('EnterOpensCloses', function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;

    assertFalse(dropdown.shadowRoot!.querySelector('iron-dropdown')!.opened);
    enter();
    assertTrue(dropdown.shadowRoot!.querySelector('iron-dropdown')!.opened);
    enter();
    assertFalse(dropdown.shadowRoot!.querySelector('iron-dropdown')!.opened);
  });

  test(
      'HighlightedFollowsMouse', function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([
          destinationOne,
          createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS),
        ]);
        dropdown.value = destinationOne;
        clickDropdown();

        move(getList()[1]!, {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Two', getHighlightedElementText());
        move(getList()[2]!, {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Three', getHighlightedElementText());

        // Interacting with the keyboard should update the highlighted element.
        up();
        assertEquals('Two', getHighlightedElementText());

        // When the user moves the mouse again, the highlighted element should
        // change.
        move(getList()[0]!, {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('One', getHighlightedElementText());
      });

  test('Disabled', function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    dropdown.disabled = true;

    clickDropdown();
    assertFalse(dropdown.shadowRoot!.querySelector('iron-dropdown')!.opened);
    assertEquals('-1', dropdown.$.destinationDropdown.getAttribute('tabindex'));

    dropdown.disabled = false;
    clickDropdown();
    assertTrue(dropdown.shadowRoot!.querySelector('iron-dropdown')!.opened);
    assertEquals('0', dropdown.$.destinationDropdown.getAttribute('tabindex'));
  });

  test(
      'HighlightedWhenOpened', function() {
        const destinationTwo = createDestination('Two', DestinationOrigin.CROS);
        const destinationThree =
            createDestination('Three', DestinationOrigin.CROS);
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          destinationTwo,
          destinationThree,
        ]);

        dropdown.value = destinationTwo;
        clickDropdown();
        assertEquals('Two', getHighlightedElementText());
        clickDropdown();

        dropdown.value = destinationThree;
        clickDropdown();
        assertEquals('Three', getHighlightedElementText());
      });
});
