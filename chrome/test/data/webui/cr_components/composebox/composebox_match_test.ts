// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_match.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxMatchElement} from 'chrome://resources/cr_components/composebox/composebox_match.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestStyle} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, installMock} from './composebox_test_utils.js';

function getTextContent(
    element: ComposeboxMatchElement, selector: string): string {
  const child = element.shadowRoot.querySelector(selector);
  assertTrue(!!child);
  return child.textContent.trim();
}

suite('ComposeboxMatch', () => {
  let matchElement: ComposeboxMatchElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock as unknown as PageHandlerRemote,
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

  test('renders the secondary text on a second row', async () => {
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      description: 'test description',
      isTwoRowSuggestion: true,
    });
    await microtasksFinished();

    assertTrue(matchElement.isTwoRowSuggestion);
    assertTrue(matchElement.hasAttribute('is-two-row-suggestion'));
    assertEquals('test contents', getTextContent(matchElement, '#contents'));
    assertEquals(
        'test description', getTextContent(matchElement, '#description'));

    // Single-row matches keep rendering only the primary text.
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      description: 'test description',
      isTwoRowSuggestion: false,
    });
    await microtasksFinished();

    assertFalse(matchElement.isTwoRowSuggestion);
    assertFalse(matchElement.hasAttribute('is-two-row-suggestion'));
    assertEquals('test contents', getTextContent(matchElement, '#contents'));
    assertEquals(null, matchElement.shadowRoot.querySelector('#description'));
  });

  test('is not two-row without secondary text or for rich images', async () => {
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      description: '',
      isTwoRowSuggestion: true,
    });
    await microtasksFinished();

    assertFalse(matchElement.isTwoRowSuggestion);
    assertEquals(null, matchElement.shadowRoot.querySelector('#description'));

    // Rich image matches have their own layout and never render a second row.
    matchElement.richImageSuggestionsEnabled = true;
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      description: 'test description',
      imageUrl: 'https://example.com/image.png',
      isTwoRowSuggestion: true,
      suggestStyle: SuggestStyle.kRichImage,
    });
    await microtasksFinished();

    assertTrue(matchElement.isRichImage);
    assertFalse(matchElement.isTwoRowSuggestion);
    assertEquals(null, matchElement.shadowRoot.querySelector('#description'));
  });

  test('clamps the primary text of two-row matches', async () => {
    // #textContainer is a flex column for two-row matches, so a line clamp on
    // it has no effect and has to be applied to #contents instead. This has to
    // hold for both things that clamp: deep search and `overrideClampLineNum`.
    matchElement.toolMode = ToolMode.kDeepSearch;
    matchElement.match = createAutocompleteMatch({
      contents: 'Very long text '.repeat(20),
      description: 'test description',
      isTwoRowSuggestion: true,
    });
    await microtasksFinished();

    const contents = matchElement.shadowRoot.querySelector('#contents');
    const description = matchElement.shadowRoot.querySelector('#description');
    assertTrue(!!contents);
    assertTrue(!!description);
    assertStyle(contents, '-webkit-line-clamp', '2');
    // The secondary text always stays on a single line.
    assertStyle(description, '-webkit-line-clamp', 'none');

    // `overrideClampLineNum` is read in connectedCallback, so it has to be set
    // before the element is attached.
    const el: ComposeboxMatchElement =
        document.createElement('cr-composebox-match');
    el.overrideClampLineNum = 3;
    document.body.appendChild(el);
    el.match = createAutocompleteMatch({
      contents: 'Very long text '.repeat(20),
      description: 'test description',
      isTwoRowSuggestion: true,
    });
    await microtasksFinished();

    const overrideContents = el.shadowRoot.querySelector('#contents');
    assertTrue(!!overrideContents);
    assertStyle(overrideContents, '-webkit-line-clamp', '3');

    // Clean up.
    el.remove();
  });

  test(
      'renders background image when SuggestStyle.kRichImage is set',
      async () => {
        matchElement.richImageSuggestionsEnabled = true;
        matchElement.match = createAutocompleteMatch({
          imageUrl: 'https://example.com/image.png',
          suggestStyle: SuggestStyle.kRichImage,
        });
        await microtasksFinished();

        assertTrue(matchElement.isRichImage);
        assertEquals('rich-image', matchElement.getAttribute('suggest-style'));
        assertTrue(!!matchElement.$.image);
        assertTrue(
            matchElement.$.image.style.backgroundImage.includes('image.png'));
      });

  test(
      'does not render background image when suggestStyle is not kRichImage',
      async () => {
        matchElement.richImageSuggestionsEnabled = true;
        matchElement.match = createAutocompleteMatch({
          imageUrl: 'https://example.com/image.png',
          suggestStyle: SuggestStyle.kDefault,
        });
        await microtasksFinished();

        assertFalse(matchElement.isRichImage);
        assertEquals('default', matchElement.getAttribute('suggest-style'));
        assertTrue(!!matchElement.$.image);
        assertEquals('', matchElement.$.image.style.backgroundImage);
      });

  test('does not render background image when flag is disabled', async () => {
    matchElement.richImageSuggestionsEnabled = false;
    matchElement.match = createAutocompleteMatch({
      imageUrl: 'https://example.com/image.png',
      suggestStyle: SuggestStyle.kRichImage,
    });
    await microtasksFinished();

    assertFalse(matchElement.isRichImage);
    assertEquals('default', matchElement.getAttribute('suggest-style'));
    assertTrue(!!matchElement.$.image);
    assertEquals('', matchElement.$.image.style.backgroundImage);
  });

  test('click triggers openAutocompleteMatch', async () => {
    const match = createAutocompleteMatch({
      destinationUrl: 'https://google.com/',
    });
    matchElement.match = match;
    matchElement.matchIndex = 1;
    await microtasksFinished();

    matchElement.click();

    const [resultSequenceId, index, url] =
        await searchboxHandler.whenCalled('openAutocompleteMatch');
    assertEquals(0, resultSequenceId);
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
    const whenMatchFocusin = eventToPromise<CustomEvent<{index: number}>>(
        'match-focusin', matchElement);

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

  test('iconContainer does not shrink with long text', async () => {
    matchElement.match = createAutocompleteMatch({
      contents: 'Very long text '.repeat(20),
    });
    await microtasksFinished();

    assertStyle(matchElement.$.iconContainer, 'flex-shrink', '0');
  });
});
