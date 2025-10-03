// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import {SelectionLineState} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {createAutocompleteMatch, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchboxMatchElement} from 'chrome://new-tab-page/new_tab_page.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import {assertArrayEquals, assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('CrComponentsRealboxMatchTest', () => {
  let matchEl: SearchboxMatchElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    matchEl = document.createElement('cr-searchbox-match');
    matchEl.match = createAutocompleteMatch();
    matchEl.matchIndex = 0;
    document.body.appendChild(matchEl);
  });

  test('MousedownEventsAreSentToHandler', async () => {
    const matchIndex = 2;
    const destinationUrl = {url: 'http://google.com'};
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    matchEl.dispatchEvent(new MouseEvent('mousedown'));
    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(matchIndex, args.line);
    assertEquals(destinationUrl, args.url);
    assertEquals(NavigationPredictor.kMouseDown, args.navigationPredictor);
  });

  test('ClickNavigates', async () => {
    const matchIndex = 1;
    const destinationUrl = {url: 'http://google.com'};
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    const clickEvent = new MouseEvent('click', {
      button: 0,
      cancelable: true,
      altKey: true,
      ctrlKey: false,
      metaKey: true,
      shiftKey: false,
    });
    matchEl.dispatchEvent(clickEvent);
    assertTrue(clickEvent.defaultPrevented);
    const clickArgs =
        await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertDeepEquals(
        [
          matchIndex,
          destinationUrl,
          true,
          clickEvent.button,
          clickEvent.altKey,
          clickEvent.ctrlKey,
          clickEvent.metaKey,
          clickEvent.shiftKey,
        ],
        [
          clickArgs.line,
          clickArgs.url,
          clickArgs.areMatchesShowing,
          clickArgs.mouseButton,
          clickArgs.altKey,
          clickArgs.ctrlKey,
          clickArgs.metaKey,
          clickArgs.shiftKey,
        ]);
    testProxy.handler.reset();

    // Right clicks are ignored.
    const rightClickEvent =
        new MouseEvent('click', {button: 2, cancelable: true});
    matchEl.dispatchEvent(rightClickEvent);
    assertFalse(rightClickEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    // Middle clicks are accepted.
    const middleClickEvent =
        new MouseEvent('click', {button: 1, cancelable: true});
    matchEl.dispatchEvent(middleClickEvent);
    assertTrue(middleClickEvent.defaultPrevented);
    const middleClickArgs =
        await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(matchIndex, middleClickArgs.line);
    assertDeepEquals(destinationUrl, middleClickArgs.url);
    assertEquals(1, middleClickArgs.mouseButton);
  });

  test('ClickFiresEvent', async () => {
    const clickPromise = eventToPromise('match-click', matchEl);

    const clickEvent = new MouseEvent('click', {
      button: 0,
      cancelable: true,
      altKey: true,
      ctrlKey: false,
      metaKey: true,
      shiftKey: false,
    });
    matchEl.dispatchEvent(clickEvent);

    await clickPromise;
  });

  test('DeleteButtonRemovesMatch', async () => {
    const matchIndex = 1;
    const destinationUrl = {url: 'http://google.com'};
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    matchEl.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(matchIndex, keydownArgs.line);
    assertEquals(destinationUrl, keydownArgs.url);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Pressing 'Enter' the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
    testProxy.handler.reset();

    matchEl.$.remove.click();
    const clickArgs =
        await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(matchIndex, clickArgs.line);
    assertEquals(destinationUrl, clickArgs.url);
    assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
    // Clicking the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('UpdateSelectionUpdatesClasses', async () => {
    // Add keyword chip and 2 actions.
    const match = createAutocompleteMatch();
    match.keywordChipHint = 'keyword';
    match.actions.push({
      hint: 'hint',
      suggestionContents: 'suggestionContents',
      iconPath: 'iconPath',
      a11yLabel: 'a11yLabel',
    });
    match.actions.push({...match.actions[0]!});
    matchEl.match = match;
    await microtasksFinished();

    // When a match is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertFalse(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When a match is unselected.
    matchEl.selection = {
      line: 1,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertFalse(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.$.remove.classList.contains('selected'));

    // When the keyword chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kKeywordMode,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertTrue(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the 1st action chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([true, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the 2nd action chip is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 1,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, true], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertFalse(!!matchEl.shadowRoot.querySelector('#remove.selected'));

    // When the remove button is selected.
    matchEl.selection = {
      line: 0,
      state: SelectionLineState.kFocusedButtonRemoveSuggestion,
      actionIndex: 0,
    };
    await microtasksFinished();
    assertTrue(
        !!matchEl.shadowRoot.querySelector('#focus-indicator.selected-within'));
    assertFalse(!!matchEl.shadowRoot.querySelector('#keyword.selected'));
    assertArrayEquals([false, false], [
      ...matchEl.shadowRoot.querySelectorAll(
          '#actions-container cr-searchbox-action'),
    ].map(action => action.classList.contains('selected')));
    assertTrue(!!matchEl.shadowRoot.querySelector('#remove.selected'));
  });
});
