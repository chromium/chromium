// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/searchbox/searchbox_match.js';

import {NavigationPredictor} from 'chrome://resources/cr_components/searchbox/omnibox.mojom-webui.js';
import {SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxMatchElement} from 'chrome://resources/cr_components/searchbox/searchbox_match.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createAutocompleteMatch} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('CrComponentsRealboxMatchTest', () => {
  let matchEl: SearchboxMatchElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    matchEl = document.createElement('cr-searchbox-match');
    matchEl.match = createAutocompleteMatch();
    matchEl.matchIndex = 0;
    document.body.appendChild(matchEl);

    await flushTasks();
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
});
