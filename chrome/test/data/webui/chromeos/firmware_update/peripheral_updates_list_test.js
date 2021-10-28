// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PeripheralUpdateListElement} from 'chrome://accessory-update/peripheral_updates_list.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function peripheralUpdatesListTest() {
  /** @type {?PeripheralUpdateListElement} */
  let peripheralUpdateListElement = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (peripheralUpdateListElement) {
      peripheralUpdateListElement.remove();
    }
    peripheralUpdateListElement = null;
  });

  function initializeUpdateList() {
    assertFalse(!!peripheralUpdateListElement);

    // Add the update list to the DOM.
    peripheralUpdateListElement = /** @type {!PeripheralUpdateListElement} */ (
        document.createElement('peripheral-updates-list'));
    assertTrue(!!peripheralUpdateListElement);
    document.body.appendChild(peripheralUpdateListElement);

    return flushTasks();
  }

  test('UpdateListInitialized', () => {
    // TODO(michaelcheco): Remove this stub test once the update list element
    // has more capabilities to test.
    return initializeUpdateList().then(() => {
      assertTrue(
          !!peripheralUpdateListElement.shadowRoot.querySelector('#container'));
    });
  });
}
