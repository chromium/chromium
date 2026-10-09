// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_match.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {BaseLayout} from 'chrome://resources/cr_components/composebox/composebox_match.js';
import type {ComposeboxMatchElement} from 'chrome://resources/cr_components/composebox/composebox_match.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {createSuggestTemplateInfo} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestStyle} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {SecondaryTextPlacement} from 'chrome://resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
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
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'test contents',
        secondaryText: 'test description',
      }),
    });
    await microtasksFinished();

    const contents = matchElement.$.textContainer;
    assertEquals('test contents', contents.textContent.trim());
  });

  test('renders the secondary text below the primary text', async () => {
    matchElement.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'test contents',
        secondaryText: 'test description',
        secondaryTextPlacement: SecondaryTextPlacement.kBelowPrimaryText,
      }),
    });
    await microtasksFinished();

    assertEquals(
        'below-primary-text',
        matchElement.getAttribute('secondary-text-placement'));
    assertEquals('test contents', getTextContent(matchElement, '#primaryText'));
    assertEquals(
        'test description', getTextContent(matchElement, '#secondaryText'));

    // The two-row height is not fixed; it falls out of the 24px primary line
    // and 20px secondary line inside the 10px of block padding.
    const container =
        matchElement.shadowRoot.querySelector<HTMLElement>('.container');
    assertTrue(!!container);
    assertEquals(64, container.offsetHeight);
  });

  test('does not apply two-row styling to rich image matches', async () => {
    // SearchboxHandler places the secondary text below the primary text for
    // every match that has an image, which is also what makes a match
    // eligible for the rich image style, so rich image matches carry the
    // attribute too. The two-row rules are qualified with the suggest style so
    // that they do not pick it up.
    matchElement.richImageSuggestionsEnabled = true;
    matchElement.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'test contents',
        secondaryTextPlacement: SecondaryTextPlacement.kBelowPrimaryText,
        image: {url: 'https://example.com/image.png', dominantColor: ''},
      }),
      suggestStyle: SuggestStyle.kRichImage,
    });
    await microtasksFinished();

    assertTrue(matchElement.isRichImage);
    assertEquals(
        'below-primary-text',
        matchElement.getAttribute('secondary-text-placement'));
    const container = matchElement.shadowRoot.querySelector('.container');
    assertTrue(!!container);
    assertStyle(container, 'padding-block-start', '0px');
    assertStyle(container, 'padding-block-end', '0px');
    const secondaryText =
        matchElement.shadowRoot.querySelector('#secondaryText');
    assertTrue(!!secondaryText);
    assertStyle(secondaryText, 'display', 'none');
  });

  test('clamps the primary text of two-row matches', async () => {
    // #textContainer holds both rows for two-row matches, so a line clamp on it
    // would cut the secondary text and has to be applied to #primaryText
    // instead. `overrideClampLineNum` is read in connectedCallback, so it has
    // to be set before the element is attached.
    const el: ComposeboxMatchElement =
        document.createElement('cr-composebox-match');
    el.overrideClampLineNum = 3;
    document.body.appendChild(el);
    el.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'Very long text '.repeat(20),
        secondaryText: 'test description',
        secondaryTextPlacement: SecondaryTextPlacement.kBelowPrimaryText,
      }),
    });
    await microtasksFinished();

    const primaryText = el.shadowRoot.querySelector('#primaryText');
    const secondaryText = el.shadowRoot.querySelector('#secondaryText');
    assertTrue(!!primaryText);
    assertTrue(!!secondaryText);
    assertStyle(el.$.textContainer, '-webkit-line-clamp', 'none');
    assertStyle(primaryText, '-webkit-line-clamp', '3');
    // The secondary text always stays on a single line.
    assertStyle(secondaryText, '-webkit-line-clamp', 'none');
    assertStyle(secondaryText, 'white-space', 'nowrap');

    // Clean up.
    el.remove();
  });

  test(
      'renders background image when SuggestStyle.kRichImage is set',
      async () => {
        matchElement.richImageSuggestionsEnabled = true;
        matchElement.match = createAutocompleteMatch({
          suggestTemplate: createSuggestTemplateInfo({
            image: {url: 'https://example.com/image.png', dominantColor: ''},
          }),
          suggestStyle: SuggestStyle.kRichImage,
        });
        await microtasksFinished();

        assertTrue(matchElement.isRichImage);
        assertEquals('rich-image', matchElement.getAttribute('suggest-style'));
        assertEquals('image', matchElement.getAttribute('base-layout'));
        assertEquals(BaseLayout.IMAGE, matchElement.baseLayout);
        assertTrue(!!matchElement.$.image);
        assertTrue(
            matchElement.$.image.style.backgroundImage.includes('image.png'));

        // Updating to a kRichImage match without an imageUrl transitions back
        // to the default text base-layout.
        matchElement.match = createAutocompleteMatch({
          suggestStyle: SuggestStyle.kRichImage,
        });
        await microtasksFinished();

        assertFalse(matchElement.isRichImage);
        assertEquals('default', matchElement.getAttribute('suggest-style'));
        assertEquals('text', matchElement.getAttribute('base-layout'));
        assertEquals(BaseLayout.TEXT, matchElement.baseLayout);
        assertEquals('', matchElement.$.image.style.backgroundImage);
      });

  test(
      'does not render background image when suggestStyle is not kRichImage',
      async () => {
        matchElement.richImageSuggestionsEnabled = true;
        matchElement.match = createAutocompleteMatch({
          suggestTemplate: createSuggestTemplateInfo({
            image: {url: 'https://example.com/image.png', dominantColor: ''},
          }),
          suggestStyle: SuggestStyle.kDefault,
        });
        await microtasksFinished();

        assertFalse(matchElement.isRichImage);
        assertEquals('default', matchElement.getAttribute('suggest-style'));
        assertEquals('text', matchElement.getAttribute('base-layout'));
        assertEquals(BaseLayout.TEXT, matchElement.baseLayout);
        assertTrue(!!matchElement.$.image);
        assertEquals('', matchElement.$.image.style.backgroundImage);
      });

  test('does not render background image when flag is disabled', async () => {
    matchElement.richImageSuggestionsEnabled = false;
    matchElement.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        image: {url: 'https://example.com/image.png', dominantColor: ''},
      }),
      suggestStyle: SuggestStyle.kRichImage,
    });
    await microtasksFinished();

    assertFalse(matchElement.isRichImage);
    assertEquals('default', matchElement.getAttribute('suggest-style'));
    assertEquals('text', matchElement.getAttribute('base-layout'));
    assertEquals(BaseLayout.TEXT, matchElement.baseLayout);
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
        const primaryText = el.shadowRoot.querySelector('#primaryText');
        assertTrue(!!primaryText);
        assertStyle(primaryText, '-webkit-line-clamp', '3');

        // Clean up.
        el.remove();
      });

  test('iconContainer does not shrink with long text', async () => {
    matchElement.match = createAutocompleteMatch({
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'Very long text '.repeat(20),
      }),
    });
    await microtasksFinished();

    assertStyle(matchElement.$.iconContainer, 'flex-shrink', '0');
  });

  test(
      'willTabExitMatch accounts for supportsDeletion and remove button focus',
      async () => {
        // When supportsDeletion is false, both Tab and Shift-Tab exit the
        // match.
        matchElement.match = createAutocompleteMatch({
          supportsDeletion: false,
        });
        await microtasksFinished();
        assertTrue(matchElement.willTabExitMatch(/*shiftKey=*/ false));
        assertTrue(matchElement.willTabExitMatch(/*shiftKey=*/ true));

        // When supportsDeletion is true and focus is on the match row (not the
        // remove button), Shift-Tab exits the match while forward Tab moves to
        // the remove button.
        matchElement.match = createAutocompleteMatch({
          supportsDeletion: true,
        });
        matchElement.toggleAttribute('selected', true);
        await microtasksFinished();

        assertFalse(matchElement.isRemoveButtonFocused());
        assertFalse(matchElement.willTabExitMatch(/*shiftKey=*/ false));
        assertTrue(matchElement.willTabExitMatch(/*shiftKey=*/ true));

        // When focus is on the remove button, forward Tab exits the match
        // while Shift-Tab moves back to the match row.
        matchElement.$.remove.style.display = 'inline-flex';
        matchElement.$.remove.focus();
        await microtasksFinished();

        assertTrue(matchElement.isRemoveButtonFocused());
        assertTrue(matchElement.willTabExitMatch(/*shiftKey=*/ false));
        assertFalse(matchElement.willTabExitMatch(/*shiftKey=*/ true));
      });

  test('loading state suppresses image/text and ignores clicks', async () => {
    matchElement.richImageSuggestionsEnabled = true;
    matchElement.matchIndex = 1;
    matchElement.match = createAutocompleteMatch({
      suggestStyle: SuggestStyle.kRichImage,
      suggestTemplate: createSuggestTemplateInfo({
        primaryText: 'test contents',
        image: {url: 'https://example.com/image.png', dominantColor: ''},
      }),
    });
    matchElement.loading = true;
    await microtasksFinished();

    assertTrue(matchElement.hasAttribute('loading'));
    const imageEl =
        matchElement.shadowRoot.querySelector<HTMLElement>('#image');
    assertTrue(!!imageEl);
    assertEquals('', imageEl.style.backgroundImage);

    // Clicks and focusin while loading should be ignored.
    let focusinFired = false;
    matchElement.addEventListener('match-focusin', () => {
      focusinFired = true;
    });
    matchElement.dispatchEvent(new FocusEvent('focusin'));
    matchElement.click();
    await microtasksFinished();
    assertFalse(focusinFired);
    assertEquals(0, searchboxHandler.getCallCount('openAutocompleteMatch'));

    // Once loading finishes, background image is applied and events work.
    matchElement.loading = false;
    await microtasksFinished();
    assertFalse(matchElement.hasAttribute('loading'));
    assertTrue(imageEl.style.backgroundImage.includes('example.com'));

    matchElement.dispatchEvent(new FocusEvent('focusin'));
    matchElement.click();
    await microtasksFinished();
    assertTrue(focusinFired);
    assertEquals(1, searchboxHandler.getCallCount('openAutocompleteMatch'));
  });
});
