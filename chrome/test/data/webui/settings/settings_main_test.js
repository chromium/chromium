// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, pageVisibility, Router, routes, SearchRequest, setSearchManagerForTesting} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, whenAttributeIs} from 'chrome://webui-test/test_util.js';
// clang-format on

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

    /** @private {?SearchRequest} */
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
      this.searchRequest_ = new SearchRequest(text);
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
    Router.getInstance().navigateTo(routes.BASIC);
    searchManager = new TestSearchManager();
    setSearchManagerForTesting(searchManager);
    PolymerTest.clearBody();
    settingsMain = document.createElement('settings-main');
    settingsMain.prefs = settingsPrefs.prefs;
    settingsMain.toolbarSpinnerActive = false;
    settingsMain.pageVisibility = pageVisibility;
    document.body.appendChild(settingsMain);
  });

  teardown(function() {
    settingsMain.remove();
  });

  test('searchContents() triggers SearchManager', function() {
    flush();

    const expectedQuery1 = 'foo';
    const expectedQuery2 = 'bar';
    const expectedQuery3 = '';

    return settingsMain.searchContents(expectedQuery1)
        .then(function() {
          return searchManager.whenCalled('search');
        })
        .then(function(query) {
          assertEquals(expectedQuery1, query);

          searchManager.resetResolver('search');
          return settingsMain.searchContents(expectedQuery2);
        })
        .then(function() {
          return searchManager.whenCalled('search');
        })
        .then(function(query) {
          assertEquals(expectedQuery2, query);

          searchManager.resetResolver('search');
          return settingsMain.searchContents(expectedQuery3);
        })
        .then(function() {
          return searchManager.whenCalled('search');
        })
        .then(function(query) {
          assertEquals(expectedQuery3, query);
        });
  });

  function showManagedHeader() {
    return settingsMain.showManagedHeader_(
        settingsMain.inSearchMode_, settingsMain.showingSubpage_,
        settingsMain.showPages_.about);
  }

  test('managed header hides when searching', function() {
    flush();

    assertTrue(showManagedHeader());

    searchManager.setMatchesFound(false);
    return settingsMain.searchContents('Query1')
        .then(() => {
          assertFalse(showManagedHeader());

          searchManager.setMatchesFound(true);
          return settingsMain.searchContents('Query2');
        })
        .then(() => {
          assertFalse(showManagedHeader());
        });
  });

  test('managed header hides when showing subpage', function() {
    flush();

    assertTrue(showManagedHeader());

    const basicPage = settingsMain.$$('settings-basic-page');
    basicPage.fire('subpage-expand', {});

    assertFalse(showManagedHeader());
  });

  test('managed header hides when showing about page', function() {
    flush();

    assertTrue(showManagedHeader());
    Router.getInstance().navigateTo(routes.ABOUT);

    assertFalse(showManagedHeader());
  });

  /** @return {!HTMLElement} */
  function getToggleContainer() {
    const page = settingsMain.$$('settings-basic-page');
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

  test('no results page shows and hides', function() {
    flush();
    const noSearchResults = settingsMain.$.noSearchResults;
    assertTrue(!!noSearchResults);
    assertTrue(noSearchResults.hidden);

    assertToggleContainerVisible(true);

    searchManager.setMatchesFound(false);
    return settingsMain.searchContents('Query1')
        .then(function() {
          assertFalse(noSearchResults.hidden);
          assertToggleContainerVisible(false);

          searchManager.setMatchesFound(true);
          return settingsMain.searchContents('Query2');
        })
        .then(function() {
          assertTrue(noSearchResults.hidden);
        });
  });

  // Ensure that when the user clears the search box, the "no results" page
  // is hidden and the "advanced page toggle" is visible again.
  test('no results page hides on clear', function() {
    flush();
    const noSearchResults = settingsMain.$.noSearchResults;
    assertTrue(!!noSearchResults);
    assertTrue(noSearchResults.hidden);

    assertToggleContainerVisible(true);

    searchManager.setMatchesFound(false);
    // Clearing the search box is effectively a search for the empty string.
    return settingsMain.searchContents('').then(function() {
      flush();
      assertTrue(noSearchResults.hidden);
      assertToggleContainerVisible(true);
    });
  });

  /**
   * Asserts the visibility of the basic and advanced pages.
   * @param {string} Expected 'display' value for the basic page.
   * @param {string} Expected 'display' value for the advanced page.
   * @return {!Promise}
   */
  function assertPageVisibility(expectedBasic, expectedAdvanced) {
    flush();
    const page = settingsMain.$$('settings-basic-page');
    assertEquals(
        expectedBasic, getComputedStyle(page.$$('#basicPage')).display);

    return page.$$('#advancedPageTemplate').get().then(function(advancedPage) {
      assertEquals(expectedAdvanced, getComputedStyle(advancedPage).display);
    });
  }

  // TODO(michaelpg): It would be better not to drill into
  // settings-basic-page. If search should indeed only work in Settings
  // (as opposed to Advanced), perhaps some of this logic should be
  // delegated to settings-basic-page now instead of settings-main.

  /**
   * Asserts the visibility of the basic and advanced pages after exiting
   * search mode.
   * @param {string} Expected 'display' value for the advanced page.
   * @return {!Promise}
   */
  function assertAdvancedVisibilityAfterSearch(expectedAdvanced) {
    searchManager.setMatchesFound(true);
    return settingsMain.searchContents('Query1')
        .then(function() {
          searchManager.setMatchesFound(false);
          return settingsMain.searchContents('');
        })
        .then(function() {
          // Imitate behavior of clearing search.
          Router.getInstance().navigateTo(routes.BASIC);
          flush();
          return assertPageVisibility('block', expectedAdvanced);
        });
  }

  test('exiting search mode, advanced collapsed', function() {
    // Simulating searching while the advanced page is collapsed.
    settingsMain.currentRouteChanged(routes.BASIC);
    flush();
    return assertAdvancedVisibilityAfterSearch('none');
  });

  // Ensure that clearing the search results restores both "basic" and
  // "advanced" page, when the search has been initiated from a subpage
  // whose parent is the "advanced" page.
  test('exiting search mode, advanced expanded', function() {
    // Trigger basic page to be rendered once.
    Router.getInstance().navigateTo(routes.APPEARANCE);
    flush();

    // Navigate to an "advanced" subpage.
    Router.getInstance().navigateTo(routes.LANGUAGES);
    flush();
    return assertAdvancedVisibilityAfterSearch('block');
  });

  // Ensure that searching, then entering a subpage, then going back
  // lands the user in a page where both basic and advanced sections are
  // visible, because the page is still in search mode.
  test('returning from subpage to search results', function() {
    Router.getInstance().navigateTo(routes.BASIC);
    flush();

    searchManager.setMatchesFound(true);
    return settingsMain.searchContents('Query1').then(function() {
      // Simulate navigating into a subpage.
      Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
      settingsMain.$$('settings-basic-page').fire('subpage-expand');
      flush();

      // Simulate clicking the left arrow to go back to the search results.
      Router.getInstance().navigateTo(routes.BASIC);
      return assertPageVisibility('block', 'block');
    });
  });

  // TODO(michaelpg): Move these to a new test for settings-basic-page.
  test('can collapse advanced on advanced section route', function() {
    Router.getInstance().navigateTo(routes.LANGUAGES);
    flush();

    const basicPage = settingsMain.$$('settings-basic-page');
    let advancedPage = null;

    return eventToPromise('showing-section', settingsMain)
        .then(() => {
          return basicPage.$$('#advancedPageTemplate').get();
        })
        .then(function(advanced) {
          advancedPage = advanced;
          return assertPageVisibility('block', 'block');
        })
        .then(function() {
          const whenHidden = whenAttributeIs(advancedPage, 'hidden', '');
          eventToPromise('scroll-to-bottom', basicPage)
              .then(event => event.detail.callback());

          const advancedToggle =
              getToggleContainer().querySelector('#advancedToggle');
          assertTrue(!!advancedToggle);
          advancedToggle.click();
          return whenHidden;
        })
        .then(function() {
          return assertPageVisibility('block', 'none');
        });
  });

  test('navigating to a basic page does not collapse advanced', function() {
    Router.getInstance().navigateTo(routes.LANGUAGES);
    flush();

    assertToggleContainerVisible(true);

    Router.getInstance().navigateTo(routes.PEOPLE);
    flush();

    return assertPageVisibility('block', 'block');
  });

  test('updates the title based on current route', function() {
    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(document.title, loadTimeData.getString('settings'));

    Router.getInstance().navigateTo(routes.ABOUT);
    assertEquals(
        document.title,
        loadTimeData.getStringF(
            'settingsAltPageTitle', loadTimeData.getString('aboutPageTitle')));
  });
});
