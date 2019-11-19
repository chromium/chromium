// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.m.js';
//
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {blur, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

/** @fileoverview Suite of tests for cr-toolbar-search-field. */
suite('cr-toolbar-search-field', function() {
  /** @type {?CrToolbarSearchFieldElement} */
  let field = null;

  /** @type {?Array<string>} */
  let searches = null;

  /** @param {string} term */
  function simulateSearch(term) {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
  }

  setup(function() {
    PolymerTest.clearBody();
    field = document.createElement('cr-toolbar-search-field');
    searches = [];
    field.addEventListener('search-changed', function(event) {
      searches.push(event.detail);
    });
    document.body.appendChild(field);
  });

  teardown(function() {
    field.remove();
    field = null;
    searches = null;
  });

  // Test that no initial 'search-changed' event is fired during
  // construction and initialization of the cr-toolbar-search-field element.
  test('no initial search-changed event', function() {
    let didFire = false;
    const onSearchChanged = function() {
      didFire = true;
    };

    // Need to attach listener event before the element is created, to catch
    // the unnecessary initial event.
    document.body.addEventListener('search-changed', onSearchChanged);
    document.body.innerHTML =
        '<cr-toolbar-search-field></cr-toolbar-search-field>';
    // Remove event listener on |body| so that other tests are not affected.
    document.body.removeEventListener('search-changed', onSearchChanged);

    assertFalse(didFire, 'Should not have fired search-changed event');
  });

  test('opens and closes correctly', function() {
    assertFalse(field.showingSearch);
    field.click();
    assertTrue(field.showingSearch);
    assertEquals(field.$.searchInput, field.root.activeElement);

    MockInteractions.blur(field.$.searchInput);
    assertFalse(field.showingSearch);

    field.click();
    assertEquals(field.$.searchInput, field.root.activeElement);

    MockInteractions.pressAndReleaseKeyOn(
        field.$.searchInput, 27, '', 'Escape');
    assertFalse(field.showingSearch, 'Pressing escape closes field.');
    assertNotEquals(field.$.searchInput, field.root.activeElement);
  });

  test('clear search button clears and refocuses input', function() {
    field.click();
    simulateSearch('query1');
    Polymer.dom.flush();
    assertTrue(field.hasSearchText);

    const clearSearch = field.$$('#clearSearch');
    clearSearch.focus();
    clearSearch.click();
    assertTrue(field.showingSearch);
    assertEquals('', field.getValue());
    assertEquals(field.$.searchInput, field.root.activeElement);
    assertFalse(field.hasSearchText);
    assertFalse(field.spinnerActive);
  });

  test('notifies on new searches', function() {
    field.click();
    simulateSearch('query1');
    Polymer.dom.flush();
    assertEquals('query1', field.getValue());

    field.$$('#clearSearch').click();
    assertTrue(field.showingSearch);
    assertEquals('', field.getValue());

    simulateSearch('query2');
    // Expecting identical query to be ignored.
    simulateSearch('query2');

    assertDeepEquals(['query1', '', 'query2'], searches);
  });

  test('notifies on setValue', function() {
    field.click();
    field.setValue('foo');
    field.setValue('');
    field.setValue('bar');
    // Expecting identical query to be ignored.
    field.setValue('bar');
    field.setValue('baz');
    assertDeepEquals(['foo', '', 'bar', 'baz'], searches);
  });

  test('does not notify on setValue with noEvent=true', function() {
    field.click();
    field.setValue('foo', true);
    field.setValue('bar');
    field.setValue('baz', true);
    assertDeepEquals(['bar'], searches);
  });

  test('treat consecutive whitespace as single space', function() {
    field.click();
    const query = 'foo        bar     baz';
    simulateSearch(query);
    Polymer.dom.flush();
    assertEquals(query, field.getValue());

    // Expecting effectively the same query to be ignored.
    const effectivelySameQuery = 'foo   bar    baz';
    simulateSearch(effectivelySameQuery);
    Polymer.dom.flush();
    assertEquals(effectivelySameQuery, field.getValue());

    assertDeepEquals(['foo bar baz'], searches);
  });

  test('ignore leading whitespace', () => {
    field.click();
    const query = ' foo';
    simulateSearch(query);
    Polymer.dom.flush();
    assertEquals(query, field.getValue());

    // Expecting effectively the same query to be ignored.
    const effectivelySameQuery = '     foo';
    simulateSearch(effectivelySameQuery);
    Polymer.dom.flush();
    assertEquals(effectivelySameQuery, field.getValue());

    assertDeepEquals(['foo'], searches);
  });

  test('when there is trailing whitespace, replace with one space', () => {
    field.click();
    const query = 'foo  ';
    simulateSearch(query);
    Polymer.dom.flush();
    assertEquals(query, field.getValue());

    // Expecting effectively the same query to be ignored.
    const effectivelySameQuery = 'foo        ';
    simulateSearch(effectivelySameQuery);
    Polymer.dom.flush();
    assertEquals(effectivelySameQuery, field.getValue());

    assertDeepEquals(['foo '], searches);
  });

  // Tests that calling setValue() from within a 'search-changed' callback
  // does not result in an infinite loop.
  test('no infinite loop', function() {
    let counter = 0;
    field.addEventListener('search-changed', function(event) {
      counter++;
      // Calling setValue() with the already existing value should not
      // trigger another 'search-changed' event.
      field.setValue(event.detail);
    });

    field.click();
    field.setValue('bar');
    assertEquals(1, counter);
    assertDeepEquals(['bar'], searches);
  });

  test('blur does not close field when a search is active', function() {
    field.click();
    simulateSearch('test');
    MockInteractions.blur(field.$.searchInput);

    assertTrue(field.showingSearch);
  });

  test('opens when value is changed', function() {
    // Change search value without explicitly opening the field first.
    // Similar to what happens when pasting or dragging into the input
    // field.
    assertFalse(field.hasSearchText);
    simulateSearch('test');
    assertTrue(field.hasSearchText);
    Polymer.dom.flush();

    const clearSearch = field.$$('#clearSearch');
    assertFalse(clearSearch.hidden);
    assertTrue(field.showingSearch);
  });

  test('closes when value is cleared while unfocused', function() {
    MockInteractions.focus(field.$.searchInput);
    simulateSearch('test');
    Polymer.dom.flush();

    // Does not close the field if it is focused when cleared.
    assertTrue(field.showingSearch);
    field.setValue('');
    assertTrue(field.showingSearch);

    // Does close the field if it is blurred before being cleared.
    simulateSearch('test');
    MockInteractions.blur(field.$.searchInput);
    field.setValue('');
    assertFalse(field.showingSearch);
  });
});
