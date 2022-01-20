// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {FirmwareUpdateAppElement} from 'chrome://accessory-update/firmware_update_app.js';
import {FirmwareUpdate, UpdateProviderInterface, UpdateState} from 'chrome://accessory-update/firmware_update_types.js';
import {getUpdateProvider, setUpdateControllerForTesting, setUpdateProviderForTesting} from 'chrome://accessory-update/mojo_interface_provider.js';
import {mojoString16ToString} from 'chrome://accessory-update/mojo_utils.js';
import {UpdateCardElement} from 'chrome://accessory-update/update_card.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function firmwareUpdateAppTest() {
  /** @type {?FirmwareUpdateAppElement} */
  let page = null;

  /** @type {?FakeUpdateProvider} */
  let provider = null;

  /** @type {?FakeUpdateController} */
  let controller = null;

  setup(() => {
    document.body.innerHTML = '';
    controller = new FakeUpdateController();
    controller.setUpdateIntervalInMs(0);
    setUpdateControllerForTesting(controller);
    provider = new FakeUpdateProvider();
    setUpdateProviderForTesting(provider);
    provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);
  });

  teardown(() => {
    controller.reset();
    controller = null;
    provider.reset();
    provider = null;
    page.remove();
    page = null;
  });

  function initializePage() {
    page = /** @type {!FirmwareUpdateAppElement} */ (
        document.createElement('firmware-update-app'));
    document.body.appendChild(page);
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

  /** @return {!UpdateState} */
  function getUpdateState() {
    return page.shadowRoot.querySelector('firmware-update-dialog')
        .installationProgress.state;
  }

  /** @return {!FirmwareUpdate} */
  function getFirmwareUpdateFromDialog() {
    return page.shadowRoot.querySelector('firmware-update-dialog').update;
  }

  function getUpdateDialogTitle() {
    return /** @type {!HTMLDivElement} */ (
        page.shadowRoot.querySelector('firmware-update-dialog')
            .shadowRoot.querySelector('#updateDialogTitle'));
  }

  test('LandingPageLoaded', () => {
    initializePage();
    // TODO(michaelcheco): Remove this stub test once the page has more
    // capabilities to test.
    assertEquals(
        'Update peripherals',
        page.shadowRoot.querySelector('#header').textContent.trim());
  });

  test('SettingGettingTestProvider', () => {
    initializePage();
    let fake_provider =
        /** @type {!UpdateProviderInterface} */ (new FakeUpdateProvider());
    setUpdateProviderForTesting(fake_provider);
    assertEquals(fake_provider, getUpdateProvider());
  });

  test('OpenUpdateDialog', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for first firmware update card.
    getUpdateCards()[0].shadowRoot.querySelector(`#updateButton`).click();
    // Process |OnStateChanged| and |OnProgressChanged| calls.
    await flushTasks();
    await flushTasks();
    assertTrue(getUpdateDialog().open);
  });

  test('SuccessfulUpdate', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for firmware update.
    getUpdateCards()[1].shadowRoot.querySelector(`#updateButton`).click();
    // Process |OnStateChanged| and |OnProgressChanged| calls.
    await flushTasks();
    await flushTasks();
    assertEquals(UpdateState.kUpdating, getUpdateState());
    const fakeFirmwareUpdate = getFirmwareUpdateFromDialog();
    assertEquals(
        `Updating ${mojoString16ToString(fakeFirmwareUpdate.deviceName)}`,
        getUpdateDialogTitle().innerText.trim());
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    await flushTasks();
    assertEquals(UpdateState.kSuccess, getUpdateState());
    assertTrue(getUpdateDialog().open);
    assertEquals(
        `Your ${
            mojoString16ToString(fakeFirmwareUpdate.deviceName)} is up to date`,
        getUpdateDialogTitle().innerText.trim());
  });

  test('UpdateFailed', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for firmware update. The third fake update in the list
    // will fail.
    getUpdateCards()[2].shadowRoot.querySelector(`#updateButton`).click();
    // Process |OnStateChanged| and |OnProgressChanged| calls.
    await flushTasks();
    await flushTasks();
    assertEquals(UpdateState.kUpdating, getUpdateState());
    const fakeFirmwareUpdate = getFirmwareUpdateFromDialog();
    assertEquals(
        `Updating ${mojoString16ToString(fakeFirmwareUpdate.deviceName)}`,
        getUpdateDialogTitle().innerText.trim());
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    await flushTasks();
    assertEquals(UpdateState.kFailed, getUpdateState());
    assertTrue(getUpdateDialog().open);
    assertEquals(
        `Failed to update ${
            mojoString16ToString(fakeFirmwareUpdate.deviceName)}`,
        getUpdateDialogTitle().innerText.trim());
  });

  test('InflightUpdate', async () => {
    // Simulate an inflight update is already in progress.
    provider.setInflightUpdate(fakeFirmwareUpdates[0][0]);
    initializePage();
    await flushTasks();
    await flushTasks();

    // Simulate InstallProgressChangedObserver being called.
    controller.beginUpdate();
    await flushTasks();
    // Check that the update dialog is now opened with an update.
    assertEquals(UpdateState.kUpdating, getUpdateState());
    const fakeUpdate = getFirmwareUpdateFromDialog();
    assertEquals(
        `Updating ${mojoString16ToString(fakeUpdate.deviceName)}`,
        getUpdateDialogTitle().innerText.trim());
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    await flushTasks();
    assertEquals(UpdateState.kSuccess, getUpdateState());
    assertTrue(getUpdateDialog().open);
  });

  test('InflightUpdateNoProgressUpdate', async () => {
    // Simulate an inflight update is already in progress.
    provider.setInflightUpdate(fakeFirmwareUpdates[0][0]);
    initializePage();
    await flushTasks();
    await flushTasks();

    // Check that the update dialog is now opened with an update.
    assertEquals(UpdateState.kIdle, getUpdateState());
    const fakeUpdate = getFirmwareUpdateFromDialog();
    assertTrue(getUpdateDialog().open);
  });
}
