// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    this.handler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowed_models: [],
        allowed_tools: [],
        allowed_input_types: [],
        active_model: 0,  // kUnspecified
        active_tool: 0,   // kUnspecified
        disabled_models: [],
        disabled_tools: [],
        disabled_input_types: [],
      },
    }));
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

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    app = document.createElement('omnibox-popup-app');
    document.body.appendChild(app);

    await microtasksFinished();
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
    const shownResult = createAutocompleteResultForTesting({
      matches: [
        createSearchMatchForTesting({isHidden: false}),
        createSearchMatchForTesting({isHidden: true}),
      ],
    });
    testProxy.page.autocompleteResultChanged(shownResult);
    await microtasksFinished();

    // Ensure dropdown shows.
    assertTrue(isVisible(app.getDropdown()));

    // Set autocomplete result with no visible matches.
    const hiddenResult = createAutocompleteResultForTesting({
      matches: [
        createSearchMatchForTesting({isHidden: true}),
        createSearchMatchForTesting({isHidden: true}),
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
    const noResult = createAutocompleteResultForTesting({matches: []});
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
      const contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertFalse(!!contextualEntrypoint);
    });

    test('AiModePrefUpdatesContextualEntrypointVisibility', async () => {
      let contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextualEntrypoint);
      assertTrue(isVisible(contextualEntrypoint));

      // Disable AI Mode Shortcuts.
      testProxy.page.onShowAiModePrefChanged(false);
      await microtasksFinished();
      contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertFalse(!!contextualEntrypoint);

      // Enable AI Mode Shortcuts.
      testProxy.page.onShowAiModePrefChanged(true);
      await microtasksFinished();
      contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextualEntrypoint);
      assertTrue(isVisible(contextualEntrypoint));
    });

    test('KeywordModeUpdatesContextualEntrypointVisibility', async () => {
      let contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextualEntrypoint);
      assertTrue(isVisible(contextualEntrypoint));

      // Enter keyword mode.
      testProxy.page.setKeywordSelected(true);
      await microtasksFinished();
      assertFalse(isVisible(contextualEntrypoint));

      // Exit keyword mode.
      testProxy.page.setKeywordSelected(false);
      await microtasksFinished();
      contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(isVisible(contextualEntrypoint));
    });

    test('OnShowCallsBlur', async () => {
      // Arrange: Focus the button and confirm it's focused.
      const contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextualEntrypoint);
      await microtasksFinished();
      const entrypointButton =
          contextualEntrypoint.shadowRoot?.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton);
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
        url: 'https://www.google.com/search?q=foo',
        showInPreviousTabChip: true,
      };
      testProxy.handler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: [tabInfo]}));
      localApp.remove();
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);
      testProxy.page.autocompleteResultChanged(
          createAutocompleteResultForTesting());
      await microtasksFinished();

      testProxy.initVisibilityPrefs();
      await microtasksFinished();

      testProxy.page.onShow();
      await microtasksFinished();

      const recentTabChip = localApp.shadowRoot?.querySelector(
          'composebox-recent-tab-chip');
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
      let contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertFalse(isVisible(contextualEntrypoint));

      testProxy.page.updateAimEligibility(true);
      await microtasksFinished();
      contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(isVisible(contextualEntrypoint));

      testProxy.page.updateAimEligibility(false);
      await microtasksFinished();
      contextualEntrypoint = localApp.shadowRoot?.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertFalse(isVisible(contextualEntrypoint));
    });
  });
});
