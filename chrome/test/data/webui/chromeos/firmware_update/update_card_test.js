// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeCriticalFirmwareUpdate, fakeFirmwareUpdate} from 'chrome://accessory-update/fake_data.js';
import {FirmwareUpdate, UpdatePriority} from 'chrome://accessory-update/firmware_update_types.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

export function updateCardTest() {
  /** @type {?UpdateCardElement} */
  let updateCardElement = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    if (updateCardElement) {
      updateCardElement.remove();
    }
    updateCardElement = null;
  });

  /**
   * @param {!FirmwareUpdate} update
   * @return {!Promise}
   */
  function initializeUpdateList(update) {
    assertFalse(!!updateCardElement);

    // Add the update card to the DOM.
    updateCardElement = /** @type {!UpdateCardElement} */ (
        document.createElement('update-card'));
    assertTrue(!!updateCardElement);
    updateCardElement.update = update;
    document.body.appendChild(updateCardElement);

    return flushTasks();
  }

  function getPriorityTextElement() {
    return /** @type {!HTMLSpanElement} */ (
        updateCardElement.shadowRoot.querySelector('#priorityText'));
  }

  test('UpdateCardPopulated', () => {
    return initializeUpdateList(fakeFirmwareUpdate).then(() => {
      assertEquals(
          mojoString16ToString(fakeFirmwareUpdate.deviceName),
          updateCardElement.$.name.innerText);
      assertEquals(
          `Version ${fakeFirmwareUpdate.deviceVersion}`,
          updateCardElement.$.version.innerText);
      assertFalse(isVisible(getPriorityTextElement()));
    });
  });

  test('PriorityTextVisibleForCriticalUpdate', () => {
    return initializeUpdateList(fakeCriticalFirmwareUpdate).then(() => {
      assertTrue(isVisible(getPriorityTextElement()));
      assertEquals(
          'Critical update', getPriorityTextElement().innerText.trim());
    });
  });
}
