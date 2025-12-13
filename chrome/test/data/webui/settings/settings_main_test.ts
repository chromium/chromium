// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsMainElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, Router, routes, setSearchManagerForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSearchManager} from './test_search_manager.js';
// clang-format on

suite('MainPageTests', function() {
  let searchManager: TestSearchManager;
  let settingsMain: SettingsMainElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    Router.getInstance().navigateTo(routes.BASIC);
    searchManager = new TestSearchManager();
    setSearchManagerForTesting(searchManager);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMain = document.createElement('settings-main');
    settingsMain.prefs = settingsPrefs.prefs!;
    settingsMain.toolbarSpinnerActive = false;
    document.body.appendChild(settingsMain);
  });

  teardown(function() {
    settingsMain.remove();
  });

  test('searchContents() triggers SearchManager', async function() {
    flush();

    const expectedQuery1 = 'foo';
    const expectedQuery2 = 'bar';
    const expectedQuery3 = '';

    await settingsMain.searchContents(expectedQuery1);

    let query = await searchManager.whenCalled('search');
    assertEquals(expectedQuery1, query);

    searchManager.resetResolver('search');
    await settingsMain.searchContents(expectedQuery2);

    query = await searchManager.whenCalled('search');
    assertEquals(expectedQuery2, query);

    searchManager.resetResolver('search');
    await settingsMain.searchContents(expectedQuery3);

    query = await searchManager.whenCalled('search');
    assertEquals(expectedQuery3, query);
  });

  function showingManagedHeader(): boolean {
    return !!settingsMain.shadowRoot!.querySelector('managed-footnote');
  }

  test('managed header hides when searching', function() {
    flush();

    assertTrue(showingManagedHeader());

    searchManager.setMatchesFound(false);
    return settingsMain.searchContents('Query1')
        .then(() => {
          assertFalse(showingManagedHeader());

          searchManager.setMatchesFound(true);
          return settingsMain.searchContents('Query2');
        })
        .then(() => {
          assertFalse(showingManagedHeader());
        });
  });

  test('managed header hides when showing subpage', function() {
    flush();
    assertTrue(showingManagedHeader());
    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
    flush();
    assertFalse(showingManagedHeader());
  });

  test('managed header hides when showing about page', function() {
    flush();
    assertTrue(showingManagedHeader());
    Router.getInstance().navigateTo(routes.ABOUT);
    flush();
    assertFalse(showingManagedHeader());
  });

  test('no results page shows and hides', function() {
    flush();
    const noSearchResults = settingsMain.$.noSearchResults;
    assertTrue(!!noSearchResults);
    assertTrue(noSearchResults.hidden);

    searchManager.setMatchesFound(false);
    return settingsMain.searchContents('Query1')
        .then(function() {
          assertFalse(noSearchResults.hidden);

          searchManager.setMatchesFound(true);
          return settingsMain.searchContents('Query2');
        })
        .then(function() {
          assertTrue(noSearchResults.hidden);
        });
  });

  // Ensure that when the user clears the search box, the "no results" page
  // is hidden.
  test('no results page hides on clear', function() {
    flush();
    const noSearchResults = settingsMain.$.noSearchResults;
    assertTrue(!!noSearchResults);
    assertTrue(noSearchResults.hidden);

    searchManager.setMatchesFound(false);
    // Clearing the search box is effectively a search for the empty string.
    return settingsMain.searchContents('').then(function() {
      flush();
      assertTrue(noSearchResults.hidden);
    });
  });

  /**
   * Asserts the visibility of the basic pages.
   * @param Expected 'display' value for the basic page.
   */
  function assertPageVisibility(expected: string) {
    flush();
    const page = settingsMain.shadowRoot!.querySelector('settings-basic-page')!;
    assertEquals(
        expected,
        getComputedStyle(page.shadowRoot!.querySelector('#basicPage')!)
            .display);
  }

  // Ensure that searching, then entering a subpage, then going back
  // lands the user in a page where basic sections are visible, because the
  // page is still in search mode.
  test('returning from subpage to search results', function() {
    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    searchManager.setMatchesFound(true);
    return settingsMain.searchContents('Query1').then(function() {
      // Simulate navigating into a subpage.
      Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
      settingsMain.shadowRoot!.querySelector('settings-basic-page')!
          .dispatchEvent(new CustomEvent(
              'subpage-expand', {bubbles: true, composed: true}));
      flush();

      // Simulate clicking the left arrow to go back to the search results.
      Router.getInstance().navigateTo(routes.BASIC);
      assertPageVisibility('block');
    });
  });

  test('updates the title based on current route', function() {
    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(document.title, loadTimeData.getString('settings'));

    Router.getInstance().navigateTo(routes.LANGUAGES);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle',
            loadTimeData.getString('languagesPageTitle')));

    Router.getInstance().navigateTo(routes.ABOUT);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle', loadTimeData.getString('aboutPageTitle')));
  });

  test('uses parent title for navigable dialog routes', function() {
    Router.getInstance().navigateTo(routes.CLEAR_BROWSER_DATA);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle',
            loadTimeData.getString('privacyPageTitle')));
  });
});
