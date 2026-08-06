// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import type {OmniboxPopupPageRemote} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxContextualEntrypointButtonElement, OmniboxPopupAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {RenderType, SelectionDirection, SelectionLineState, SelectionStep, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDefaultInputState, TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

function getContextualEntrypointButton(appElement: OmniboxPopupAppElement):
    OmniboxContextualEntrypointButtonElement {
  const popupEntrypoint = $$(appElement, 'omnibox-popup-contextual-entrypoint');
  assertTrue(!!popupEntrypoint);
  const contextualEntrypoint =
      $$(popupEntrypoint, 'omnibox-contextual-entrypoint-button');
  assertTrue(!!contextualEntrypoint);
  return contextualEntrypoint as OmniboxContextualEntrypointButtonElement;
}

suite('AppTest', function() {
  let app: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      omniboxShowContextButtonSuggestionLabel: false,
      addContext: 'Add tabs and more',
      contextButtonShapeIsOblong: false,
      composeboxShowLensIcon: false,
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance, remote} =
        omniboxPopupBrowserProxyFactory.createForTest(handler);
    callbackRouter = remote;
    omniboxPopupBrowserProxyFactory.setInstance(instance);

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

  test('CurrentTabChipShown', async () => {
    loadTimeData.overrideValues({
      composeboxShowCurrentTabChip: true,
      composeboxShowLensSearchChip: true,
    });

    // Re-create app to apply loadTimeData overrides.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('omnibox-popup-app');
    document.body.appendChild(app);
    testProxy.initVisibilityPrefs();
    await microtasksFinished();

    const mockTab: TabInfo = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://tab1.com/',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 1n},
    };

    testProxy.handler.setPromiseResolveFor('getRecentTabs', {tabs: [mockTab]});

    // Set eligibility.
    testProxy.page.updateLensSearchEligibility(true);
    testProxy.page.updateContentSharingPolicy(true);

    // Set autocomplete result with empty input.
    const result = createAutocompleteResultForTesting({
      input: '',
      matches: [],
    });
    testProxy.page.autocompleteResultChanged(result);
    await microtasksFinished();

    // Trigger show.
    callbackRouter.onShow();
    await testProxy.handler.whenCalled('getRecentTabs');
    await microtasksFinished();

    const popupEntrypoint = $$(app, 'omnibox-popup-contextual-entrypoint');
    assertTrue(!!popupEntrypoint);
    const chip = $$(popupEntrypoint, 'composebox-current-tab-chip');
    assertTrue(!!chip);
    assertTrue(isVisible(chip));
  });

  test('LensIconShown', async () => {
    loadTimeData.overrideValues({
      composeboxShowLensIcon: true,
    });

    // Re-create app to apply loadTimeData overrides.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('omnibox-popup-app');
    document.body.appendChild(app);
    testProxy.initVisibilityPrefs();
    await microtasksFinished();

    // Set eligibility.
    testProxy.page.updateLensSearchEligibility(true);

    // Set autocomplete result with empty input.
    const result = createAutocompleteResultForTesting({
      input: '',
      matches: [],
    });
    testProxy.page.autocompleteResultChanged(result);
    await microtasksFinished();

    // Trigger show.
    callbackRouter.onShow();
    await microtasksFinished();

    const popupEntrypoint = $$(app, 'omnibox-popup-contextual-entrypoint');
    assertTrue(!!popupEntrypoint);
    assertTrue(isVisible($$(popupEntrypoint, '#lensSearchIcon')));
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

  test('SecondarySideShows', async () => {
    // Ensure `canShowSecondarySide` is set to true.
    app.canShowSecondarySide = true;
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting({suggestionGroupId: 1}),
      createSearchMatchForTesting({suggestionGroupId: 100}),
    ];
    const suggestionGroupsMap = {
      1: {
        header: 'Primary',
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kDefaultPrimary,
      },
      100: {
        header: 'Secondary',
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kSecondary,
      },
    };

    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches: matches,
          suggestionGroupsMap: suggestionGroupsMap,
        }));
    await microtasksFinished();

    assertTrue(app.hasSecondarySide);

    // Verify `secondary-side` element is rendered and visible.
    const dropdown = $$(app, 'cr-searchbox-dropdown');
    assertTrue(!!dropdown);
    assertTrue(isVisible($$(dropdown, '.secondary-side')));

    // Verify secondary side is hidden when `canShowSecondarySide` is false.
    app.canShowSecondarySide = false;
    await microtasksFinished();
    assertFalse(isVisible($$(dropdown, '.secondary-side')));
  });

  suite('TallSearchbox', () => {
    let localApp: OmniboxPopupAppElement;

    setup(async () => {
      // Use setup instead of suiteSetup to ensure a clean state for each test.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        omniboxAimPopupEnabled: true,
        omniboxShowContextButtonSuggestionLabel: false,
        searchboxLayoutMode: 'TallBottomContext',
      });

      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);
      testProxy.initVisibilityPrefs();
      await microtasksFinished();
    });

    test('ContextMenuEntrypointHiddenWhenDisabled', async () => {
      testProxy.page.updateAimPopupEligibility(false);
      await microtasksFinished();
      assertFalse(!!$$(localApp, 'omnibox-popup-contextual-entrypoint'));
    });

    // TODO(b/539623520): Move to `omnibox_contextual_entrypoint_test.ts`. Left
    // here for now to verify refactor (b/539624759).
    test('OnShowCallsBlur', async () => {
      // Arrange: Focus the button and confirm it's focused.
      const contextualEntrypoint = getContextualEntrypointButton(localApp);
      await microtasksFinished();
      const innerEntrypoint = $$(
          contextualEntrypoint, 'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!innerEntrypoint);
      const entrypointButton = $$<HTMLElement>(innerEntrypoint, '#entrypoint');
      assertTrue(!!entrypointButton);
      entrypointButton.focus();
      await microtasksFinished();
      assertTrue(entrypointButton.matches(':focus-within'));

      // Act: Show the popup.
      callbackRouter.onShow();
      await microtasksFinished();

      // Assert: The button is no longer focused.
      assertFalse(entrypointButton.matches(':focus-within'));
    });

    test('HideClassicContextButton', async () => {
      const contextualEntrypoint = getContextualEntrypointButton(localApp);
      assertTrue(isVisible(contextualEntrypoint));

      // Re-create app with `hideClassicContextButton` set to true.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        omniboxShowContextButtonSuggestionLabel: false,
        hideClassicContextButton: true,
      });
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);

      testProxy.initVisibilityPrefs();
      testProxy.page.updateAimPopupEligibility(true);
      await microtasksFinished();

      assertFalse(!!$$(localApp, 'omnibox-popup-contextual-entrypoint'));
    });

    // TODO(b/539623520): Move to `omnibox_contextual_entrypoint_test.ts`. Left
    // here for now to verify refactor (b/539624759).
    test('ShowContextButtonText', async () => {
      let contextualEntrypoint = getContextualEntrypointButton(localApp);
      const innerEntrypoint = $$(
          contextualEntrypoint, 'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!innerEntrypoint);
      assertFalse(!!$$(innerEntrypoint, '#description'));

      // Re-create app with `composeboxShowContextMenuDescription` set to true.
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        omniboxAimPopupEnabled: true,
        omniboxShowContextButtonSuggestionLabel: false,
        composeboxShowContextMenuDescription: true,
        searchboxLayoutMode: 'TallBottomContext',
      });
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);

      testProxy.initVisibilityPrefs();
      testProxy.page.updateAimPopupEligibility(true);
      await microtasksFinished();

      contextualEntrypoint = getContextualEntrypointButton(localApp);
      const newInnerEntrypoint = $$(
          contextualEntrypoint, 'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!newInnerEntrypoint);
      const description = $$(newInnerEntrypoint, '#description');
      assertTrue(!!description);
      assertEquals('Add tabs and more', description.textContent.trim());
    });

    // TODO(b/539623520): Move to `omnibox_contextual_entrypoint_test.ts`. Left
    // here for now to verify refactor (b/539624759).
    test('ContextMenuEntrypointMenuOpenWorkaround', async () => {
      const contextualEntrypoint = getContextualEntrypointButton(localApp);
      const innerEntrypoint = $$(
          contextualEntrypoint, 'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!innerEntrypoint);

      // Click fires event and applies workaround.
      innerEntrypoint.dispatchEvent(
          new CustomEvent('context-menu-entrypoint-click', {
            detail: {x: 10, y: 20},
            bubbles: true,
            composed: true,
          }));

      assertTrue(contextualEntrypoint.classList.contains('menu-open'));

      // Mojom callback clears class.
      callbackRouter.onContextMenuClosed();
      await microtasksFinished();

      assertFalse(contextualEntrypoint.classList.contains('menu-open'));
    });
  });

  suite('AimEligibility', () => {
    let localApp: OmniboxPopupAppElement;

    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);

      testProxy.initVisibilityPrefs();
      await microtasksFinished();
    });

    test('AimEligibility', async () => {
      testProxy.page.updateAimPopupEligibility(false);
      await microtasksFinished();
      let popupEntrypoint = $$(localApp, 'omnibox-popup-contextual-entrypoint');
      assertFalse(!!popupEntrypoint);

      testProxy.page.updateAimPopupEligibility(true);
      await microtasksFinished();
      const contextualEntrypoint = getContextualEntrypointButton(localApp);
      assertTrue(isVisible(contextualEntrypoint));

      testProxy.page.updateAimPopupEligibility(false);
      await microtasksFinished();
      popupEntrypoint = $$(localApp, 'omnibox-popup-contextual-entrypoint');
      assertFalse(!!popupEntrypoint);
    });

    test('DisallowedInputsHidesEntrypoint', async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({contextualMenuUsePecApi: true});
      localApp = document.createElement('omnibox-popup-app');
      document.body.appendChild(localApp);

      testProxy.initVisibilityPrefs();
      testProxy.page.updateAimPopupEligibility(true);
      await microtasksFinished();

      testProxy.page.onInputStateChanged({
        ...createDefaultInputState(),
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
      });
      await microtasksFinished();

      const popupEntrypoint =
          $$(localApp, 'omnibox-popup-contextual-entrypoint');
      assertTrue(!!popupEntrypoint);
      const contextualEntrypoint =
          $$(popupEntrypoint, 'omnibox-contextual-entrypoint-button');
      assertFalse(!!contextualEntrypoint);
    });
  });
});

suite('AppTestSelectionControl', () => {
  let localApp: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxShowContextButtonSuggestionLabel: false,
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
    const [_sequenceId, selection, disposition] =
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
    const [_sequenceId, selection, disposition] =
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
