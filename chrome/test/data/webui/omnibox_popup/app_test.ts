// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {OmniboxPopupBrowserProxy, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SelectionDirection, SelectionLineState, SelectionStep} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOmniboxPopupBrowserProxy} from './test_omnibox_popup_browser_proxy.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('AppTest', function() {
  let app: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;
  let omniboxPopupTestProxy: TestOmniboxPopupBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    omniboxPopupTestProxy = new TestOmniboxPopupBrowserProxy();
    OmniboxPopupBrowserProxy.setInstance(omniboxPopupTestProxy);

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
      omniboxPopupTestProxy.page.onShow();
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

      omniboxPopupTestProxy.page.onShow();
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

suite('AppTestSelectionControl', () => {
  let localApp: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    // Use setup instead of suiteSetup to ensure a clean state for each test.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      webuiOmniboxPopupSelectionControlEnabled: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    localApp = document.createElement('omnibox-popup-app');
    document.body.appendChild(localApp);
    testProxy.initVisibilityPrefs();
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: [
            createSearchMatchForTesting({contents: 'a'}),
            createSearchMatchForTesting(
                {contents: 'b', supportsDeletion: true}),
            createSearchMatchForTesting({contents: 'c'}),
          ],
        }));
    return microtasksFinished();
  });

  test('StepSelection', async () => {
    // Starts as if omnibox just focused, with default selection (none) so
    // first step is onto first line.
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kStateOrLine);
    testProxy.page.openCurrentSelection(WindowOpenDisposition.CURRENT_TAB);
    const [selection, disposition] =
        await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(WindowOpenDisposition.CURRENT_TAB, disposition);
    assertDeepEquals(
        {
          line: 1,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        },
        selection);
  });

  test('OpenCurrentSelection', async () => {
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kAllLines);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kWholeLine);
    testProxy.page.openCurrentSelection(WindowOpenDisposition.CURRENT_TAB);
    const [selection, disposition] =
        await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(WindowOpenDisposition.CURRENT_TAB, disposition);
    assertDeepEquals(
        {
          line: 0,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        },
        selection);
  });
});
