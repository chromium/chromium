// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_embeddings/filter_chips.js';

import type {HistoryEmbeddingsFilterChips} from 'chrome://resources/cr_components/history_embeddings/filter_chips.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings-filter-chips', () => {
  let element: HistoryEmbeddingsFilterChips;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement('cr-history-embeddings-filter-chips');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('UpdatesByGroupChipByBinding', () => {
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('history-embeddings:by-group', element.$.byGroupChipIcon.icon);
    element.showResultsByGroup = true;
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('cr:check', element.$.byGroupChipIcon.icon);
  });

  test('UpdatesByGroupChipByClicking', async () => {
    let notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    let notifyEvent = await notifyEventPromise;
    assertTrue(element.showResultsByGroup);
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertTrue(notifyEvent.detail.value);

    notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    notifyEvent = await notifyEventPromise;
    assertFalse(element.showResultsByGroup);
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertFalse(notifyEvent.detail.value);
  });
});
