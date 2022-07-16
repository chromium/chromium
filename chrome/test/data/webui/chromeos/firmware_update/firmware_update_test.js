// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {FirmwareUpdateAppElement} from 'chrome://accessory-update/firmware_update_app.js';
import {DialogState} from 'chrome://accessory-update/firmware_update_dialog.js';
import {UpdateProviderInterface} from 'chrome://accessory-update/firmware_update_types.js';
import {getUpdateProvider, setUpdateProviderForTesting} from 'chrome://accessory-update/mojo_interface_provider.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function firmwareUpdateAppTest() {
  /** @type {?FirmwareUpdateAppElement} */
  let page = null;

  /** @type {?FakeUpdateProvider} */
  let provider = null;

  setup(() => {
    provider = new FakeUpdateProvider();
    setUpdateProviderForTesting(provider);
    provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);
    page = /** @type {!FirmwareUpdateAppElement} */ (
        document.createElement('firmware-update-app'));
    document.body.appendChild(page);
  });

  teardown(() => {
    provider.reset();
    provider = null;
    page.remove();
    page = null;
  });

  /** @return {!CrDialogElement} */
  function getDevicePrepDialog() {
    return page.shadowRoot.querySelector('firmware-update-dialog')
        .shadowRoot.querySelector('#devicePrepDialog');
  }

  /** @return {!CrDialogElement} */
  function getUpdateDialog() {
    return page.shadowRoot.querySelector('firmware-update-dialog')
        .shadowRoot.querySelector('#updateDialog');
  }

  /** @return {!Array<!UpdateCardElement>} */
  function getUpdateCards() {
    const updateList = page.shadowRoot.querySelector('peripheral-updates-list');
    return updateList.shadowRoot.querySelectorAll('update-card');
  }

  /**
   * @param {!CrDialogElement} dialogElement
   */
  function getNextButton(dialogElement) {
    return /** @type {!CrButtonElement} */ (
        dialogElement.querySelector('#nextButton'));
  }

  /** @return {!DialogState} */
  function getDialogState() {
    return page.shadowRoot.querySelector('firmware-update-dialog').dialogState;
  }

  test('LandingPageLoaded', () => {
    // TODO(michaelcheco): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals(
        'Firmware updates',
        page.shadowRoot.querySelector('#header').textContent.trim());
  });

  test('SettingGettingTestProvider', () => {
    let fake_provider =
        /** @type {!UpdateProviderInterface} */ (new FakeUpdateProvider());
    setUpdateProviderForTesting(fake_provider);
    assertEquals(fake_provider, getUpdateProvider());
  });

  test('OpenDevicePrepDialog', async () => {
    await flushTasks();
    assertFalse(!!getDevicePrepDialog());
    // Open dialog for first firmware update card.
    getUpdateCards()[0].shadowRoot.querySelector(`#updateButton`).click();
    await flushTasks();
    assertTrue(getDevicePrepDialog().open);
  });

  test('OpenUpdateDialog', async () => {
    await flushTasks();
    // Open dialog for first firmware update card.
    getUpdateCards()[0].shadowRoot.querySelector(`#updateButton`).click();
    await flushTasks();
    getNextButton(getDevicePrepDialog()).click();
    await flushTasks();
    assertTrue(getUpdateDialog().open);
  });
}
