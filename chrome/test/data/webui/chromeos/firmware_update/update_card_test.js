// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function updateCardTest() {
  /** @type {?UpdateCardElement} */
  let updateCardElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (updateCardElement) {
      updateCardElement.remove();
    }
    updateCardElement = null;
  });

  function initializeUpdateList() {
    assertFalse(!!updateCardElement);

    // Add the update card to the DOM.
    updateCardElement = /** @type {!UpdateCardElement} */ (
        document.createElement('update-card'));
    assertTrue(!!updateCardElement);
    document.body.appendChild(updateCardElement);

    return flushTasks();
  }

  test('UpdateCardInitialized', () => {
    // TODO(michaelcheco): Remove this stub test once the update card element
    // has more capabilities to test.
    return initializeUpdateList().then(() => {
      assertTrue(!!updateCardElement.shadowRoot.querySelector('#container'));
    });
  });
}
