// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {create} from 'chrome-untrusted://glic/explain_fulfillment.js';
import type {ExplainFulfillmentElement} from 'chrome-untrusted://glic/explain_fulfillment.js';
import {ExplainFulfillment} from 'chrome-untrusted://glic/explain_fulfillment.mojom-webui.js';
import type {ExplainFulfillmentInterface} from 'chrome-untrusted://glic/explain_fulfillment.mojom-webui.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeExplainFulfillment implements ExplainFulfillmentInterface {
  askGeminiCount = 0;

  getExplanation() {
    return Promise.resolve({explanation: 'An explanation.'});
  }

  openTabForSearch(_query: string) {}

  askGemini() {
    this.askGeminiCount++;
  }
}

suite('ExplainFulfillment', () => {
  let card: ExplainFulfillmentElement;
  let fulfillment: FakeExplainFulfillment;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({askGemini: 'Ask Gemini'});
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
        'An explanation.', card.querySelector('.explanation')!.textContent);
  });

  test('AskGeminiButtonAsksGemini', () => {
    card.querySelector<HTMLElement>('.ask-gemini')!.click();
    assertEquals(1, fulfillment.askGeminiCount);
  });
});
