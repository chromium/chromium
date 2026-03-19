// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_match.js';

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxMatchElement} from 'chrome://resources/cr_components/composebox/composebox_match.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, installMock} from './composebox_test_utils.js';

suite('ComposeboxMatch', () => {
  let matchElement: ComposeboxMatchElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock as unknown as PageHandlerRemote, new PageCallbackRouter(),
            searchboxHandler as unknown as SearchboxPageHandlerRemote,
            new SearchboxPageCallbackRouter())));

    matchElement = document.createElement('cr-composebox-match');
    document.body.appendChild(matchElement);
    await microtasksFinished();
  });

  test('renders match contents', async () => {
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
    });
    await microtasksFinished();

    const contents = matchElement.$.textContainer;
    assertEquals('test contents', contents.textContent.trim());
  });

  test('click triggers openAutocompleteMatch', async () => {
    const match = createAutocompleteMatch({
      destinationUrl: 'https://google.com/',
    });
    matchElement.match = match;
    matchElement.matchIndex = 1;
    await microtasksFinished();

    matchElement.click();

    const [index, url] =
        await searchboxHandler.whenCalled('openAutocompleteMatch');
    assertEquals(1, index);
    assertEquals(match.destinationUrl, url);
  });

  test('clicking remove button triggers deleteAutocompleteMatch', async () => {
    const match = createAutocompleteMatch({
      destinationUrl: 'https://google.com/',
      supportsDeletion: true,
    });
    matchElement.match = match;
    matchElement.matchIndex = 1;
    await microtasksFinished();

    matchElement.$.remove.click();

    const [index, url] =
        await searchboxHandler.whenCalled('deleteAutocompleteMatch');
    assertEquals(1, index);
    assertEquals(match.destinationUrl, url);
  });

  test('default event is prevented when clicking remove button', async () => {
    const match = createAutocompleteMatch({
      supportsDeletion: true,
    });
    matchElement.match = match;
    await microtasksFinished();

    const event = new MouseEvent('mousedown', {cancelable: true});
    matchElement.$.remove.dispatchEvent(event);

    assertTrue(event.defaultPrevented);
  });

  test('focusing a match fires `match-focusin` event', async () => {
    matchElement.matchIndex = 2;
    const whenMatchFocusin = eventToPromise('match-focusin', matchElement);

    matchElement.dispatchEvent(new FocusEvent('focusin'));

    const event = await whenMatchFocusin;
    assertEquals(2, event.detail.index);
  });

  test('clamps lines for deep search', async () => {
    assertStyle(matchElement.$.textContainer, '-webkit-line-clamp', 'none');

    matchElement.toolMode = ToolMode.kDeepSearch;
    await microtasksFinished();
    assertEquals('2', matchElement.style.getPropertyValue('--clamp-line-num'));
    assertStyle(matchElement.$.textContainer, '-webkit-line-clamp', '2');

    matchElement.toolMode = ToolMode.kUnspecified;
    await microtasksFinished();
    assertStyle(matchElement.$.textContainer, '-webkit-line-clamp', 'none');
  });

  test(
      'clamps lines according to `overrideClampLineNum` property', async () => {
        const el: ComposeboxMatchElement =
            document.createElement('cr-composebox-match');
        el.overrideClampLineNum = 3;
        document.body.appendChild(el);
        await microtasksFinished();

        assertEquals('3', el.style.getPropertyValue('--clamp-line-num'));
        assertStyle(el.$.textContainer, '-webkit-line-clamp', '3');

        el.toolMode = ToolMode.kDeepSearch;
        await microtasksFinished();
        // Should still respect the override.
        assertEquals('3', el.style.getPropertyValue('--clamp-line-num'));
        assertStyle(el.$.textContainer, '-webkit-line-clamp', '3');

        // Clean up.
        el.remove();
      });
});
