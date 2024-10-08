// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/firmware_update_app.js';

import {fakeFirmwareUpdates} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {FakeUpdateProvider} from 'chrome://accessory-update/fake_update_provider.js';
import {UpdateState} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import type {FirmwareUpdate} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import type {FirmwareUpdateAppElement} from 'chrome://accessory-update/firmware_update_app.js';
import {FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';
import {getUpdateProvider, setUpdateControllerForTesting, setUpdateProviderForTesting} from 'chrome://accessory-update/mojo_interface_provider.js';
import type {UpdateCardElement} from 'chrome://accessory-update/update_card.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('FirmwareUpdateAppTest', () => {
  let page: FirmwareUpdateAppElement|null = null;

  let provider: FakeUpdateProvider|null = null;

  let controller: FakeUpdateController|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    controller = new FakeUpdateController();
    controller.setUpdateIntervalInMs(0);
    setUpdateControllerForTesting(controller);
    provider = new FakeUpdateProvider();
    setUpdateProviderForTesting(provider);
    provider?.setFakeFirmwareUpdates(fakeFirmwareUpdates);
  });

  teardown(() => {
    controller?.reset();
    controller = null;
    provider?.reset();
    provider = null;
    page?.remove();
    page = null;
  });

  function initializePage(): void {
    page = document.createElement('firmware-update-app') as
        FirmwareUpdateAppElement;
    assert(!!page);
    document.body.appendChild(page);
  }

  function getConfirmationDialog(): CrDialogElement {
    assert(page);
    const fwConfirmDialog = strictQuery(
        'firmware-confirmation-dialog', page.shadowRoot, HTMLElement)!;
    assert(fwConfirmDialog);
    const confirmDialog = strictQuery(
        '#confirmationDialog', fwConfirmDialog.shadowRoot, CrDialogElement)!;
    assert(confirmDialog);
    return confirmDialog;
  }

  function confirmUpdate(): Promise<void> {
    const confirmationDialog = getConfirmationDialog();
    assertTrue(confirmationDialog.open);
    const nextButton =
        strictQuery('#nextButton', confirmationDialog, CrButtonElement);
    assert(nextButton);
    nextButton.click();
    return flushTasks();
  }

  function cancelUpdate(): Promise<void> {
    const confirmationDialog = getConfirmationDialog();
    assertTrue(confirmationDialog.open);
    const cancelButton =
        strictQuery('#cancelButton', confirmationDialog, CrButtonElement);
    assert(cancelButton);
    cancelButton.click();
    return flushTasks();
  }

  function getFirmwareUpdateDialog(): FirmwareUpdateDialogElement {
    assert(page);
    return strictQuery(
        'firmware-update-dialog', page.shadowRoot, FirmwareUpdateDialogElement)!
        ;
  }

  function getUpdateDialog(): CrDialogElement {
    return strictQuery(
        '#updateDialog', getFirmwareUpdateDialog().shadowRoot, CrDialogElement)!
        ;
  }

  function getUpdateCards(): UpdateCardElement[] {
    assert(page);
    const updateList =
        page.shadowRoot!.querySelector('peripheral-updates-list')!;
    const updateCards = updateList.shadowRoot!.querySelectorAll('update-card');
    return Array.from(updateCards);
  }

  function getUpdateState(): UpdateState {
    return getFirmwareUpdateDialog()!.installationProgress!.state;
  }

  function getFirmwareUpdateFromDialog(): FirmwareUpdate|null {
    return getFirmwareUpdateDialog().update;
  }

  function getUpdateDialogTitle(): HTMLDivElement {
    return strictQuery(
        '#updateDialogTitle', getFirmwareUpdateDialog().shadowRoot,
        HTMLDivElement);
  }

  function getFakeFirmwareUpdate(
      arrayIndex: number, updateIndex: number): FirmwareUpdate {
    const arr = fakeFirmwareUpdates[arrayIndex];
    assert(arr);
    const update = arr[updateIndex];
    assert(update);
    return update;
  }

  test('SettingGettingTestProvider', () => {
    initializePage();
    const fake_provider = new FakeUpdateProvider();
    setUpdateProviderForTesting(fake_provider);
    assertEquals(fake_provider, getUpdateProvider());
  });

  test('OpenConfirmationDialog', async () => {
    initializePage();
    await flushTasks();
    assert(page);
    // Open dialog for first firmware update card.
    const whenFired = eventToPromise('cr-dialog-open', page);
    const button = strictQuery(
        `#updateButton`, getUpdateCards()[0]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    await whenFired;
    assertTrue(!!getConfirmationDialog(), 'Confirmation dialog exists');
    assertTrue(getConfirmationDialog().open, 'Confirmation dialog is open');
    await cancelUpdate();
    const fwConfirmDialog = strictQuery(
        'firmware-confirmation-dialog', page.shadowRoot, HTMLElement);
    assertFalse(isVisible(
        fwConfirmDialog.shadowRoot!.querySelector('#confirmationDialog')));
  });

  test('ConfirmationDialogShowsDisclaimerWhenFlagEnabled', async () => {
    // Enable the upstream trusted reports flag
    loadTimeData.overrideValues({
      isUpstreamTrustedReportsFirmwareEnabled: true,
    });

    // Setup the app.
    initializePage();
    await flushTasks();
    assert(page);

    // Open dialog for first firmware update card.
    let whenFired = eventToPromise('cr-dialog-open', page);
    let button = strictQuery(
        `#updateButton`, getUpdateCards()[0]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    await whenFired;
    let fwConfirmDialog = strictQuery(
        'firmware-confirmation-dialog', page.shadowRoot, HTMLElement);

    // The disclaimer should be displayed.
    assertTrue(!!fwConfirmDialog.shadowRoot!.querySelector('#disclaimer'));

    // Clear app element to reset the test.
    page?.remove();
    await flushTasks();

    // Disable the upstream trusted reports flag
    loadTimeData.overrideValues({
      isUpstreamTrustedReportsFirmwareEnabled: false,
    });

    // Setup the app.
    initializePage();
    await flushTasks();
    assert(page);

    // Open dialog for first firmware update card.
    whenFired = eventToPromise('cr-dialog-open', page);
    button = strictQuery(
        `#updateButton`, getUpdateCards()[0]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    await whenFired;
    fwConfirmDialog = strictQuery(
        'firmware-confirmation-dialog', page.shadowRoot, HTMLElement);

    // The disclaimer should not be displayed.
    assertFalse(!!fwConfirmDialog.shadowRoot!.querySelector('#disclaimer'));
  });

  test('OpenUpdateDialog', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for first firmware update card.
    const button = strictQuery(
        `#updateButton`, getUpdateCards()[0]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    const whenFired = eventToPromise('cr-dialog-open', page!);
    await confirmUpdate();
    // Process |OnProgressChanged| call.
    await flushTasks();
    return whenFired.then(() => assertTrue(getUpdateDialog().open));
  });

  test('SuccessfulUpdate', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for firmware update.
    const button = strictQuery(
        `#updateButton`, getUpdateCards()[1]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    const whenFired = eventToPromise('cr-dialog-open', page!);
    await confirmUpdate();
    // Process |OnProgressChanged| call.
    await flushTasks();
    return whenFired
        .then(() => {
          assertEquals(UpdateState.kUpdating, getUpdateState());
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(
              loadTimeData.getStringF(
                  'updating',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
          // Allow firmware update to complete.
          return controller?.getUpdateCompletedPromiseForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(UpdateState.kSuccess, getUpdateState());
          assertTrue(getUpdateDialog().open);
          assertEquals(
              loadTimeData.getStringF(
                  'deviceUpToDate',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
        });
  });

  test('SuccessfulUpdateButNeedsReboot', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for firmware update.
    const button = strictQuery(
        `#updateButton`, getUpdateCards()[5]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    const whenFired = eventToPromise('cr-dialog-open', page!);
    await confirmUpdate();
    // Process |OnProgressChanged| call.
    await flushTasks();
    return whenFired
        .then(() => {
          assertEquals(UpdateState.kUpdating, getUpdateState());
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(
              loadTimeData.getStringF(
                  'updating',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
          // Allow firmware update to complete.
          return controller?.getUpdateCompletedPromiseForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(UpdateState.kSuccess, getUpdateState());
          assertTrue(getUpdateDialog().open);
          assertEquals(
              loadTimeData.getStringF(
                  'deviceReadyToInstallUpdate',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
        });
  });

  test('UpdateFailed', async () => {
    initializePage();
    await flushTasks();
    // Open dialog for firmware update. The third fake update in the list
    // will fail.
    const button = strictQuery(
        `#updateButton`, getUpdateCards()[2]!.shadowRoot, CrButtonElement);
    button.click();
    await flushTasks();
    const whenFired = eventToPromise('cr-dialog-open', page!);
    await confirmUpdate();
    // Process |OnProgressChanged| call.
    await flushTasks();
    return whenFired
        .then(() => {
          assertEquals(UpdateState.kUpdating, getUpdateState());
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(
              loadTimeData.getStringF(
                  'updating',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
          return controller?.getUpdateCompletedPromiseForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const fakeFirmwareUpdate = getFirmwareUpdateFromDialog()!;
          assertEquals(UpdateState.kFailed, getUpdateState());
          assertTrue(getUpdateDialog().open);
          assertEquals(
              loadTimeData.getStringF(
                  'updateFailedTitleText',
                  mojoString16ToString(fakeFirmwareUpdate.deviceName)),
              getUpdateDialogTitle().innerText.trim());
        });
  });

  test('InflightUpdate', async () => {
    // Simulate an inflight update is already in progress.
    provider?.setInflightUpdate(getFakeFirmwareUpdate(0, 0));
    initializePage();
    await flushTasks();
    await flushTasks();

    // Simulate InstallProgressChangedObserver being called.
    controller?.beginUpdate('fakeDeviceId', {path: 'fake.cab'});
    await flushTasks();
    await flushTasks();
    // Check that the update dialog is now opened with an update.
    assertEquals(UpdateState.kUpdating, getUpdateState());
    const fakeUpdate = getFirmwareUpdateFromDialog()!;
    assertEquals(
        loadTimeData.getStringF(
            'updating', mojoString16ToString(fakeUpdate.deviceName)),
        getUpdateDialogTitle().innerText.trim());
    // Allow firmware update to complete.
    await controller?.getUpdateCompletedPromiseForTesting();
    await flushTasks();
    assertEquals(UpdateState.kSuccess, getUpdateState());
    assertTrue(getUpdateDialog().open);
  });

  test('InflightUpdateNoProgressUpdate', async () => {
    // Simulate an inflight update is already in progress.
    provider?.setInflightUpdate(getFakeFirmwareUpdate(0, 0));
    initializePage();
    await flushTasks();
    await flushTasks();

    // Check that the update dialog is now opened with an update.
    assertEquals(UpdateState.kIdle, getUpdateState());
    const fakeUpdate = getFirmwareUpdateFromDialog()!;
    assertTrue(!!fakeUpdate);
    assertTrue(getUpdateDialog().open);
  });
});
