// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/update_card.js';

import {fakeCriticalFirmwareUpdate, fakeFirmwareUpdate} from 'chrome://accessory-update/fake_data.js';
import {FirmwareUpdate} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('UpdateCardTest', () => {
  let updateCardElement: UpdateCardElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    updateCardElement?.remove();
    updateCardElement = null;
  });

  function initializeUpdateList(update: FirmwareUpdate): Promise<void> {
    assertFalse(!!updateCardElement);

    // Add the update card to the DOM.
    updateCardElement =
        document.createElement('update-card') as UpdateCardElement;
    assertTrue(!!updateCardElement);
    updateCardElement.update = update;
    document.body.appendChild(updateCardElement);

    return flushTasks();
  }

  function getNameText(): string {
    assert(updateCardElement);
    const name =
        strictQuery('#name', updateCardElement.shadowRoot, HTMLElement);
    assertTrue(!!name);
    return name!.innerText;
  }

  function getVersionText(): string {
    assert(updateCardElement);
    const version =
        strictQuery('#version', updateCardElement.shadowRoot, HTMLElement);
    assertTrue(!!version);
    return version!.innerText;
  }

  function getPriorityTextElement(): HTMLSpanElement {
    assert(updateCardElement);
    return strictQuery(
        '#priorityText', updateCardElement.shadowRoot, HTMLSpanElement)!;
  }

  test('UpdateCardPopulated', () => {
    return initializeUpdateList(fakeFirmwareUpdate).then(() => {
      assertEquals(
          mojoString16ToString(fakeFirmwareUpdate.deviceName), getNameText());
      assertEquals(
          `Version ${fakeFirmwareUpdate.deviceVersion}`, getVersionText());
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
});
