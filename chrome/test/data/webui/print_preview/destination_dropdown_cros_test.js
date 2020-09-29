// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {keyDownOn, move} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

import {getGoogleDriveDestination, getSaveAsPdfDestination} from './print_preview_test_utils.js';

window.destination_dropdown_cros_test = {};
const destination_dropdown_cros_test = window.destination_dropdown_cros_test;
destination_dropdown_cros_test.suiteName =
    'PrintPreviewDestinationDropdownCrosTest';
/** @enum {string} */
destination_dropdown_cros_test.TestNames = {
  CorrectListItems: 'correct list items',
  ClickCloses: 'click closes dropdown',
  HighlightedAfterUpDown: 'highlighted after keyboard press up and down',
  DestinationChangeAfterUpDown:
      'destination changes after keyboard press up and down',
  EnterOpensCloses: 'enter opens and closes dropdown',
  HighlightedFollowsMouse: 'highlighted follows mouse',
  Disabled: 'disabled',
  HighlightedWhenOpened: 'highlighted when opened',
};

suite(destination_dropdown_cros_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationDropdownCrosElement} */
  let dropdown;

  /** @param {!Array<!Destination>} items */
  function setItemList(items) {
    dropdown.itemList = items;
    flush();
  }

  /** @return {!NodeList} */
  function getList() {
    return dropdown.shadowRoot.querySelectorAll('.list-item');
  }

  function clickDropdown() {
    dropdown.$$('#destination-dropdown').click();
  }

  function clickDropdownFocus() {
    dropdown.$$('#destination-dropdown').click();
    dropdown.$$('#destination-dropdown').focus();
  }

  function clickOutsideDropdown() {
    document.body.click();
    dropdown.$$('#destination-dropdown').blur();
  }

  function down() {
    keyDownOn(
        dropdown.$$('#destination-dropdown'), 'ArrowDown', [], 'ArrowDown');
  }

  function up() {
    keyDownOn(dropdown.$$('#destination-dropdown'), 'ArrowUp', [], 'ArrowUp');
  }

  function enter() {
    keyDownOn(dropdown.$$('#destination-dropdown'), 'Enter', [], 'Enter');
  }

  /** @return {?Element} */
  function getHighlightedElement() {
    return dropdown.$$('.highlighted');
  }

  /** @return {string} */
  function getHighlightedElementText() {
    return getHighlightedElement().textContent.trim();
  }

  /**
   * @param {string} displayName
   * @param {!DestinationOrigin} destinationOrigin
   * @return {!Destination}
   */
  function createDestination(displayName, destinationOrigin) {
    return new Destination(
        displayName, DestinationType.LOCAL, destinationOrigin, displayName,
        DestinationConnectionStatus.ONLINE);
  }

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    dropdown =
        /** @type {!PrintPreviewDestinationDropdownCrosElement} */
        (document.createElement('print-preview-destination-dropdown-cros'));
    document.body.appendChild(dropdown);
    dropdown.noDestinations = false;
    dropdown.driveDestinationKey = getGoogleDriveDestination('account').key;
    dropdown.pdfDestinationKey = getSaveAsPdfDestination().key;
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.CorrectListItems),
      function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS)
        ]);

        const itemList = getList();
        assertEquals(7, itemList.length);
        assertEquals('One', itemList[0].textContent.trim());
        assertEquals('Two', itemList[1].textContent.trim());
        assertEquals('Three', itemList[2].textContent.trim());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickCloses), function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;
        const ironDropdown = dropdown.$$('iron-dropdown');

        clickDropdownFocus();
        assertTrue(ironDropdown.opened);

        getList()[0].click();
        assertFalse(ironDropdown.opened);

        clickDropdownFocus();
        assertTrue(ironDropdown.opened);

        // Clicking outside the dropdown will cause it to lose focus and close.
        // This will verify on-blur closes the dropdown.
        clickOutsideDropdown();
        assertFalse(ironDropdown.opened);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedAfterUpDown),
      function() {
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
      assert(destination_dropdown_cros_test.TestNames
                 .DestinationChangeAfterUpDown),
      function() {
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
        assert(dropdown.value === pdfDestination);

        // Verify a down press sends the Save to Google Drive destination as the
        // next value selected.
        const whenSelectedAfterDownPress =
            eventToPromise('dropdown-value-selected', dropdown);
        down();
        whenSelectedAfterDownPress.then(event => {
          assertEquals(
              getGoogleDriveDestination('account').key, event.detail.value);
        });
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.EnterOpensCloses),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;

        assertFalse(dropdown.$$('iron-dropdown').opened);
        enter();
        assertTrue(dropdown.$$('iron-dropdown').opened);
        enter();
        assertFalse(dropdown.$$('iron-dropdown').opened);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedFollowsMouse),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([
          destinationOne, createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS)
        ]);
        dropdown.value = destinationOne;
        clickDropdown();

        move(getList()[1], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Two', getHighlightedElementText());
        move(getList()[2], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Three', getHighlightedElementText());

        // Interacting with the keyboard should update the highlighted element.
        up();
        assertEquals('Two', getHighlightedElementText());

        // When the user moves the mouse again, the highlighted element should
        // change.
        move(getList()[0], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('One', getHighlightedElementText());
      });

  test(assert(destination_dropdown_cros_test.TestNames.Disabled), function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    dropdown.disabled = true;

    clickDropdown();
    assertFalse(dropdown.$$('iron-dropdown').opened);
    assertEquals(
        '-1', dropdown.$$('#destination-dropdown').getAttribute('tabindex'));

    dropdown.disabled = false;
    clickDropdown();
    assertTrue(dropdown.$$('iron-dropdown').opened);
    assertEquals(
        '0', dropdown.$$('#destination-dropdown').getAttribute('tabindex'));
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedWhenOpened),
      function() {
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
