// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_embeddings/result_image.js';

import type {HistoryEmbeddingsResultImageElement} from 'chrome://resources/cr_components/history_embeddings/result_image.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('cr-history-embeddings-result-image', () => {
  let element: HistoryEmbeddingsResultImageElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-history-embeddings-result-image');
    document.body.appendChild(element);
  });

  test('ShowsSvgByDefault', () => {
    const svg = element.shadowRoot!.querySelector('svg');
    assertTrue(!!svg);
  });
});
