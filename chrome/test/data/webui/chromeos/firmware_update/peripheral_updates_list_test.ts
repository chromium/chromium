// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/peripheral_updates_list.js';

import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {FirmwareUpdate} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {setUpdateProviderForTesting} from 'chrome://accessory-update/mojo_interface_provider.js';
import {PeripheralUpdateListElement} from 'chrome://accessory-update/peripheral_updates_list.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PeripheralUpdatesListTest', () => {
  let peripheralUpdateListElement: PeripheralUpdateListElement|null = null;

  let provider: FakeUpdateProvider|null = null;

  setup(() => {
    provider = new FakeUpdateProvider();
    setUpdateProviderForTesting(provider);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    peripheralUpdateListElement?.remove();
    peripheralUpdateListElement = null;
    provider?.reset();
    provider = null;
  });

  function initializeUpdateList(): Promise<void> {
    assertFalse(!!peripheralUpdateListElement);
    provider?.setFakeFirmwareUpdates(fakeFirmwareUpdates);

    // Add the update list to the DOM.
    peripheralUpdateListElement =
        document.createElement('peripheral-updates-list') as
        PeripheralUpdateListElement;
    assertTrue(!!peripheralUpdateListElement);
    document.body.appendChild(peripheralUpdateListElement);

    return flushTasks();
  }

  function clearFirmwareUpdates(): Promise<void> {
    peripheralUpdateListElement?.setFirmwareUpdatesForTesting([]);
    return flushTasks();
  }

  function getFirmwareUpdates(): FirmwareUpdate[] {
    assert(peripheralUpdateListElement);
    return peripheralUpdateListElement?.getFirmwareUpdatesForTesting();
  }

  function getUpdateCards(): UpdateCardElement[] {
    assert(peripheralUpdateListElement?.shadowRoot);
    return Array.from(peripheralUpdateListElement?.shadowRoot.querySelectorAll(
        'update-card'));
  }

  test('UpdateCardsPopulated', () => {
    return initializeUpdateList().then(() => {
      const updateCards = getUpdateCards();
      getFirmwareUpdates().forEach((u: FirmwareUpdate, i: number): void => {
        const card = updateCards[i];
        assertTrue(!!card);
        const updateCardName =
            card!.shadowRoot!.querySelector('#name') as HTMLElement;
        assertTrue(!!updateCardName);
        assertEquals(
            mojoString16ToString(u.deviceName), updateCardName.innerText);
      });
    });
  });

  test('EmptyState', () => {
    return initializeUpdateList()
        .then(() => clearFirmwareUpdates())
        .then(() => {
          assert(peripheralUpdateListElement?.shadowRoot);
          const upToDateText =
              peripheralUpdateListElement?.shadowRoot.querySelector(
                  '#upToDateText')!;
          assertTrue(isVisible(upToDateText));
        });
  });
});
