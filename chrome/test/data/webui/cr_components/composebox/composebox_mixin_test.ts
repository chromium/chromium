// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';

import {ComposeboxFile, ContextType, ContextualSearchInputStateDeletionType, TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DriveDisclaimerStatus, DriveUploadError, InputMethod, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {AutocompleteMatch, AutocompleteResult, PageRemote as SearchboxPageRemote, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ContextUploadStatus, InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="not is_android">
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

// </if>
import {installMock, MockInputState} from './composebox_test_utils.js';
import type {TestComposeboxMixinElement} from './test_composebox_mixin.js';

function simulateUserTextInput(
    inputElement: ComposeboxInputElement, value: string): Promise<void> {
  inputElement.input = value;
  inputElement.fire('input-input');
  return microtasksFinished();
}

suite('ComposeboxMixinTest', () => {
  let element: TestComposeboxMixinElement;
  let searchboxHandler: SearchboxPageHandlerRemote&
      TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let metrics: MetricsTracker;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metrics = fakeMetricsPrivate();
    const callbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new SearchboxPageHandlerRemote(), callbackRouter)));
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    // <if expr="not is_android">
    searchboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    // </if>
    searchboxHandler.setPromiseResolveFor('getInputState', {
      state: new MockInputState(),
    });

    element = document.createElement('test-composebox-mixin');
    // The mixin queries ZPS on mount by default, which advances the
    // autocomplete query id; opt out so per-test assertions start clean.
    element.queryZpsOnLoad = false;
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test(
      'refreshTabSuggestions() dedupes restored tabs with same tabId',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          contextManagementInOmniboxEnabled: true,
        });
        const tab1 = {
          tabId: 0,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Restored = {
          tabId: 2,  // Same ID as recent tab
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Recent = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab3 = {
          tabId: 3,
          title: 'Tab 3',
          url: 'about:blank?3',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock searchboxHandler.getRecentTabs to return tab2Recent and tab3.
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab2Recent, tab3]}));

        element.contextManagementInComposeboxEnabled = true;
        // Set aimThreadRestoredTabs to contain tab1 and tab2Restored.
        element.aimThreadRestoredTabs = [tab1, tab2Restored];

        await element.refreshTabSuggestions();

        // Expected tabSuggestions: [tab1, tab2Restored, tab3]
        // (tab2Recent from recent tabs should be filtered out because its tabId
        // matches tab2Restored)
        assertEquals(3, element.tabSuggestions.length);
        assertEquals(0, element.tabSuggestions[0]!.tabId);
        assertEquals('about:blank?1', element.tabSuggestions[0]!.url);
        assertEquals(2, element.tabSuggestions[1]!.tabId);
        assertEquals('about:blank?2', element.tabSuggestions[1]!.url);
        assertEquals(3, element.tabSuggestions[2]!.tabId);
        assertEquals('about:blank?3', element.tabSuggestions[2]!.url);
      });

  test(
      'refreshTabSuggestions() keeps tabs with diff tabId and same URL',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          contextManagementInOmniboxEnabled: true,
        });
        const tab1 = {
          tabId: 0,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Restored = {
          tabId: 1,  // Different ID than recent tab, e.g. it was closed (or 0)
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Recent = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab3 = {
          tabId: 3,
          title: 'Tab 3',
          url: 'about:blank?3',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock searchboxHandler.getRecentTabs to return tab2Recent and tab3.
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab2Recent, tab3]}));

        // Set aimThreadRestoredTabs to contain tab1 and tab2Restored.
        element.aimThreadRestoredTabs = [tab1, tab2Restored];

        await element.refreshTabSuggestions();

        // Expected tabSuggestions: [tab1, tab2Restored, tab2Recent, tab3]
        // (tab2Recent from recent tabs should NOT be filtered out because its
        // tabId differs from tab2Restored, even though they share the same URL)
        assertEquals(4, element.tabSuggestions.length);
        assertEquals(0, element.tabSuggestions[0]!.tabId);
        assertEquals('about:blank?1', element.tabSuggestions[0]!.url);
        assertEquals(1, element.tabSuggestions[1]!.tabId);
        assertEquals('about:blank?2', element.tabSuggestions[1]!.url);
        assertEquals(2, element.tabSuggestions[2]!.tabId);
        assertEquals('about:blank?2', element.tabSuggestions[2]!.url);
        assertEquals(3, element.tabSuggestions[3]!.tabId);
        assertEquals('about:blank?3', element.tabSuggestions[3]!.url);
      });

  test(
      'Suggestions are sorted by most recently selected shown first.',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          contextManagementInOmniboxEnabled: true,
        });

        // Disable tab deselection so restored tabs cannot be deselected.
        element.tabDeselectionEnabled = false;

        // Setup tabs:
        // 1. Restored (tab1, tab2)
        // 2. Recent & Selected (tab3)
        // 3. Recent & Unselected (tab4)
        const tab1 = {
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2 = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab3 = {
          tabId: 3,
          title: 'Tab 3',
          url: 'about:blank?3',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab4 = {
          tabId: 4,
          title: 'Tab 4',
          url: 'about:blank?4',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock open tabs in browser to return tab3 and tab4.
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab3, tab4]}));

        element.contextManagementInComposeboxEnabled = true;
        element.aimThreadRestoredTabs = [tab1, tab2];
        element.addedTabsIds = new Map([
          [3, 'token3' as unknown as UnguessableToken],
        ]);

        await element.refreshTabSuggestions();

        // After `refreshTabSuggestions()`, tabs should be sorted as:
        // `[selectedRecent, restored, unselectedRecent]`.
        assertEquals(4, element.tabSuggestions.length);
        assertEquals(3, element.tabSuggestions[0]!.tabId);
        assertEquals(1, element.tabSuggestions[1]!.tabId);
        assertEquals(2, element.tabSuggestions[2]!.tabId);
        assertEquals(4, element.tabSuggestions[3]!.tabId);

        // Modify selection of recent tabs (deselect tab3, select tab4).
        element.addedTabsIds = new Map([
          [4, 'token4' as unknown as UnguessableToken],
        ]);

        element.onContextMenuOpened();

        // Tabs should be re-sorted based on updated states.
        assertEquals(4, element.tabSuggestions.length);
        assertEquals(4, element.tabSuggestions[0]!.tabId);
        assertEquals(1, element.tabSuggestions[1]!.tabId);
        assertEquals(2, element.tabSuggestions[2]!.tabId);
        assertEquals(3, element.tabSuggestions[3]!.tabId);
      });

  test(
      'refreshTabSuggestions() filters out closed restored tabs when tabDeselectionEnabled is true',
      async () => {
        const tab1 = {
          tabId: 10,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Closed = {
          tabId: 20,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock open tabs in browser to return only tab1 (tab2 is closed/deleted
        // from tab strip).
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab1]}));

        element.tabDeselectionEnabled = true;
        element.aimThreadRestoredTabs = [tab1, tab2Closed];

        await element.refreshTabSuggestions();

        // Verify: deleteTabContext is called for the closed restored tab
        // (tabId: 20).
        const deleteTabContextCalls =
            searchboxHandler.getCallCount('deleteTabContext');
        assertEquals(1, deleteTabContextCalls);
        assertEquals(20, searchboxHandler.getArgs('deleteTabContext')[0]);

        // Verify: closed restored tab is filtered out from
        // aimThreadRestoredTabs list.
        assertEquals(1, element.aimThreadRestoredTabs.length);
        assertEquals(10, element.aimThreadRestoredTabs[0]!.tabId);
      });

  test(
      'refreshTabSuggestions() filters navigated tabs when tabDeselectionEnabled true',
      async () => {
        const tab1 = {
          tabId: 10,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab1Navigated = {
          tabId: 10,
          title: 'Tab 1',
          url: 'about:blank?1_new',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock open tabs to return tab1 with new URL
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab1Navigated]}));

        element.tabDeselectionEnabled = true;
        element.aimThreadRestoredTabs = [tab1];

        await element.refreshTabSuggestions();

        // Verify: deleteTabContext is called for the navigated tab.
        const deleteTabContextCalls =
            searchboxHandler.getCallCount('deleteTabContext');
        assertEquals(1, deleteTabContextCalls);
        assertEquals(10, searchboxHandler.getArgs('deleteTabContext')[0]);

        // Verify: navigated restored tab is filtered out from active list.
        assertEquals(0, element.aimThreadRestoredTabs.length);
      });


  test(
      'refreshTabSuggestions() removes tab context if it was navigated',
      async () => {
        const tokenTab = 'test-token-tab' as unknown as UnguessableToken;
        const selectedTabId = 100;
        const mockTabFile = new ComposeboxFile(
            tokenTab, 'Selected Tab', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: selectedTabId,
              url: 'about:blank?original',
            });

        element.files = new Map([[tokenTab, mockTabFile]]);
        element.addedTabsIds = new Map([[selectedTabId, tokenTab]]);

        const freshTab = {
          tabId: selectedTabId,
          title: 'Selected Tab',
          url: 'about:blank?navigated',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [freshTab]}));

        await element.refreshTabSuggestions();

        assertFalse(element.files.has(tokenTab));
        assertFalse(element.addedTabsIds.has(selectedTabId));
        assertEquals(1, searchboxHandler.getCallCount('deleteContext'));
      });

  test('submitCleanup() clears active tab selections', async () => {
    const tokenTab = 'test-token-tab' as unknown as UnguessableToken;
    const selectedTabId = 100;
    const mockTabFile = new ComposeboxFile(
        tokenTab, 'Selected Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: selectedTabId,
          url: 'about:blank',
        });

    // Add the selected tab to the active files and added tabs maps.
    element.files = new Map([[tokenTab, mockTabFile]]);
    element.addedTabsIds = new Map([[selectedTabId, tokenTab]]);

    await microtasksFinished();

    element.submitCleanup();

    // Verify: The selected Tab 100 must be completely removed from the
    // current active selection.
    assertFalse(element.addedTabsIds.has(selectedTabId));
    assertFalse(element.files.has(tokenTab));
  });

  test(
      'cacheSubmittedTabs appends submitted tabs in chronological order',
      () => {
        const token1 = 'token1' as unknown as UnguessableToken;
        const tab1 =
            new ComposeboxFile(token1, 'Tab 1', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: 1,
              url: 'about:blank?1',
            });
        const token2 = 'token2' as unknown as UnguessableToken;
        const tab2 =
            new ComposeboxFile(token2, 'Tab 2', 'tab', InputType.kBrowserTab, {
              isDeletable: true,
              tabId: 2,
              url: 'about:blank?2',
            });

        element.files = new Map([[token1, tab1], [token2, tab2]]);
        element.addedTabsIds = new Map([[1, token1], [2, token2]]);
        element.aimThreadRestoredTabs = [
          {
            tabId: 3,
            title: 'Tab 3',
            url: 'about:blank?3',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          },
        ];

        element.contextManagementInComposeboxEnabled = true;
        element.cacheSubmittedTabs();

        assertEquals(3, element.aimThreadRestoredTabs.length);
        // Newly submitted tabs (tab1, tab2) should be appended chronologically:
        // [tab3, tab1, tab2]
        assertEquals(3, element.aimThreadRestoredTabs[0]!.tabId);
        assertEquals(1, element.aimThreadRestoredTabs[1]!.tabId);
        assertEquals(2, element.aimThreadRestoredTabs[2]!.tabId);
      });

  test(
      'setAimThreadRestoredTabs force-refreshes suggestions when restored tabs are set or cleared',
      async () => {
        searchboxHandler.resetResolver('getRecentTabs');
        searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});

        const tab1 = {
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Emit setAimThreadRestoredTabs callback with non-empty list.
        searchboxCallbackRouterRemote.setAimThreadRestoredTabs([tab1]);
        await microtasksFinished();

        // Verify: refreshTabSuggestions(true) was triggered, so getRecentTabs
        // is called.
        assertEquals(1, searchboxHandler.getCallCount('getRecentTabs'));

        // Reset call count and test with empty list.
        searchboxHandler.resetResolver('getRecentTabs');
        searchboxHandler.setPromiseResolveFor('getRecentTabs', {tabs: []});
        searchboxCallbackRouterRemote.setAimThreadRestoredTabs([]);
        await microtasksFinished();

        // Verify: refreshTabSuggestions is called even when list is empty to
        // clear or update suggestions.
        assertEquals(1, searchboxHandler.getCallCount('getRecentTabs'));
      });

  test('queryAutocomplete passes cursor position', async () => {
    element.input = 'hello';
    await microtasksFinished();

    const inputElement = element.getInputElement();
    inputElement.inputElement.value = 'hello';
    inputElement.inputElement.focus();
    inputElement.inputElement.selectionStart = 3;
    inputElement.inputElement.selectionEnd = 3;

    searchboxHandler.resetResolver('queryAutocomplete');
    element.queryAutocomplete(/*clearMatches=*/ false);

    const args = await searchboxHandler.whenCalled('queryAutocomplete');
    assertDeepEquals(args, [
      0,
      'hello',
      false,
      3,
      SuggestInventory.kDefault,
      false,
      '',
      InputMethod.kKeyboard,
    ]);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        element.input = 'hello';
        await microtasksFinished();

        const inputElement = element.getInputElement();
        inputElement.inputElement.value = 'hello';
        inputElement.inputElement.focus();
        inputElement.inputElement.selectionStart = 3;
        inputElement.inputElement.selectionEnd = 3;

        // Simulate a programming update of the input as happens when, e.g., the
        // user closes the composebox. This update won't be immediately
        // reflected in the DOM.
        element.input = 'hello world';

        // Clear the `queryAutocomplete` called for ZPS.
        searchboxHandler.resetResolver('queryAutocomplete');
        element.queryAutocomplete(/*clearMatches=*/ false);

        const args = await searchboxHandler.whenCalled('queryAutocomplete');
        assertDeepEquals(args, [
          0,
          'hello world',
          false,
          11,
          SuggestInventory.kDefault,
          false,
          '',
          InputMethod.kKeyboard,
        ]);
      });

  test('queries autocomplete on load by default', async () => {
    searchboxHandler.resetResolver('queryAutocomplete');
    const freshComposebox = document.createElement('test-composebox-mixin');
    document.body.appendChild(freshComposebox);
    await microtasksFinished();

    assertEquals(1, searchboxHandler.getCallCount('queryAutocomplete'));
  });

  test(
      'does not query autocomplete on load when queryZpsOnLoad is false',
      async () => {
        searchboxHandler.resetResolver('queryAutocomplete');
        const freshComposebox = document.createElement('test-composebox-mixin');
        // queryZpsOnLoad is read in connectedCallback, so it must be set before
        // the element connects. Contextual Tasks sets it false and drives
        // autocomplete from its own zero-state logic instead.
        freshComposebox.queryZpsOnLoad = false;
        document.body.appendChild(freshComposebox);
        await microtasksFinished();

        assertEquals(0, searchboxHandler.getCallCount('queryAutocomplete'));
      });

  test(
      'Shift+Enter allows inserting a newline when input is focused and not empty',
      async () => {
        element.input = 'Some text';
        await microtasksFinished();

        const inputElement = element.getInputElement();
        inputElement.inputElement.focus();

        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(inputElement.inputElement);

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertFalse(event.defaultPrevented);
      });

  test(
      'Enter prevents inserting a newline and attempts to submit query when focus is not in dropdown',
      async () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: false,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getInputElement().inputElement);

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
      });

  test(
      'Shift+Enter submits dropdown selection when focus is in dropdown',
      async () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getDropdownElement());

        element.getWrapperElement().dispatchEvent(event);
        await microtasksFinished();

        assertTrue(event.defaultPrevented);
      });

  test('autocomplete matches are cleared on submit', async () => {
    element.input = 'Some text';
    await microtasksFinished();

    const event = new KeyboardEvent('keydown', {
      key: 'Enter',
      shiftKey: false,
      bubbles: true,
      cancelable: true,
    });
    element.setActiveElement(element.getInputElement().inputElement);
    element.getWrapperElement().dispatchEvent(event);
    await microtasksFinished();

    const clearResult = await searchboxHandler.whenCalled('stopAutocomplete');
    assertTrue(clearResult);
    assertFalse(element.showDropdown);
    assertEquals(null, element.result);
    assertEquals('', element.lastQueriedInput);
  });

  test('activeQueryId is not reset to -1 when selection cleared and input is empty', async () => {
    element.input = '';
    element.activeQueryId = 0;
    element.lastQueriedInput = '';

    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: '', matches} as AutocompleteResult;
    element.selectedMatchIndex = 0;
    await element.updateComplete;

    element.selectedMatchIndex = -1;
    await element.updateComplete;

    assertEquals(0, element.activeQueryId);
  });

  test('activeQueryId is reset to -1 when selection cleared and input is not empty', async () => {
    element.input = 'Some text';
    element.activeQueryId = 0;
    element.lastQueriedInput = '';

    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: '', matches} as AutocompleteResult;
    element.selectedMatchIndex = 0;
    await element.updateComplete;

    element.selectedMatchIndex = -1;
    await element.updateComplete;

    assertEquals(-1, element.activeQueryId);
  });

  test('smartComposeInlineHint is sliced on sequential typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await microtasksFinished();

    const inputElem = element.getInputElement();
    await simulateUserTextInput(inputElem, 'hello ');

    assertEquals('world', element.smartComposeInlineHint);
    assertEquals('hello ', element.input);

    await simulateUserTextInput(inputElem, 'hello w');

    assertEquals('orld', element.smartComposeInlineHint);
  });

  test('smartComposeInlineHint is cleared on non-matching typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await microtasksFinished();

    const inputElem = element.getInputElement();
    await simulateUserTextInput(inputElem, 'hello!');

    assertEquals('', element.smartComposeInlineHint);
  });

  test(
      'filters tabs from carousel when tab chips to coins flag is enabled',
      async () => {
        loadTimeData.overrideValues({
          tabFaviconChipsToCoinsEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const freshComposebox = document.createElement('test-composebox-mixin');
        document.body.appendChild(freshComposebox);

        const regularFile = ({
                              name: 'image.png',
                              type: 'image/png',
                            } as Partial<ComposeboxFile>) as ComposeboxFile;
        const tabFile = ({
                          name: 'Google',
                          url: 'http://google.com',
                        } as Partial<ComposeboxFile>) as ComposeboxFile;
        freshComposebox.files = new Map([
          ['uuid-1' as unknown as UnguessableToken, regularFile],
          ['uuid-2' as unknown as UnguessableToken, tabFile],
        ]);

        freshComposebox.requestUpdate();
        await microtasksFinished();

        const filteredFiles = freshComposebox.getFilteredCarouselFiles();
        assertEquals(1, filteredFiles.length);
        assertEquals('image.png', filteredFiles[0]!.name);
      });

  test('does not filter tabs from carousel when flag is disabled', async () => {
    loadTimeData.overrideValues({
      tabFaviconChipsToCoinsEnabled: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const freshComposebox = document.createElement('test-composebox-mixin');
    document.body.appendChild(freshComposebox);

    const regularFile = ({
                          name: 'image.png',
                          type: 'image/png',
                        } as Partial<ComposeboxFile>) as ComposeboxFile;
    const tabFile = ({
                      name: 'Google',
                      url: 'http://google.com',
                    } as Partial<ComposeboxFile>) as ComposeboxFile;
    freshComposebox.files = new Map([
      ['uuid-1' as unknown as UnguessableToken, regularFile],
      ['uuid-2' as unknown as UnguessableToken, tabFile],
    ]);

    freshComposebox.requestUpdate();
    await microtasksFinished();

    const filteredFiles = freshComposebox.getFilteredCarouselFiles();
    assertEquals(2, filteredFiles.length);
  });

  test(
      'onOpenDriveUpload suppresses upload if disclaimer restricted',
      async () => {
        searchboxHandler.setResultFor(
            'getDriveDisclaimerStatus',
            Promise.resolve({status: DriveDisclaimerStatus.kRestricted}));

        await element.onOpenDriveUpload();

        assertEquals(
            1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
        assertEquals(0, searchboxHandler.getCallCount('onDriveUploadClicked'));
      });

  test(
      'onOpenDriveUpload triggers upload if disclaimer not accepted',
      async () => {
        searchboxHandler.setResultFor(
            'getDriveDisclaimerStatus',
            Promise.resolve({status: DriveDisclaimerStatus.kNotAccepted}));
        searchboxHandler.setResultFor(
            'onDriveUploadClicked',
            Promise.resolve({response: {files: [], error: null}}));

        await element.onOpenDriveUpload();

        assertEquals(
            1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
        assertEquals(1, searchboxHandler.getCallCount('onDriveUploadClicked'));
      });

  test('onOpenDriveUpload triggers upload if disclaimer accepted', async () => {
    searchboxHandler.setResultFor(
        'getDriveDisclaimerStatus',
        Promise.resolve({status: DriveDisclaimerStatus.kAccepted}));
    searchboxHandler.setResultFor(
        'onDriveUploadClicked',
        Promise.resolve({response: {files: [], error: null}}));

    await element.onOpenDriveUpload();

    assertEquals(1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
    assertEquals(1, searchboxHandler.getCallCount('onDriveUploadClicked'));
  });

  test('addDriveUploads adds files to composebox', async () => {
    const token = {high: 1n, low: 1n} as unknown as UnguessableToken;
    element.addDriveUploads([{
      token,
      mimeType: 'image/png',
      fileName: 'file.png',
      thumbnailUrl: 'thumb',
      iconUrl: 'icon',
    }]);

    await microtasksFinished();
    assertTrue(element.files.has(token));
    const file = element.files.get(token)!;
    assertEquals('file.png', file.name);
    assertEquals('image/png', file.type);
    assertFalse(element.showDropdown);
  });

  test('addDriveUploads handles max files exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kMaxFilesExceeded);
    await microtasksFinished();
    assertEquals(element.i18n('maxFilesReachedError'), element.errorMessage);
  });

  test('addDriveUploads handles size limit exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kSizeLimitExceeded);
    await microtasksFinished();
    assertEquals(
        element.i18n(
            'composeboxFileUploadInvalidTooLarge',
            Math.floor(element.maxFileSize / (1024 * 1024))),
        element.errorMessage);
  });

  test('updateState calls addDriveUploads for drive uploads', async () => {
    const token = {high: 2n, low: 2n} as unknown as UnguessableToken;
    element.state = {
      text: 'hello',
      files: [{
        token,
        mimeType: 'image/png',
        fileName: 'file.png',
        thumbnailUrl: 'thumb',
        iconUrl: 'icon',
      }],
      mode: 0,
      model: 0,
      // <if expr="not is_android">
      smartTabSharingActive: false,
      // </if>
    };
    await microtasksFinished();
    assertTrue(element.files.has(token));
    assertEquals('hello', element.input);
  });

  test('onInputStateChanged updates inputState property', async () => {
    const inputState = {
      allowedModels: [],
      allowedTools: [],
      allowedInputTypes: [],
      activeModel: ModelMode.kGeminiRegular,
      activeTool: ToolMode.kUnspecified,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      inputTypeConfigs: [],
      toolConfigs: [],
      modelConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: 'Hint',
      maxInputsByType: {},
      maxTotalInputs: 5,
      isCanvasQuerySubmitted: false,
    } as unknown as InputState;
    element.onInputStateChanged(inputState);
    await microtasksFinished();
    assertDeepEquals(element.inputState, inputState);
  });

  test('NotifySessionStarted called on connectedCallback', () => {
    assertEquals(1, searchboxHandler.getCallCount('notifySessionStarted'));
  });

  test('clear button title changes with input text', async () => {
    const cancelIcon =
        element.getInputElement().shadowRoot.querySelector('#cancelIcon')!;
    assertEquals(
        loadTimeData.getString('composeboxCancelButtonTitle'),
        cancelIcon.getAttribute('title'));

    await simulateUserTextInput(element.getInputElement(), 'Test');
    assertEquals(
        loadTimeData.getString('composeboxCancelButtonTitleInput'),
        cancelIcon.getAttribute('title'));
  });

  test(
      'updates state from state property with regular files and modes',
      async () => {
        assertEquals(0, searchboxHandler.getCallCount('setActiveToolMode'));
        assertEquals(0, searchboxHandler.getCallCount('setActiveModelMode'));

        const mockFile =
            new File(['test'], 'test.pdf', {type: 'application/pdf'});
        element.state = {
          text: 'hello world',
          files: [{file: mockFile}],
          mode: ToolMode.kDeepSearch,
          model: ModelMode.kGeminiRegular,
          // <if expr="not is_android">
          smartTabSharingActive: false,
          // </if>
        };
        await searchboxHandler.whenCalled('addFileContext');
        await element.updateComplete;

        assertEquals('hello world', element.input);
        assertEquals(1, element.files.size);
        assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
        assertEquals(
            ToolMode.kDeepSearch,
            searchboxHandler.getArgs('setActiveToolMode')[0]);
        assertEquals(1, searchboxHandler.getCallCount('setActiveModelMode'));
        assertEquals(
            ModelMode.kGeminiRegular,
            searchboxHandler.getArgs('setActiveModelMode')[0]);
      });

  test('navigates matches with ArrowDown and ArrowUp', async () => {
    const input = element.getInputElement().inputElement;
    const matchesElement = element.getDropdownElement();

    element.result = {input: '', matches: []} as unknown as AutocompleteResult;
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(-1, matchesElement.selectedMatchIndex);

    const matches = [
      {fillIntoEdit: 'test1'} as AutocompleteMatch,
      {fillIntoEdit: 'test2'} as AutocompleteMatch,
    ];
    element.result = {input: 'test', matches} as AutocompleteResult;
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(1, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowDown',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    element.dropdownNeeded = false;
    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);
  });

  test('selects first or last match with PageUp and PageDown', async () => {
    const input = element.getInputElement().inputElement;
    const matchesElement = element.getDropdownElement();

    const matches = [
      {fillIntoEdit: 'test1'} as AutocompleteMatch,
      {fillIntoEdit: 'test2'} as AutocompleteMatch,
      {fillIntoEdit: 'test3'} as AutocompleteMatch,
    ];
    element.result = {input: 'test', matches} as AutocompleteResult;
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'PageDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(2, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'PageUp', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);

    input.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'PageDown',
      altKey: true,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(0, matchesElement.selectedMatchIndex);
  });

  test(
      'PageDown and PageUp are ignored when no matches are available',
      async () => {
        const input = element.getInputElement().inputElement;
        const matchesElement = element.getDropdownElement();

        input.dispatchEvent(new KeyboardEvent(
            'keydown', {key: 'PageDown', bubbles: true, composed: true}));
        await element.updateComplete;
        assertEquals(-1, matchesElement.selectedMatchIndex);
      });

  test('Tab behavior when focus is in input', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;
    const matchesElement = element.getDropdownElement();

    const matches = [{fillIntoEdit: 'match1'} as AutocompleteMatch];
    element.result = {input: 'tes', matches} as AutocompleteResult;
    await microtasksFinished();

    matchesElement.selectNext();
    assertEquals(0, matchesElement.selectedMatchIndex);
    input.focus();

    input.dispatchEvent(new KeyboardEvent(
        'keydown',
        {key: 'Tab', shiftKey: true, bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(-1, matchesElement.selectedMatchIndex);

    await simulateUserTextInput(inputElem, 'tes');
    element.smartComposeInlineHint = 't';
    await element.updateComplete;

    const tabEvent = new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true});
    input.dispatchEvent(tabEvent);
    await microtasksFinished();

    assertEquals('test', input.value);
    assertTrue(tabEvent.defaultPrevented);
  });

  test('Tab on last dropdown match unselects active match', async () => {
    const matchesElement = element.getDropdownElement();
    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
      {fillIntoEdit: 'match2', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: 'm', matches} as AutocompleteResult;
    await microtasksFinished();

    matchesElement.selectNext();
    matchesElement.selectNext();
    assertEquals(1, matchesElement.selectedMatchIndex);

    await matchesElement.updateComplete;
    element.setActiveElement(matchesElement);

    const tabEvent = new KeyboardEvent(
        'keydown',
        {key: 'Tab', bubbles: true, cancelable: true, composed: true});
    matchesElement.dispatchEvent(tabEvent);
    await element.updateComplete;

    assertEquals(-1, matchesElement.selectedMatchIndex);
    assertFalse(tabEvent.defaultPrevented);
  });

  test('Tab in dropdown is ignored when key modifiers are active', async () => {
    const matchesElement = element.getDropdownElement();
    const matches = [
      {fillIntoEdit: 'match1', supportsDeletion: false} as AutocompleteMatch,
      {fillIntoEdit: 'match2', supportsDeletion: false} as AutocompleteMatch,
    ];
    element.result = {input: 'm', matches} as AutocompleteResult;
    await element.updateComplete;

    matchesElement.selectNext();
    matchesElement.selectNext();
    await matchesElement.updateComplete;
    await element.updateComplete;
    element.setActiveElement(matchesElement);
    const tabEventCtrl = new KeyboardEvent('keydown', {
      key: 'Tab',
      ctrlKey: true,
      bubbles: true,
      cancelable: true,
    });
    matchesElement.dispatchEvent(tabEventCtrl);
    await element.updateComplete;
    assertEquals(1, matchesElement.selectedMatchIndex);
  });

  test('Tab in dropdown is ignored when no matches are available', async () => {
    const matchesElement = element.getDropdownElement();

    const tabEventNoMatch = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
    });
    matchesElement.dispatchEvent(tabEventNoMatch);
    await element.updateComplete;
    assertEquals(-1, matchesElement.selectedMatchIndex);
  });

  test('Smart Compose hint is hidden during backspacing', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;

    await simulateUserTextInput(inputElem, 'tes');
    element.smartComposeInlineHint = 't';
    await element.updateComplete;

    assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Backspace'}));
    await microtasksFinished();

    assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
  });

  test('Smart Compose hint is hidden when cursor is not at end', async () => {
    element.smartComposeEnabled = true;
    const inputElem = element.getInputElement();
    const input = inputElem.inputElement;

    await simulateUserTextInput(inputElem, 'test');
    element.smartComposeInlineHint = 'a';
    await element.updateComplete;

    assertTrue(!!inputElem.shadowRoot.querySelector('#smartCompose'));

    input.selectionStart = 2;
    input.selectionEnd = 2;
    inputElem.requestUpdate();
    await microtasksFinished();

    assertFalse(!!inputElem.shadowRoot.querySelector('#smartCompose'));
  });

  test(
      'Smart Compose hint is hidden when it wraps in the middle of a word',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wrap')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'wrap';
          await element.updateComplete;

          assertFalse(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test(
      'Smart Compose hint is NOT hidden when only full hint wraps but first word fits',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wraps')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'fits wraps';
          await element.updateComplete;

          assertTrue(!!inputElement.shadowRoot.querySelector('#smartCompose'));
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test(
      'Tab key does not accept Smart Compose when hidden by wrapping',
      async () => {
        const inputElement = element.getInputElement();
        const input = inputElement.inputElement as HTMLTextAreaElement;

        const originalMeasureText =
            CanvasRenderingContext2D.prototype.measureText;
        try {
          CanvasRenderingContext2D.prototype.measureText = function(
              text: string) {
            if (text.includes('wrap')) {
              return {width: 150} as TextMetrics;
            }
            return {width: 50} as TextMetrics;
          };
          Object.defineProperty(
              input, 'clientWidth', {configurable: true, get: () => 100});

          element.smartComposeEnabled = true;
          await simulateUserTextInput(inputElement, 'tes.');
          element.smartComposeInlineHint = 'wrap';
          await element.updateComplete;

          element.setActiveElement(input);
          const tabEvent = new KeyboardEvent('keydown', {
            key: 'Tab',
            bubbles: true,
            cancelable: true,
          });
          element.getWrapperElement().dispatchEvent(tabEvent);
          await element.updateComplete;

          assertEquals('tes.', element.input);
        } finally {
          CanvasRenderingContext2D.prototype.measureText = originalMeasureText;
        }
      });

  test('sets and deletes visual selection thumbnail', async () => {
    assertFalse(element.showFileCarousel);

    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    const testToken =
        '12345678901234567890123456789012' as unknown as UnguessableToken;
    searchboxCallbackRouterRemote.addFileContext(testToken, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: true,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await element.updateComplete;

    assertTrue(element.showFileCarousel);
    const fileCarousel =
        element.shadowRoot.querySelector<ComposeboxFileCarouselElement>(
            '#carousel')!;
    assertTrue(!!fileCarousel);
    await fileCarousel.updateComplete;

    assertEquals(1, fileCarousel.files.length);
    assertEquals(testToken, fileCarousel.files[0]!.uuid);
    assertEquals(thumbnailUrl, fileCarousel.files[0]!.dataUrl);
    assertTrue(fileCarousel.files[0]!.isDeletable);

    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);
    const removeImgButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertTrue(!!removeImgButton);
    removeImgButton.click();
    await element.updateComplete;

    assertEquals(1, searchboxHandler.getCallCount('deleteContext'));
    const [idArg, fromChip] = searchboxHandler.getArgs('deleteContext')[0]!;
    assertEquals(testToken, idArg);
    assertFalse(fromChip);
    assertFalse(element.showFileCarousel);
    assertFalse(!!element.shadowRoot.querySelector('#carousel'));
  });

  test('setVisualSelectionThumbnail not deletable', async () => {
    const thumbnailUrl = 'data:image/png;base64,sometestdata';
    const testToken =
        '12345678901234567890123456789013' as unknown as UnguessableToken;
    searchboxCallbackRouterRemote.addFileContext(testToken, {
      fileName: 'Visual Selection',
      mimeType: 'image/png',
      imageDataUrl: thumbnailUrl,
      isDeletable: false,
      selectionTime: new Date(),
    } as SelectedFileInfo);
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await element.updateComplete;

    assertTrue(element.showFileCarousel);
    const fileCarousel =
        element.shadowRoot.querySelector<ComposeboxFileCarouselElement>(
            '#carousel')!;
    assertTrue(!!fileCarousel);
    await fileCarousel.updateComplete;

    assertEquals(1, fileCarousel.files.length);
    assertFalse(fileCarousel.files[0]!.isDeletable);

    const fileThumbnail =
        fileCarousel.shadowRoot.querySelector('cr-composebox-file-thumbnail');
    assertTrue(!!fileThumbnail);
    const removeButton =
        fileThumbnail.shadowRoot.querySelector<HTMLElement>('#removeImgButton');
    assertEquals(null, removeButton);
  });

  test(
      'clears suggestInventory when properties change and has content',
      async () => {
        element.suggestInventory = SuggestInventory.kTravel;
        assertEquals(SuggestInventory.kTravel, element.suggestInventory);

        const emptyInputState =
            new MockInputState({activeTool: ToolMode.kUnspecified});
        element.onInputStateChanged(emptyInputState);
        await element.updateComplete;
        assertEquals(SuggestInventory.kTravel, element.suggestInventory);

        const toolInputState =
            new MockInputState({activeTool: ToolMode.kDeepSearch});
        element.onInputStateChanged(toolInputState);
        await element.updateComplete;
        assertEquals(null, element.suggestInventory);
      });

  test('delete tool chip', async () => {
    element.composeboxSource = 'TestEmbedder';
    const inputState = new MockInputState({activeTool: ToolMode.kDeepSearch});
    element.onInputStateChanged(inputState);
    await element.updateComplete;

    element.handleToolClick(ToolMode.kDeepSearch);
    await element.updateComplete;

    assertEquals(1, searchboxHandler.getCallCount('setActiveToolMode'));
    assertEquals(
        ToolMode.kUnspecified,
        searchboxHandler.getArgs('setActiveToolMode')[0]);

    const metricName =
        'ContextualSearch.UserAction.InputStateDeletion.TestEmbedder';
    assertEquals(
        1,
        metrics.count(metricName, ContextualSearchInputStateDeletionType.TOOL));
  });

  test('setDefaultModel uses activeModel from backend', async () => {
    const inputState = new MockInputState({
      allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
      activeModel: ModelMode.kGeminiPro,
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          aimUrlParams: [],
          menuLabel: 'Regular',
          hintText: 'Hint Regular',
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          aimUrlParams: [{paramKey: 'xyz', paramValue: '1'}],
          menuLabel: 'Pro',
          hintText: 'Hint Pro',
          menuTooltip: '',
        },
      ],
      modelSectionConfig: null,
    });
    element.onInputStateChanged(inputState);
    await element.updateComplete;

    element.setDefaultModel();

    assertEquals(1, searchboxHandler.getCallCount('setActiveModelMode'));
    assertEquals(
        ModelMode.kGeminiPro,
        searchboxHandler.getArgs('setActiveModelMode')[0]);
  });

  test('empty input computes canSubmitFilesAndInput as false', async () => {
    await simulateUserTextInput(element.getInputElement(), '');
    assertFalse(element.canSubmitFilesAndInput);
  });

  test(
      'whitespace input computes canSubmitFilesAndInput as false', async () => {
        await simulateUserTextInput(element.getInputElement(), ' ');
        assertFalse(element.canSubmitFilesAndInput);
      });

  test(
      'submitQuery is a no-op when canSubmitFilesAndInput is false',
      async () => {
        assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
        assertEquals(0, searchboxHandler.getCallCount('openAutocompleteMatch'));

        await simulateUserTextInput(element.getInputElement(), '');
        assertFalse(element.canSubmitFilesAndInput);

        element.submitQuery();
        assertEquals(0, searchboxHandler.getCallCount('submitQuery'));
        assertEquals(0, searchboxHandler.getCallCount('openAutocompleteMatch'));
      });

  test('metrics are recorded for ToolMode clicks', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';
    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kDeepSearch},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.DEEP_RESEARCH));
    assertEquals(1, metrics.count(`${metricName}.DeepResearch`, 0));

    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kCanvas},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.CANVAS));
    assertEquals(1, metrics.count(`${metricName}.Canvas`, 0));

    element.onToolClick(new CustomEvent('tool-click', {
      detail: {toolMode: ToolMode.kImageGen},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE_GEN));
    assertEquals(1, metrics.count(`${metricName}.ImageGen`, 0));
  });

  test('metrics are recorded for ModelMode clicks', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';
    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProAutoroute},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.AUTO_MODEL));
    assertEquals(1, metrics.count(`${metricName}.AutoModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiPro},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.THINKING_MODEL));
    assertEquals(1, metrics.count(`${metricName}.ThinkingModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiRegular},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.REGULAR_MODEL));
    assertEquals(1, metrics.count(`${metricName}.RegularModel`, 0));

    element.onModelClick(new CustomEvent('model-click', {
      detail: {model: ModelMode.kGeminiProNoGenUi},
    }));
    assertEquals(1, metrics.count(metricName, ContextType.PRO_NO_GEN_UI_MODEL));
    assertEquals(1, metrics.count(`${metricName}.ProNoGenUiModel`, 0));
  });

  test('metrics are recorded for file uploads', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';

    element.onOpenImageUpload();
    assertEquals(1, metrics.count(metricName, ContextType.IMAGE));
    assertEquals(1, metrics.count(`${metricName}.Image`, 0));

    element.onOpenFileUpload();
    assertEquals(1, metrics.count(metricName, ContextType.FILE));
    assertEquals(1, metrics.count(`${metricName}.File`, 0));
  });

  test('metrics are recorded for tab additions', () => {
    element.composeboxSource = 'TestEmbedder';

    const metricName =
        'TestEmbedder.AimEntrypoint.AimPopup.ContextualElement.Clicked';

    element.onAddTabContext(new CustomEvent('add-tab-context', {
      detail: {
        id: 1,
        title: 'Title',
        url: 'http://test.com',
        delayUpload: false,
        origin: TabUploadOrigin.OTHER,
      },
    }));
    assertEquals(1, metrics.count(metricName, ContextType.TAB));
    assertEquals(1, metrics.count(`${metricName}.Tab`, 0));
  });

  test('session abandoned on cancel button click', async () => {
    element.suggestInventory = SuggestInventory.kTravel;
    await element.updateComplete;

    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', element);
    const cancelIcon =
        element.getInputElement().shadowRoot.querySelector<HTMLElement>(
            '#cancelIcon')!;
    cancelIcon.click();
    const event = await whenCloseComposebox;
    assertEquals('', event.detail.composeboxText);
    assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
    assertEquals(null, element.suggestInventory);
  });

  test('session abandoned on esc click', async () => {
    element.closeOnEscape = true;
    await simulateUserTextInput(element.getInputElement(), 'test');
    element.suggestInventory = SuggestInventory.kTravel;
    await element.updateComplete;

    const whenCloseComposebox =
        eventToPromise<CustomEvent<{composeboxText: string}>>(
            'close-composebox', element);
    element.getWrapperElement().dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape'}));
    await element.updateComplete;

    const event = await whenCloseComposebox;
    assertEquals('test', event.detail.composeboxText);
    assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
    assertEquals(null, element.suggestInventory);
  });

  test(
      'esc clears input instead of closing when closeOnEscape is false and has content',
      async () => {
        element.closeOnEscape = false;
        await simulateUserTextInput(element.getInputElement(), 'test');
        await element.updateComplete;

        const closePromise = eventToPromise('close-composebox', element);
        let closed = false;
        closePromise.then(() => closed = true);

        element.getWrapperElement().dispatchEvent(
            new KeyboardEvent('keydown', {key: 'Escape'}));
        await element.updateComplete;

        assertFalse(closed);
        assertEquals('', element.getInputElement().input);
        assertEquals(1, searchboxHandler.getCallCount('clearFiles'));
      });

  test(
      'clearAllInputs clears restored tabs if querySubmitted is false',
      async () => {
        element.contextManagementInComposeboxEnabled = false;
        element.aimThreadRestoredTabs = [{
          tabId: 1,
          title: 'Stale Tab',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        }];
        element.hasCachedSubmittedTabsThisTurn = true;

        // Call `clearAllInputs` with `querySubmitted = true`.
        element.clearAllInputs(
            /* querySubmitted= */ true,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();

        // Verify: `aimThreadRestoredTabs` is NOT cleared.
        assertEquals(1, element.aimThreadRestoredTabs.length);
        assertTrue(element.hasCachedSubmittedTabsThisTurn);

        // Call `clearAllInputs` with `querySubmitted = false`.
        element.clearAllInputs(
            /* querySubmitted= */ false,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();

        // Verify: `aimThreadRestoredTabs` is cleared.
        assertEquals(0, element.aimThreadRestoredTabs.length);
        assertFalse(element.hasCachedSubmittedTabsThisTurn);
      });

  test('Omnibox keepMenuOpenOnTabSelect returns false by default', () => {
    assertFalse(element.keepMenuOpenOnTabSelect);
  });

  test(
      'keepMenuOpenForMultiSelection is gated by keepMenuOpenOnTabSelect',
      async () => {
        element.contextManagementInComposeboxEnabled = true;
        let openMenuCalled = false;
        element.getContextEntrypointElement = () => {
          return {
            openMenuForMultiSelection: () => {
              openMenuCalled = true;
            },
          } as ContextualEntrypointAndMenuElement;
        };

        // Gating flag off: returns early
        Object.defineProperty(element, 'keepMenuOpenOnTabSelect', {
          get: () => false,
          configurable: true,
        });
        await element.keepMenuOpenForMultiSelection();
        assertFalse(openMenuCalled);

        // Gating flag on: calls openMenuForMultiSelection
        Object.defineProperty(element, 'keepMenuOpenOnTabSelect', {
          get: () => true,
          configurable: true,
        });
        await element.keepMenuOpenForMultiSelection();
        assertTrue(openMenuCalled);

        // Context management disabled: always keeps menu open regardless of
        // gating flag
        element.contextManagementInComposeboxEnabled = false;
        Object.defineProperty(element, 'keepMenuOpenOnTabSelect', {
          get: () => false,
          configurable: true,
        });
        openMenuCalled = false;
        await element.keepMenuOpenForMultiSelection();
        assertTrue(openMenuCalled);
      });

  test(
      'keepMenuOpenForMultiSelection called on add/delete tab context',
      async () => {
        element.contextManagementInComposeboxEnabled = true;
        let keepMenuOpenCalled = false;
        element.keepMenuOpenForMultiSelection = () => {
          keepMenuOpenCalled = true;
          return Promise.resolve();
        };

        await element.onAddTabContext(new CustomEvent('add-tab-context', {
          detail: {
            id: 1,
            title: 'Test',
            url: 'about:blank',  // Mojo converts obj to str.
            delayUpload: false,
            origin: TabUploadOrigin.CONTEXT_MENU,
          },
        }));
        assertTrue(keepMenuOpenCalled);

        keepMenuOpenCalled = false;
        await element.onDeleteTabContext(new CustomEvent('delete-tab-context', {
          detail: {
            tabId: 1,
          },
        }));
        assertTrue(keepMenuOpenCalled);
      });

  test('onContextMenuClosed sets shareTabsFlyoutOpen to false', async () => {
    element.shareTabsFlyoutOpen = true;
    await element.onContextMenuClosed();
    assertFalse(element.shareTabsFlyoutOpen);
  });

  // Restored tabs are tabs that the server has said it has received. These are
  // used as tabs that persist across queries.
  test(
      'clearAllInputs respects contextManagementInComposeboxEnabled' +
          ' flag for resetRestoredTabs',
      async () => {
        element.aimThreadRestoredTabs = [{
          tabId: 1,
          title: 'Restored Tab',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        }];

        // Query submitted (querySubmitted = true): restored tabs are NOT
        // reset regardless of flag.
        element.clearAllInputs(
            /* querySubmitted= */ true,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();
        assertEquals(1, element.aimThreadRestoredTabs.length);

        // Flag OFF: "clearAllInputs(querySubmitted = false)" resets restored
        // tabs (clear all button pressed).
        element.contextManagementInComposeboxEnabled = false;
        element.clearAllInputs(
            /* querySubmitted= */ false,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();
        assertEquals(0, element.aimThreadRestoredTabs.length);

        // Reset restored tabs.
        element.aimThreadRestoredTabs = [{
          tabId: 1,
          title: 'Restored Tab',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        }];

        // Explicit new thread / resetRestoredTabs clears restored tabs
        // regardless of flag.
        element.resetRestoredTabs();
        await microtasksFinished();
        assertEquals(0, element.aimThreadRestoredTabs.length);

        // Reset restored tabs.
        element.aimThreadRestoredTabs = [{
          tabId: 1,
          title: 'Restored Tab',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        }];

        // Flag ON: "clearAllInputs(querySubmitted = false)" (clear all button
        // pressed) preserves restored tabs.
        element.contextManagementInComposeboxEnabled = true;
        element.clearAllInputs(
            /* querySubmitted= */ false,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();
        assertEquals(1, element.aimThreadRestoredTabs.length);

        // Query submitted "(querySubmitted = true)": restored tabs are NOT
        // reset regardless of flag.
        element.clearAllInputs(
            /* querySubmitted= */ true,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();
        assertEquals(1, element.aimThreadRestoredTabs.length);

        // Explicit new thread / resetRestoredTabs clears restored tabs
        // regardless of flag.
        element.resetRestoredTabs();
        await microtasksFinished();
        assertEquals(0, element.aimThreadRestoredTabs.length);

        // Reset restored tabs.
        element.aimThreadRestoredTabs = [{
          tabId: 1,
          title: 'Restored Tab',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        }];

        // Flag ON, but source is Omnibox: "clearAllInputs(querySubmitted =
        // false)" (clear all button pressed) resets restored tabs.
        element.contextManagementInComposeboxEnabled = true;
        element.composeboxSource = 'Omnibox';
        element.clearAllInputs(
            /* querySubmitted= */ false,
            /* shouldBlockAutoSuggestedTabs= */ false);
        await microtasksFinished();
        assertEquals(0, element.aimThreadRestoredTabs.length);
      });

  test(
      'undeletableFiles preserves non-deletable files but' +
          ' after, submitCleanup clears tabs',
      async () => {
        const dummyToken1 = {
          high: 1n,
          low: 1n,
        } as unknown as UnguessableToken;
        const dummyToken2 = {
          high: 2n,
          low: 2n,
        } as unknown as UnguessableToken;

        const undeletableFile = ComposeboxFile.createFromFile(
            dummyToken1, {name: 'file.pdf', type: 'application/pdf'},
            ContextUploadStatus.kUploadSuccessful, {isDeletable: false});

        const tabFile = ComposeboxFile.createFromTab(
            dummyToken2, 123, 'Tab Title',
            {url: 'about:blank'} as unknown as Url, {isDeletable: false});

        element.files = new Map([
          [dummyToken1, undeletableFile],
          [dummyToken2, tabFile],
        ]);
        element.addedTabsIds = new Map([[123, dummyToken2]]);

        // Submit cleanup deletes active turn tabs regardless of `isDeletable`.
        element.submitCleanup();
        await microtasksFinished();

        // Verify: tab is deleted, but non-deletable file remains.
        assertFalse(element.files.has(dummyToken2));
        assertTrue(element.files.has(dummyToken1));
      });

  test(
      'onVoicePermissionChanged updates isListening and dispatches event',
      async () => {
        element.inVoiceSearchMode = true;
        element.hasVoiceSearchError = false;
        element.isListening = true;
        await element.updateComplete;

        let eventFired = false;
        let eventDetail: any = null;
        element.addEventListener(
            'voice-permission-prompt-changed', (e: Event) => {
              eventFired = true;
              eventDetail = (e as CustomEvent).detail;
            });

        element.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 100,
              },
            }));
        await element.updateComplete;

        assertTrue(eventFired);
        assertEquals(100, eventDetail.height);
        assertEquals(100, eventDetail.width);
        assertTrue(eventDetail.isOpened);
        assertFalse(element.isListening);

        // Reset tracker and test closing event:
        eventFired = false;
        eventDetail = null;

        element.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: false,
                height: 0,
                width: 0,
              },
            }));
        await element.updateComplete;

        assertTrue(eventFired);
        assertFalse(eventDetail.isOpened);
        assertTrue(element.isListening);
      });

  test('voice permission opened event not fired if size is 0', async () => {
    element.inVoiceSearchMode = true;
    element.hasVoiceSearchError = false;
    element.isListening = true;
    await element.updateComplete;

    let eventFired = false;
    element.addEventListener('voice-permission-prompt-changed', () => {
      eventFired = true;
    });

    // Directly call mixin event handler with height 0 width 0.
    element.onVoicePermissionChanged(
        new CustomEvent('voice-permission-changed', {
          detail: {
            isOpened: true,
            height: 0,
            width: 0,
          },
        }));
    await element.updateComplete;

    assertFalse(eventFired);
    assertFalse(element.isListening);
  });

  // <if expr="not is_android">
  test(
      'connectedCallback calls getSmartTabSharingActive when' +
          ' smartTabSharingVisible pre-set to true',
      async () => {
        searchboxHandler.setResultMapperFor(
            'getSmartTabSharingActive', () => Promise.resolve({active: true}));

        const newElement = document.createElement('test-composebox-mixin');
        newElement.smartTabSharingVisible = true;
        document.body.appendChild(newElement);
        await searchboxHandler.whenCalled('getSmartTabSharingActive');
        await microtasksFinished();
        await newElement.updateComplete;

        assertEquals(
            1, searchboxHandler.getCallCount('getSmartTabSharingActive'));
        assertTrue(newElement.smartTabSharingActive);
        document.body.removeChild(newElement);
      });

  test(
      'connectedCallback does NOT call getSmartTabSharingActive when' +
          ' smartTabSharingVisible is false',
      async () => {
        const newElement = document.createElement('test-composebox-mixin');
        newElement.smartTabSharingVisible = false;
        document.body.appendChild(newElement);
        await newElement.updateComplete;

        assertEquals(
            0, searchboxHandler.getCallCount('getSmartTabSharingActive'));
        assertFalse(newElement.smartTabSharingActive);
        document.body.removeChild(newElement);
      });

  test(
      'host template .prop binding triggers getSmartTabSharingActive' +
          ' at child mount',
      async () => {
        searchboxHandler.setResultMapperFor(
            'getSmartTabSharingActive', () => Promise.resolve({active: true}));

        const host = document.createElement('div');
        host.innerHTML = getTrustedHtml(`
          <test-composebox-mixin smart-tab-sharing-visible></test-composebox-mixin>
        `);
        document.body.appendChild(host);

        const newElement = host.querySelector<TestComposeboxMixinElement>(
            'test-composebox-mixin');
        assertTrue(!!newElement);

        await searchboxHandler.whenCalled('getSmartTabSharingActive');
        await microtasksFinished();
        await newElement.updateComplete;

        assertEquals(
            1, searchboxHandler.getCallCount('getSmartTabSharingActive'));
        assertTrue(newElement.smartTabSharingActive);
        document.body.removeChild(host);
      });
  // </if>
});
