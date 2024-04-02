// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryEmbeddingsPromoElement} from 'chrome://history/history.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('HistoryEmbeddingsPromoTest', function() {
  let element: HistoryEmbeddingsPromoElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('history-embeddings-promo');
    document.body.appendChild(element);
  });

  test('Dismisses', () => {
    assertTrue(isVisible(element.$.promo));
    element.$.close.click();
    assertFalse(isVisible(element.$.promo));
  });
});
