// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SearchManager, SettingsIdleLoadElement, SettingsMainElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, pageVisibility, Router, routes, SearchRequest, setSearchManagerForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * Extending TestBrowserProxy even though SearchManager is not a browser proxy
 * itself. Essentially TestBrowserProxy can act as a "proxy" for any external
 * dependency, not just "browser proxies" (and maybe should be renamed to
 * TestProxy).
 */
class TestSearchManager extends TestBrowserProxy implements SearchManager {
  private matchesFound_: boolean = true;
  private searchRequest_: SearchRequest|null = null;

  constructor() {
    super(['search']);
  }

  setMatchesFound(matchesFound: boolean) {
    this.matchesFound_ = matchesFound;
  }

  search(text: string, page: Element) {
    this.methodCalled('search', text);

    if (this.searchRequest_ == null || !this.searchRequest_.isSame(text)) {
      this.searchRequest_ = new SearchRequest(text, page);
      this.searchRequest_.updateMatches(this.matchesFound_);
      this.searchRequest_.resolver.resolve(this.searchRequest_);
    }
    return this.searchRequest_.resolver.promise;
  }
}

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
    settingsMain.pageVisibility = pageVisibility;
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

    const basicPage =
        settingsMain.shadowRoot!.querySelector('settings-basic-page')!;
    basicPage.dispatchEvent(
        new CustomEvent('subpage-expand', {bubbles: true, composed: true}));
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
  // is hidden and the "advanced page toggle" is visible again.
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
   * Asserts the visibility of the basic and advanced pages.
   * @param Expected 'display' value for the basic page.
   * @param Expected 'display' value for the advanced page.
   */
  function assertPageVisibility(
      expectedBasic: string, expectedAdvanced: string): Promise<void> {
    flush();
    const page = settingsMain.shadowRoot!.querySelector('settings-basic-page')!;
    assertEquals(
        expectedBasic,
        getComputedStyle(page.shadowRoot!.querySelector('#basicPage')!)
            .display);

    return page.shadowRoot!
        .querySelector<SettingsIdleLoadElement>('#advancedPageTemplate')!.get()
        .then(function(advancedPage) {
          assertEquals(
              expectedAdvanced, getComputedStyle(advancedPage).display);
        });
  }

  // TODO(michaelpg): It would be better not to drill into
  // settings-basic-page. If search should indeed only work in Settings
  // (as opposed to Advanced), perhaps some of this logic should be
  // delegated to settings-basic-page now instead of settings-main.

  /**
   * Asserts the visibility of the basic and advanced pages after exiting
   * search mode.
   * @param Expected 'display' value for the advanced page.
   */
  function assertAdvancedVisibilityAfterSearch(expectedAdvanced: string):
      Promise<void> {
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
    settingsMain.currentRouteChanged();
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
      settingsMain.shadowRoot!.querySelector('settings-basic-page')!
          .dispatchEvent(new CustomEvent(
              'subpage-expand', {bubbles: true, composed: true}));
      flush();

      // Simulate clicking the left arrow to go back to the search results.
      Router.getInstance().navigateTo(routes.BASIC);
      return assertPageVisibility('block', 'block');
    });
  });

  test('navigating to a basic page does not collapse advanced', function() {
    Router.getInstance().navigateTo(routes.LANGUAGES);
    flush();

    Router.getInstance().navigateTo(routes.PEOPLE);
    flush();

    return assertPageVisibility('block', 'block');
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
