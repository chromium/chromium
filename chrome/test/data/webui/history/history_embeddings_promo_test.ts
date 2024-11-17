// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY, HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY} from 'chrome://history/history.js';
import type {HistoryEmbeddingsPromoElement} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('HistoryEmbeddingsPromoTest', function() {
  let element: HistoryEmbeddingsPromoElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.localStorage.clear();
    element = document.createElement('history-embeddings-promo');
    document.body.appendChild(element);
  });

  test('Dismisses', () => {
    assertFalse(Boolean(
        window.localStorage.getItem(HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY)));
    assertTrue(isVisible(element.$.promo));
    element.$.close.click();
    assertFalse(isVisible(element.$.promo));
    assertTrue(Boolean(
        window.localStorage.getItem(HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY)));
  });

  test('DoesNotShowIfShownAlready', () => {
    window.localStorage.setItem(HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY, 'true');
    const newPromo = document.createElement('history-embeddings-promo');
    document.body.appendChild(newPromo);
    assertFalse(isVisible(newPromo.$.promo));
  });

  test('DismissesAnswererPromo', async () => {
    loadTimeData.overrideValues({enableHistoryEmbeddingsAnswers: true});
    assertFalse(Boolean(window.localStorage.getItem(
        HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY)));
    const newPromo = document.createElement('history-embeddings-promo');
    document.body.appendChild(newPromo);
    assertTrue(isVisible(newPromo.$.promo));
    newPromo.$.close.click();
    assertFalse(isVisible(newPromo.$.promo));
    assertTrue(Boolean(window.localStorage.getItem(
        HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY)));
  });

  test('ShowsAnswererPromoEvenIfNonanwererPromoWasShown', () => {
    loadTimeData.overrideValues({enableHistoryEmbeddingsAnswers: true});
    window.localStorage.setItem(HISTORY_EMBEDDINGS_PROMO_SHOWN_KEY, 'true');
    const newPromo = document.createElement('history-embeddings-promo');
    document.body.appendChild(newPromo);
    assertTrue(isVisible(newPromo.$.promo));
  });

  test('DoesNotShowAnswererPromoIfShownAlready', () => {
    loadTimeData.overrideValues({enableHistoryEmbeddingsAnswers: true});
    window.localStorage.setItem(
        HISTORY_EMBEDDINGS_ANSWERS_PROMO_SHOWN_KEY, 'true');
    const newPromo = document.createElement('history-embeddings-promo');
    document.body.appendChild(newPromo);
    assertFalse(isVisible(newPromo.$.promo));
  });
});
