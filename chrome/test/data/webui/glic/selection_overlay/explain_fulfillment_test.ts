// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {create} from 'chrome-untrusted://glic/explain_fulfillment.js';
import type {ExplainFulfillmentElement} from 'chrome-untrusted://glic/explain_fulfillment.js';
import {ExplainFulfillment} from 'chrome-untrusted://glic/explain_fulfillment.mojom-webui.js';
import type {ExplainFulfillmentInterface} from 'chrome-untrusted://glic/explain_fulfillment.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeExplainFulfillment implements ExplainFulfillmentInterface {
  askGeminiCount = 0;
  searchQueries: string[] = [];

  getExplanation() {
    return Promise.resolve(
        {explanation: 'An explanation of [Paris](https://a.test).'});
  }

  openTabForSearch(query: string) {
    this.searchQueries.push(query);
  }

  askGemini() {
    this.askGeminiCount++;
  }
}

suite('ExplainFulfillment', () => {
  let card: ExplainFulfillmentElement;
  let fulfillment: FakeExplainFulfillment;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      askGeminiMore: 'Ask Gemini more about this',
      close: 'Close',
      sharingDisclaimer:
          'This page and highlighted text are shared with Google',
    });
    fulfillment = new FakeExplainFulfillment();
    card = document.createElement('glic-explain-fulfillment');
    card.fulfillment = fulfillment;
    document.body.appendChild(card);
    // Let the explanation arrive.
    await new Promise(resolve => setTimeout(resolve));
  });

  test('CreateReturnsCardAndRemote', () => {
    const {element, interfaceName, remote} = create();
    assertEquals(ExplainFulfillment.$interfaceName, interfaceName);
    assertEquals('glic-explain-fulfillment', element.localName);
    assertEquals(remote, element.fulfillment);
  });

  test('ShowsExplanationWhenAdded', () => {
    assertEquals(
        'An explanation of Paris.',
        card.querySelector('.explanation')!.textContent);
  });

  test('LinkSearchesForItsText', () => {
    card.querySelector<HTMLElement>('.explanation a')!.click();
    assertDeepEquals(['Paris'], fulfillment.searchQueries);
  });

  test('AskGeminiButtonAsksGemini', () => {
    const askGemini = card.querySelector<HTMLElement>('.ask-gemini')!;
    assertEquals('Ask Gemini more about this', askGemini.textContent);
    assertTrue(!!askGemini.querySelector('.ask-gemini-icon'));
    askGemini.click();
    assertEquals(1, fulfillment.askGeminiCount);
  });

  test('CloseButtonFiresCloseClick', () => {
    const closeButton = card.querySelector<HTMLElement>('.close-button')!;
    assertEquals('Close', closeButton.ariaLabel);
    assertEquals('Close', closeButton.title);

    let closeClicks = 0;
    // The event bubbles, so the overlay can listen on the card's host.
    document.body.addEventListener(
        'close-click', () => closeClicks++, {once: true});
    closeButton.click();
    assertEquals(1, closeClicks);
  });

  test('ShowsSharingDisclaimer', () => {
    const disclaimer = card.querySelector<HTMLElement>('.disclaimer')!;
    assertEquals(
        'This page and highlighted text are shared with Google',
        disclaimer.textContent);
    // It's plain text, so it's neither focusable nor clickable.
    assertEquals(-1, disclaimer.tabIndex);
    assertEquals(null, disclaimer.querySelector('a, button'));
  });
});
