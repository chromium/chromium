// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_match.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxMatchElement} from 'chrome://resources/cr_components/composebox/composebox_match.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestStyle} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
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
    // Single-row matches don't show their description.
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      description: 'test description',
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

    // The two-row height is not fixed; it falls out of the 24px primary line
    // and 20px secondary line inside the 10px of block padding.
    const container =
        matchElement.shadowRoot.querySelector<HTMLElement>('.container');
    assertTrue(!!container);
    assertEquals(64, container.offsetHeight);
  });

  test('does not apply two-row styling to rich image matches', async () => {
    // SearchboxHandler sets is_two_row_suggestion for every match that has an
    // image_url, which is also what makes a match eligible for the rich image
    // style, so rich image matches carry the attribute too. The two-row rules
    // are qualified with the suggest style so that they do not pick it up.
    matchElement.richImageSuggestionsEnabled = true;
    matchElement.match = createAutocompleteMatch({
      contents: 'test contents',
      imageUrl: 'https://example.com/image.png',
      isTwoRowSuggestion: true,
      suggestStyle: SuggestStyle.kRichImage,
    });
    await microtasksFinished();

    assertTrue(matchElement.isRichImage);
    assertTrue(matchElement.hasAttribute('is-two-row-suggestion'));
    const container = matchElement.shadowRoot.querySelector('.container');
    assertTrue(!!container);
    assertStyle(container, 'padding-block-start', '0px');
    assertStyle(container, 'padding-block-end', '0px');
  });

  test('clamps the primary text of two-row matches', async () => {
    // #textContainer holds both rows for two-row matches, so a line clamp on it
    // would cut the secondary text and has to be applied to #contents instead.
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

    const contents = el.shadowRoot.querySelector('#contents');
    const description = el.shadowRoot.querySelector('#description');
    assertTrue(!!contents);
    assertTrue(!!description);
    assertStyle(el.$.textContainer, '-webkit-line-clamp', 'none');
    assertStyle(contents, '-webkit-line-clamp', '3');
    // The secondary text always stays on a single line.
    assertStyle(description, '-webkit-line-clamp', 'none');
    assertStyle(description, 'white-space', 'nowrap');

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

  test(
      'clamps lines according to `overrideClampLineNum` property', async () => {
        const el: ComposeboxMatchElement =
            document.createElement('cr-composebox-match');
        el.overrideClampLineNum = 3;
        document.body.appendChild(el);
        await microtasksFinished();

        assertEquals('3', el.style.getPropertyValue('--clamp-line-num'));
        const contents = el.shadowRoot.querySelector('#contents');
        assertTrue(!!contents);
        assertStyle(contents, '-webkit-line-clamp', '3');

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
