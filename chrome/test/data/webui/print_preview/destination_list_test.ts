// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationListElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, getTrustedHTML} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('DestinationListTest', function() {
  let list: PrintPreviewDestinationListElement;

  setup(function() {
    // Create destinations
    const destinations = [
      new Destination(
          'id1', DestinationOrigin.LOCAL, 'One', {description: 'ABC'}),
      new Destination(
          'id2', DestinationOrigin.LOCAL, 'Two', {description: 'XYZ'}),
      new Destination(
          'id3', DestinationOrigin.LOCAL, 'Three',
          {description: 'ABC', location: '123'}),
      new Destination(
          'id4', DestinationOrigin.LOCAL, 'Four',
          {description: 'XYZ', location: '123'}),
      new Destination(
          'id5', DestinationOrigin.LOCAL, 'Five',
          {description: 'XYZ', location: '123'}),
    ];

    // Set up list
    document.body.innerHTML = getTrustedHTML`
          <print-preview-destination-list id="testList" has-action-link=true
              loading-destinations=false list-name="test">
          </print-preview-destination-list>`;
    list = document.body.querySelector<PrintPreviewDestinationListElement>(
        '#testList')!;
    list.searchQuery = null;
    list.destinations = destinations;
    list.loadingDestinations = false;
    flush();
  });

  // Tests that the list correctly shows and hides destinations based on the
  // value of the search query.
  test('FilterDestinations', function() {
    const items = list.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');
    const noMatchHint = list.shadowRoot!.querySelector<HTMLElement>(
        '.no-destinations-message')!;
    const ironList = list.$.list;

    // Query is initialized to null. All items are shown and the hint is
    // hidden.
    assertFalse(ironList.hidden);
    items.forEach(item => assertFalse((item.parentNode as HTMLElement).hidden));
    assertTrue(noMatchHint.hidden);

    // Searching for "e" should show "One", "Three", and "Five".
    list.searchQuery = /(e)/ig;
    flush();
    assertFalse(ironList.hidden);
    assertEquals(undefined, Array.from(items).find(item => {
      return !(item.parentNode as HTMLElement).hidden &&
          (item.destination.displayName === 'Two' ||
           item.destination.displayName === 'Four');
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for "ABC" should show "One" and "Three".
    list.searchQuery = /(ABC)/ig;
    flush();
    assertFalse(ironList.hidden);
    assertEquals(undefined, Array.from(items).find(item => {
      return !(item.parentNode as HTMLElement).hidden &&
          item.destination.displayName !== 'One' &&
          item.destination.displayName !== 'Three';
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for "F" should show "Four" and "Five"
    list.searchQuery = /(F)/ig;
    flush();
    assertFalse(ironList.hidden);
    assertEquals(undefined, Array.from(items).find(item => {
      return !(item.parentNode as HTMLElement).hidden &&
          item.destination.displayName !== 'Four' &&
          item.destination.displayName !== 'Five';
    }));
    assertTrue(noMatchHint.hidden);

    // Searching for UVW should show no destinations and display the "no
    // match" hint.
    list.searchQuery = /(UVW)/ig;
    flush();
    assertTrue(ironList.hidden);
    assertFalse(noMatchHint.hidden);

    // Searching for 123 should show destinations "Three", "Four", and "Five".
    list.searchQuery = /(123)/ig;
    flush();
    assertFalse(ironList.hidden);
    assertEquals(undefined, Array.from(items).find(item => {
      return !(item.parentNode as HTMLElement).hidden &&
          (item.destination.displayName === 'One' ||
           item.destination.displayName === 'Two');
    }));
    assertTrue(noMatchHint.hidden);

    // Clearing the query restores the original state.
    list.searchQuery = null;
    flush();
    assertFalse(ironList.hidden);
    items.forEach(
        item => assertFalse((item!.parentNode! as HTMLElement).hidden));
    assertTrue(noMatchHint.hidden);
  });

  // Tests that the list correctly fires the destination selected event when
  // the destination is clicked or the enter key is pressed.
  test(
      'FireDestinationSelected', function() {
        const items = list.shadowRoot!.querySelectorAll(
            'print-preview-destination-list-item');
        let whenDestinationSelected =
            eventToPromise('destination-selected', list);
        items[0]!.click();
        return whenDestinationSelected
            .then(event => {
              assertEquals(items[0]!, event.detail);
              whenDestinationSelected =
                  eventToPromise('destination-selected', list);
              keyEventOn(items[1]!, 'keydown', 13, undefined, 'Enter');
              return whenDestinationSelected;
            })
            .then(event => {
              assertEquals(items[1]!, event.detail);
            });
      });
});
