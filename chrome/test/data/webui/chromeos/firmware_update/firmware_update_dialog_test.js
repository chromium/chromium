// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeFirmwareUpdate} from 'chrome://accessory-update/fake_data.js';
import {FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';
import {FirmwareUpdate, UpdateState} from 'chrome://accessory-update/firmware_update_types.js';
import {mojoString16ToString} from 'chrome://accessory-update/mojo_utils.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from '../test_util.js';

export function firmwareUpdateDialogTest() {
  /** @type {?FirmwareUpdateDialogElement} */
  let updateDialogElement = null;

  setup(() => {
    updateDialogElement = /** @type {!FirmwareUpdateDialogElement} */ (
        document.createElement('firmware-update-dialog'));
    updateDialogElement.update = fakeFirmwareUpdate;
    updateDialogElement.installationProgress = {
      percentage: 0,
      state: UpdateState.kIdle,
    };
    document.body.appendChild(updateDialogElement);
  });

  teardown(() => {
    updateDialogElement.remove();
    updateDialogElement = null;
  });

  /**
   * @param {number} percentage
   * @param {!UpdateState} state
   * @return {!Promise}
   */
  function setInstallationProgress(percentage, state) {
    updateDialogElement.installationProgress = {percentage, state};
    return flushTasks();
  }

  /** @return {!Promise} */
  function clickDoneButton() {
    updateDialogElement.shadowRoot.querySelector('#updateDoneButton').click();
    return flushTasks();
  }

  /**
   * @suppress {visibility}
   * @param {boolean} inflight
   */
  function setIsInitiallyInflight(inflight) {
    updateDialogElement.isInitiallyInflight_ = inflight;
  }

  test('DialogStateUpdatesCorrectly', async () => {
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // |UpdateState.KIdle| handled correctly while an update is still
    // in-progress.
    await setInstallationProgress(20, UpdateState.kIdle);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Dialog remains open when the update is completed.
    await setInstallationProgress(100, UpdateState.kSuccess);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Dialog closes when the "Done" button is clicked.
    await clickDoneButton();
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateDialog'));
  });

  test('DeviceRestarting', async () => {
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Correct text is shown.
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogTitle')
            .textContent.trim(),
        loadTimeData.getStringF(
            'restartingTitleText',
            mojoString16ToString(updateDialogElement.update.deviceName)));
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogBody')
            .textContent.trim(),
        loadTimeData.getString('restartingBodyText'));
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#progress')
            .textContent.trim(),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
    // No percentage progress bar.
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar'));
  });

  test('UpdateDialogContent', async () => {
    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Check dialog contents
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogTitle')
            .textContent.trim(),
        loadTimeData.getStringF(
            'updating',
            mojoString16ToString(updateDialogElement.update.deviceName)));
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogBody')
            .textContent.trim(),
        loadTimeData.getString('updatingInfo'));
    const percentBarStatus =
        updateDialogElement.shadowRoot.querySelector('#updateProgressBar')
            .value;
    assertEquals(1, percentBarStatus);

    // Dialog remains open while the device is restarting.
    await setInstallationProgress(70, UpdateState.kRestarting);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);

    // Correct text is shown.
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogTitle')
            .textContent.trim(),
        loadTimeData.getStringF(
            'restartingTitleText',
            mojoString16ToString(updateDialogElement.update.deviceName)));
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#updateDialogBody')
            .textContent.trim(),
        loadTimeData.getString('restartingBodyText'));
    assertEquals(
        updateDialogElement.shadowRoot.querySelector('#progress')
            .textContent.trim(),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
    // No percentage progress bar.
    assertFalse(
        !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar'));
  });

  test('ProgressBarAppears', async () => {
    // Simulate update inflight, but restarting. Idle state during inflight
    // is equivalent as a restart phase.
    setIsInitiallyInflight(/*inflight=*/ true);
    await flushTasks();
    await setInstallationProgress(0, UpdateState.kIdle);
    assertTrue(
        updateDialogElement.shadowRoot.querySelector('#updateDialog').open);
    assertTrue(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));

    // Set inflight to false, expect no progress bar.
    setIsInitiallyInflight(/*inflight=*/ false);
    await flushTasks();
    assertFalse(!!updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar'));
  });
}
