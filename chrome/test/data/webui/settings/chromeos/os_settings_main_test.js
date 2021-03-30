// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {CrSettingsPrefs, pageVisibility, Router, routes, setSearchManagerForTesting, SearchRequest} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// clang-format on

cr.define('settings_main_page', function() {
  /**
   * Extending TestBrowserProxy even though SearchManager is not a browser proxy
   * itself. Essentially TestBrowserProxy can act as a "proxy" for any external
   * dependency, not just "browser proxies" (and maybe should be renamed to
   * TestProxy).
   *
   * @implements {SearchManager}
   */
  class TestSearchManager extends TestBrowserProxy {
    constructor() {
      super([
        'search',
      ]);

      /** @private {boolean} */
      this.matchesFound_ = true;

      /** @private {?settings.SearchRequest} */
      this.searchRequest_ = null;
    }

    /**
     * @param {boolean} matchesFound
     */
    setMatchesFound(matchesFound) {
      this.matchesFound_ = matchesFound;
    }

    /** @override */
    search(text, page) {
      this.methodCalled('search', text);

      if (this.searchRequest_ == null || !this.searchRequest_.isSame(text)) {
        this.searchRequest_ = new settings.SearchRequest(text);
        this.searchRequest_.finished = true;
        this.searchRequest_.updateMatches(this.matchesFound_);
        this.searchRequest_.resolver.resolve(this.searchRequest_);
      }
      return this.searchRequest_.resolver.promise;
    }
  }

  let settingsPrefs = null;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  suite('MainPageTests', function() {
    /** @type {?TestSearchManager} */
    let searchManager = null;

    /** @type {?SettingsMainElement} */
    let settingsMain = null;

    setup(function() {
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      searchManager = new TestSearchManager();
      settings.setSearchManagerForTesting(searchManager);
      PolymerTest.clearBody();
      settingsMain = document.createElement('os-settings-main');
      settingsMain.prefs = settingsPrefs.prefs;
      settingsMain.toolbarSpinnerActive = false;
      settingsMain.pageVisibility = settings.pageVisibility;
      document.body.appendChild(settingsMain);
    });

    teardown(function() {
      settingsMain.remove();
    });

    test('searchContents() triggers SearchManager', async () => {
      Polymer.dom.flush();

      const expectedQuery1 = 'foo';
      const expectedQuery2 = 'bar';
      const expectedQuery3 = '';

      await settingsMain.searchContents(expectedQuery1);
      const query1 = await searchManager.whenCalled('search');
      assertEquals(expectedQuery1, query1);

      searchManager.resetResolver('search');
      await settingsMain.searchContents(expectedQuery2);
      const query2 = await searchManager.whenCalled('search');
      assertEquals(expectedQuery2, query2);

      searchManager.resetResolver('search');
      await settingsMain.searchContents(expectedQuery3);
      const query3 = await searchManager.whenCalled('search');
      assertEquals(expectedQuery3, query3);
    });

    function showManagedHeader() {
      return settingsMain.showManagedHeader_(
          settingsMain.inSearchMode_, settingsMain.showingSubpage_,
          settingsMain.showPages_.about);
    }

    test('managed header hides when searching', async () => {
      Polymer.dom.flush();

      assertTrue(showManagedHeader());

      searchManager.setMatchesFound(false);
      await settingsMain.searchContents('Query1');
      assertFalse(showManagedHeader());

      searchManager.setMatchesFound(true);
      await settingsMain.searchContents('Query2');
      assertFalse(showManagedHeader());
    });

    test('managed header hides when showing subpage', function() {
      Polymer.dom.flush();

      assertTrue(showManagedHeader());

      const page = settingsMain.$$('os-settings-page');
      page.fire('subpage-expand', {});

      assertFalse(showManagedHeader());
    });

    test('managed header hides when showing about page', function() {
      Polymer.dom.flush();

      assertTrue(showManagedHeader());
      settings.Router.getInstance().navigateTo(settings.routes.ABOUT);

      assertFalse(showManagedHeader());
    });

    /** @return {!HTMLElement} */
    function getToggleContainer() {
      const page = settingsMain.$$('os-settings-page');
      assertTrue(!!page);
      const toggleContainer = page.$$('#toggleContainer');
      assertTrue(!!toggleContainer);
      return toggleContainer;
    }

    /**
     * Asserts that the Advanced toggle container exists in the combined
     * settings page and asserts whether it should be visible.
     * @param {boolean} expectedVisible
     */
    function assertToggleContainerVisible(expectedVisible) {
      const toggleContainer = getToggleContainer();
      if (expectedVisible) {
        assertNotEquals('none', toggleContainer.style.display);
      } else {
        assertEquals('none', toggleContainer.style.display);
      }
    }

    test('no results page shows and hides', async () => {
      Polymer.dom.flush();
      const noSearchResults = settingsMain.$.noSearchResults;
      assertTrue(!!noSearchResults);
      assertTrue(noSearchResults.hidden);

      assertToggleContainerVisible(true);

      searchManager.setMatchesFound(false);
      await settingsMain.searchContents('Query1');
      assertFalse(noSearchResults.hidden);
      assertToggleContainerVisible(false);

      searchManager.setMatchesFound(true);
      await settingsMain.searchContents('Query2');
      assertTrue(noSearchResults.hidden);
    });

    // Ensure that when the user clears the search box, the "no results" page
    // is hidden and the "advanced page toggle" is visible again.
    test('no results page hides on clear', async () => {
      Polymer.dom.flush();
      const noSearchResults = settingsMain.$.noSearchResults;
      assertTrue(!!noSearchResults);
      assertTrue(noSearchResults.hidden);

      assertToggleContainerVisible(true);

      searchManager.setMatchesFound(false);
      // Clearing the search box is effectively a search for the empty string.
      await settingsMain.searchContents('');
      Polymer.dom.flush();
      assertTrue(noSearchResults.hidden);
      assertToggleContainerVisible(true);
    });

    /**
     * Asserts the visibility of the basic and advanced pages.
     * @param {string} Expected 'display' value for the basic page.
     * @param {string} Expected 'display' value for the advanced page.
     */
    async function assertPageVisibility(expectedBasic, expectedAdvanced) {
      Polymer.dom.flush();
      const page = settingsMain.$$('os-settings-page');
      assertEquals(
          expectedBasic, getComputedStyle(page.$$('#basicPage')).display);

      const advancedPage = await page.$$('#advancedPageTemplate').get();
      assertEquals(expectedAdvanced, getComputedStyle(advancedPage).display);
    }

    /**
     * Asserts the visibility of the basic and advanced pages after exiting
     * search mode.
     * @param {string} Expected 'display' value for the advanced page.
     */
    async function assertAdvancedVisibilityAfterSearch(expectedAdvanced) {
      searchManager.setMatchesFound(true);
      await settingsMain.searchContents('Query1');

      searchManager.setMatchesFound(false);
      await settingsMain.searchContents('');

      // Imitate behavior of clearing search.
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      Polymer.dom.flush();
      await assertPageVisibility('block', expectedAdvanced);
    }

    test('exiting search mode, advanced collapsed', async () => {
      // Simulating searching while the advanced page is collapsed.
      settingsMain.currentRouteChanged(settings.routes.BASIC);
      Polymer.dom.flush();
      await assertAdvancedVisibilityAfterSearch('none');
    });

    // Ensure that clearing the search results restores both "basic" and
    // "advanced" page, when the search has been initiated from a subpage
    // whose parent is the "advanced" page.
    test('exiting search mode, advanced expanded', async () => {
      // Trigger basic page to be rendered once.
      settings.Router.getInstance().navigateTo(settings.routes.DEVICE);
      Polymer.dom.flush();

      // Navigate to an "advanced" subpage.
      settings.Router.getInstance().navigateTo(settings.routes.DATETIME);
      Polymer.dom.flush();
      await assertAdvancedVisibilityAfterSearch('block');
    });

    // Ensure that searching, then entering a subpage, then going back
    // lands the user in a page where both basic and advanced sections are
    // visible, because the page is still in search mode.
    test('returning from subpage to search results', async () => {
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      Polymer.dom.flush();

      searchManager.setMatchesFound(true);
      await settingsMain.searchContents('Query1');
      // Simulate navigating into a subpage.
      settings.Router.getInstance().navigateTo(settings.routes.DISPLAY);
      settingsMain.$$('os-settings-page').fire('subpage-expand');
      Polymer.dom.flush();

      // Simulate clicking the left arrow to go back to the search results.
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      await assertPageVisibility('block', 'block');
    });

    test('navigating to a basic page does not collapse advanced', async () => {
      settings.Router.getInstance().navigateTo(settings.routes.DATETIME);
      Polymer.dom.flush();

      assertToggleContainerVisible(true);

      settings.Router.getInstance().navigateTo(settings.routes.DEVICE);
      Polymer.dom.flush();

      await assertPageVisibility('block', 'block');
    });

    test('updates the title based on current route', function() {
      settings.Router.getInstance().navigateTo(settings.routes.BASIC);
      assertEquals(document.title, loadTimeData.getString('settings'));

      settings.Router.getInstance().navigateTo(settings.routes.ABOUT);
      assertEquals(
          document.title,
          loadTimeData.getStringF(
              'settingsAltPageTitle',
              loadTimeData.getString('aboutPageTitle')));
    });
  });
  // #cr_define_end
});
