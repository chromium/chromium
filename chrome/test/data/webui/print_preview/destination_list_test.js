// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.destination_list_test = {};
destination_list_test.suiteName = 'DestinationListTest';
/** @enum {string} */
destination_list_test.TestNames = {
  FilterDestinations: 'FilterDestinations',
  FireDestinationSelected: 'FireDestinationSelected',
};

suite(destination_list_test.suiteName, function() {
  /** @type {?PrintPreviewDestinationListElement} */
  let list = null;

  /** @override */
  setup(function() {
    // Create destinations
    const destinations = [
      new Destination(
          'id1', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'One',
          DestinationConnectionStatus.ONLINE, {description: 'ABC'}),
      new Destination(
          'id2', DestinationType.LOCAL, DestinationOrigin.LOCAL, 'Two',
          DestinationConnectionStatus.ONLINE, {description: 'XYZ'}),
      new Destination(
          'id3', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Three',
          DestinationConnectionStatus.ONLINE,
          {description: 'ABC', tags: ['__cp__location=123']}),
      new Destination(
          'id4', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Four',
          DestinationConnectionStatus.ONLINE,
          {description: 'XYZ', tags: ['__cp__location=123']}),
      new Destination(
          'id5', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Five',
          DestinationConnectionStatus.ONLINE,
          {description: 'XYZ', tags: ['__cp__location=123']})
    ];

    // Set up list
    document.body.innerHTML = `
          <print-preview-destination-list id="testList" has-action-link=true
              loading-destinations=false list-name="test">
          </print-preview-destination-list>`;
    list = document.body.querySelector('#testList');
    list.searchQuery = null;
    list.destinations = destinations;
    list.loadingDestinations = false;
    flush();
  });

  // Tests that the list correctly shows and hides destinations based on the
  // value of the search query.
  test(assert(destination_list_test.TestNames.FilterDestinations), function() {
    const items =
        list.shadowRoot.querySelectorAll('print-preview-destination-list-item');
    const noMatchHint = list.$$('.no-destinations-message');

    // Query is initialized to null. All items are shown and the hint is
    // hidden.
    items.forEach(item => assertFalse(item.hidden));
    assertTrue(noMatchHint.hidden);

    // Searching for "e" should show "One", "Three", and "Five".
    list.searchQuery = /(e)/i;
    flush();
    assertEquals(undefined, Array.from(items).find(item => {
      return !item.hidden &&
          (item.destination.displayName == 'Two' ||
           item.destination.displayName == 'Four');
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for "ABC" should show "One" and "Three".
    list.searchQuery = /(ABC)/i;
    flush();
    assertEquals(undefined, Array.from(items).find(item => {
      return !item.hidden && item.destination.displayName != 'One' &&
          item.destination.displayName != 'Three';
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for "F" should show "Four" and "Five"
    list.searchQuery = /(F)/i;
    flush();
    assertEquals(undefined, Array.from(items).find(item => {
      return !item.hidden && item.destination.displayName != 'Four' &&
          item.destination.displayName != 'Five';
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for UVW should show no destinations and display the "no
    // match" hint.
    list.searchQuery = /(UVW)/i;
    flush();
    items.forEach(item => assertTrue(item.hidden));
    assertFalse(noMatchHint.hidden);

    // Searching for 123 should show destinations "Three", "Four", and "Five".
    list.searchQuery = /(123)/i;
    flush();
    assertEquals(undefined, Array.from(items).find(item => {
      return !item.hidden &&
          (item.destination.displayName == 'One' ||
           item.destination.displayName == 'Two');
    }));
    assertTrue(noMatchHint.hidden);

    // Clearing the query restores the original state.
    list.searchQuery = /()/i;
    flush();
    items.forEach(item => assertFalse(item.hidden));
    assertTrue(noMatchHint.hidden);
  });

  // Tests that the list correctly fires the destination selected event when
  // the destination is clicked or the enter key is pressed.
  test(
      assert(destination_list_test.TestNames.FireDestinationSelected),
      function() {
        const items = list.shadowRoot.querySelectorAll(
            'print-preview-destination-list-item');
        let whenDestinationSelected =
            eventToPromise('destination-selected', list);
        items[0].click();
        return whenDestinationSelected
            .then(event => {
              assertEquals(items[0], event.detail);
              whenDestinationSelected =
                  eventToPromise('destination-selected', list);
              keyEventOn(items[1], 'keydown', 13, undefined, 'Enter');
              return whenDestinationSelected;
            })
            .then(event => {
              assertEquals(items[1], event.detail);
            });
      });
});
