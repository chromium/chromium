// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/firmware_update_dialog.js';

import {fakeFirmwareUpdate} from 'chrome://accessory-update/fake_data.js';
import {UpdateState} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PaperProgressElement} from 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('FirmwareUpdateDialogTest', () => {
  let updateDialogElement: FirmwareUpdateDialogElement|null = null;

  setup(() => {
    updateDialogElement = document.createElement('firmware-update-dialog');
    assert(updateDialogElement);
    updateDialogElement.update = fakeFirmwareUpdate;
    updateDialogElement.installationProgress = {
      percentage: 0,
      state: UpdateState.kIdle,
    };
    document.body.appendChild(updateDialogElement);
  });

  teardown(() => {
    updateDialogElement?.remove();
    updateDialogElement = null;
  });

  function setInstallationProgress(
      percentage: number, state: UpdateState): Promise<void> {
    assert(updateDialogElement);
    updateDialogElement.installationProgress = {percentage, state};
    return flushTasks();
  }

  function clickDoneButton(): Promise<void> {
    assert(updateDialogElement?.shadowRoot);
    const button = strictQuery(
        '#updateDoneButton', updateDialogElement?.shadowRoot, CrButtonElement)!;
    button.click();
    return flushTasks();
  }

  function setIsInitiallyInflight(inflight: boolean) {
    updateDialogElement?.setIsInitiallyInflightForTesting(inflight);
  }

  function getUpdateDialog(): CrDialogElement {
    assert(updateDialogElement?.shadowRoot);
    return strictQuery(
        '#updateDialog', updateDialogElement?.shadowRoot, CrDialogElement);
  }

  function getTextContent(selector: string): string {
    assert(updateDialogElement?.shadowRoot);
    const element =
        strictQuery(selector, updateDialogElement?.shadowRoot, HTMLElement);
    assert(element);
    return element.textContent?.trim() ?? '';
  }

  test('DialogStateUpdatesCorrectly', async () => {
    assert(updateDialogElement?.shadowRoot);
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // |UpdateState.KIdle| handled correctly while an update is still
    // in-progress.
    await setInstallationProgress(20, UpdateState.kIdle);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open when the update is completed.
    await setInstallationProgress(100, UpdateState.kSuccess);
    assertTrue(getUpdateDialog().open);

    // Dialog closes when the "Done" button is clicked.
    await clickDoneButton();
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateDialog'));
  });

  test('DeviceRestarting', async () => {
    assert(updateDialogElement?.shadowRoot);
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(getUpdateDialog().open);

    // Correct text is shown.
    assertEquals(
        getTextContent('#updateDialogTitle'),
        loadTimeData.getStringF(
            'restartingTitleText',
            mojoString16ToString(updateDialogElement.update!.deviceName)));
    assertEquals(
        getTextContent('#updateDialogBody'),
        loadTimeData.getString('restartingBodyText'));
    assertEquals(
        getTextContent('#progress'),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
    // No percentage progress bar.
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar'));
  });

  test('UpdateDialogContent', async () => {
    assert(updateDialogElement?.shadowRoot);
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog()!.open);

    // Check dialog contents
    assertEquals(
        getTextContent('#updateDialogTitle'),
        loadTimeData.getStringF(
            'updating',
            mojoString16ToString(updateDialogElement.update!.deviceName)));
    assertEquals(
        getTextContent('#updateDialogBody'),
        loadTimeData.getString('updatingInfo'));
    const percentBar = updateDialogElement.shadowRoot.querySelector(
                           '#updateProgressBar') as PaperProgressElement;
    const percentBarStatus = percentBar.value;
    assertEquals(1, percentBarStatus);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(getUpdateDialog().open);

    // Correct text is shown.
    assertEquals(
        getTextContent('#updateDialogTitle'),
        loadTimeData.getStringF(
            'restartingTitleText',
            mojoString16ToString(updateDialogElement.update!.deviceName)));
    assertEquals(
        getTextContent('#updateDialogBody'),
        loadTimeData.getString('restartingBodyText'));
    assertEquals(
        getTextContent('#progress'),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
    // No percentage progress bar.
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar'));
  });

  test('ProgressBarAppears', async () => {
    assert(updateDialogElement?.shadowRoot);
    // Simulate update inflight, but restarting. Idle state during inflight
    // is equivalent as a restart phase.
    setIsInitiallyInflight(/*inflight=*/ true);
    await flushTasks();
    await setInstallationProgress(0, UpdateState.kIdle);
    assertTrue(getUpdateDialog().open);
    assertTrue(isVisible(strictQuery(
        '#indeterminateProgressBar', updateDialogElement.shadowRoot,
        HTMLElement)));

    // Set inflight to false, expect no progress bar.
    setIsInitiallyInflight(/*inflight=*/ false);
    await flushTasks();
    assertFalse(isVisible(updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar')));
  });

  test('UpdateDialogContent_WaitingForUser_V2Disabled', async () => {
    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: false});

    assert(updateDialogElement?.shadowRoot);
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is waiting for user action.
    await setInstallationProgress(70, UpdateState.kWaitingForUser);
    assertTrue(getUpdateDialog().open);

    // If the v2 flag is disabled, the dialog should indicate that it's
    // restarting when the state is kWaitingForUser.
    assertEquals(
        getTextContent('#updateDialogTitle'),
        loadTimeData.getStringF(
            'restartingTitleText',
            mojoString16ToString(updateDialogElement.update!.deviceName)));
    assertEquals(
        getTextContent('#updateDialogBody'),
        loadTimeData.getString('restartingBodyText'));
    assertEquals(
        getTextContent('#progress'),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
    // No percentage progress bar.
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar'));
  });
});
