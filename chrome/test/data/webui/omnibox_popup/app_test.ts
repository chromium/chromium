// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {createAutocompleteMatch, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createAutocompleteResult(modifiers: Partial<AutocompleteResult> = {}):
    AutocompleteResult {
  const base: AutocompleteResult = {
    input: '',
    matches: [],
    suggestionGroupsMap: {},
    smartComposeInlineHint: null,
  };

  return Object.assign(base, modifiers);
}

function createSearchMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return Object.assign(
      createAutocompleteMatch(), {
        isSearchType: true,
        contents: 'hello world',
        destinationUrl: {url: 'https://www.google.com/search?q=hello+world'},
        fillIntoEdit: 'hello world',
        type: 'search-suggest',
      },
      modifiers);
}

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
  installer(mock);
  return mock;
}

// TODO(b/453041451): Create separate file for TestSearchboxBrowserProxy
//  or reuse the one `cr-searchbox` tests use.
class TestSearchboxBrowserProxy extends TestBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  page: PageRemote;

  constructor() {
    super();
    this.callbackRouter = new PageCallbackRouter();
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.handler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  initVisibilityPrefs() {
    this.page.updateAimEligibility(true);
    this.page.onShowAiModePrefChanged(true);
    this.page.updateContentSharingPolicy(true);
  }
}

suite('AppTest', function() {
  let app: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    app = document.createElement('omnibox-popup-app');
    document.body.appendChild(app);
  });

  test('ContextMenuPrevented', async function() {
    const whenFired = eventToPromise('contextmenu', document.documentElement);
    document.documentElement.dispatchEvent(
        new Event('contextmenu', {cancelable: true}));
    const e = await whenFired;
    assertTrue(e.defaultPrevented);
  });

  test('OnlyShowsDropdownIfVisibleMatches', async () => {
    // Set autocomplete result with one visible match.
    const shownResult: AutocompleteResult = createAutocompleteResult({
      matches: [
        createSearchMatch({isHidden: false}),
        createSearchMatch({isHidden: true}),
      ],
    });
    testProxy.page.autocompleteResultChanged(shownResult);
    await microtasksFinished();

    // Ensure dropdown shows.
    assertTrue(isVisible(app.getDropdown()));

    // Set autocomplete result with no visible matches.
    const hiddenResult: AutocompleteResult = createAutocompleteResult({
      matches: [
        createSearchMatch({isHidden: true}),
        createSearchMatch({isHidden: true}),
      ],
    });
    testProxy.page.autocompleteResultChanged(hiddenResult);
    await microtasksFinished();

    // Ensure dropdown hides.
    assertFalse(isVisible(app.getDropdown()));

    // Force dropdown to show again.
    testProxy.page.autocompleteResultChanged(shownResult);
    await microtasksFinished();
    assertTrue(isVisible(app.getDropdown()));

    // Set autocomplete result with no matches.
    const noResult: AutocompleteResult =
        createAutocompleteResult({matches: []});
    testProxy.page.autocompleteResultChanged(noResult);
    await microtasksFinished();

    // Ensure dropdown hides.
    assertFalse(isVisible(app.getDropdown()));
  });

  suite('TallSearchbox', () => {
    let localApp: OmniboxPopupAppElement;

    setup(async () => {
      // Use setup instead of suiteSetup to ensure a clean state for each test.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        searchboxLayoutMode: 'TallTopContext',
      });

      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);
      testProxy.initVisibilityPrefs();
      await microtasksFinished();
    });

    test('ContextMenuEntrypointHiddenWhenDisabled', async () => {
      testProxy.page.updateAimEligibility(false);
      await microtasksFinished();
      const carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertFalse(!!carousel);
    });

    test('AiModePrefUpdatesCarouselVisibility', async () => {
      let carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(!!carousel);
      assertTrue(isVisible(carousel));

      // Disable AI Mode Shortcuts.
      testProxy.page.onShowAiModePrefChanged(false);
      await microtasksFinished();
      carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertFalse(!!carousel);

      // Enable AI Mode Shortcuts.
      testProxy.page.onShowAiModePrefChanged(true);
      await microtasksFinished();
      carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(!!carousel);
      assertTrue(isVisible(carousel));
    });

    test('KeywordModeUpdatesCarouselVisibility', async () => {
      let carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(!!carousel);
      assertTrue(isVisible(carousel));

      // Enter keyword mode.
      testProxy.page.setKeywordSelected(true);
      await microtasksFinished();
      assertFalse(isVisible(carousel));

      // Exit keyword mode.
      testProxy.page.setKeywordSelected(false);
      await microtasksFinished();
      carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(isVisible(carousel));
    });

    test('OnShowCallsBlur', async () => {
      // Arrange: Focus the button and confirm it's focused.
      const carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(!!carousel);
      await microtasksFinished();
      const entrypointButton =
          carousel.$.contextEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint')!;
      entrypointButton.focus();
      await microtasksFinished();
      assertTrue(entrypointButton.matches(':focus-within'));

      // Act: Show the popup.
      testProxy.page.onShow();
      await microtasksFinished();

      // Assert: The button is no longer focused.
      assertFalse(entrypointButton.matches(':focus-within'));
    });

    test('RecentTabChipShown', async () => {
      loadTimeData.overrideValues({
        searchboxLayoutMode: 'TallTopContext',
        composeboxShowRecentTabChip: true,
        addTabUploadDelayOnRecentTabChipClick: true,
      });
      const tabInfo = {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'https://www.google.com/search?q=foo'},
        showInPreviousTabChip: true,
      };
      testProxy.handler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: [tabInfo]}));
      localApp.remove();
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);
      testProxy.page.autocompleteResultChanged(createAutocompleteResult());
      await microtasksFinished();

      testProxy.initVisibilityPrefs();
      await microtasksFinished();

      const carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(!!carousel);
      const recentTabChip =
          carousel.shadowRoot.querySelector<HTMLElement>('#recentTabChip');
      // Assert chip shows.
      assertTrue(!!recentTabChip);
    });
  });

  suite('AimEligibility', () => {
    let localApp: OmniboxPopupAppElement;

    setup(async () => {
      // Use setup instead of suiteSetup to ensure a clean state for each test.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);

      testProxy.initVisibilityPrefs();
      await microtasksFinished();
    });

    test('AimEligibility', async () => {
      testProxy.page.updateAimEligibility(false);
      await microtasksFinished();
      let carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertFalse(isVisible(carousel));

      testProxy.page.updateAimEligibility(true);
      await microtasksFinished();
      carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertTrue(isVisible(carousel));

      testProxy.page.updateAimEligibility(false);
      await microtasksFinished();
      carousel = localApp.shadowRoot?.querySelector(
          'contextual-entrypoint-and-carousel');
      assertFalse(isVisible(carousel));
    });
  });
});
