// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FirmwareUpdate, UpdatePriority} from 'chrome://accessory-update/firmware_update_types.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

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
    /** @type {!FirmwareUpdate} */
    const fakeFirmwareUpdate = {
      deviceId: '1',
      deviceName: 'Logitech keyboard',
      version: '2.1.12',
      description:
          'Update firmware for Logitech keyboard to improve performance',
      priority: UpdatePriority.kLow,
      updateModeInstructions: 'Do a backflip before updating.',
      screenshotUrl: '',
    };
    return initializeUpdateList(fakeFirmwareUpdate).then(() => {
      assertEquals(
          fakeFirmwareUpdate.deviceName, updateCardElement.$.name.innerText);
      assertEquals(
          `Version ${fakeFirmwareUpdate.version}`,
          updateCardElement.$.version.innerText);
      assertEquals(
          fakeFirmwareUpdate.description,
          updateCardElement.$.description.innerText);
      assertFalse(isVisible(getPriorityTextElement()));
    });
  });

  test('PriorityTextVisibleForCriticalUpdate', () => {
    /** @type {!FirmwareUpdate} */
    const fakeFirmwareUpdate = {
      deviceId: '1',
      deviceName: 'Logitech keyboard',
      version: '2.1.12',
      description:
          'Update firmware for Logitech keyboard to improve performance',
      priority: UpdatePriority.kCritical,
      updateModeInstructions: 'Do a backflip before updating.',
      screenshotUrl: '',
    };
    return initializeUpdateList(fakeFirmwareUpdate).then(() => {
      assertTrue(isVisible(getPriorityTextElement()));
      assertEquals(
          'Critical update', getPriorityTextElement().innerText.trim());
    });
  });
}
