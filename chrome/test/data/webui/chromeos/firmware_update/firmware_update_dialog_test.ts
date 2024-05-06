// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessory-update/firmware_update_dialog.js';

import {fakeFirmwareUpdate} from 'chrome://accessory-update/fake_data.js';
import {DeviceRequest, DeviceRequestId, DeviceRequestKind, UpdateState} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {FirmwareUpdateDialogElement} from 'chrome://accessory-update/firmware_update_dialog.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PaperProgressElement} from 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('FirmwareUpdateDialogTest', () => {
  let updateDialogElement: FirmwareUpdateDialogElement|null = null;

  teardown(() => {
    updateDialogElement?.remove();
    updateDialogElement = null;
  });

  function createUpdateDialogElement(): void {
    updateDialogElement = document.createElement('firmware-update-dialog');
    assert(updateDialogElement);
    updateDialogElement.update = fakeFirmwareUpdate;
    updateDialogElement.installationProgress = {
      percentage: 0,
      state: UpdateState.kIdle,
    };
    document.body.appendChild(updateDialogElement);
  }

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

  function createDeviceRequest(
      id: DeviceRequestId,
      kind: DeviceRequestKind = DeviceRequestKind.kImmediate): DeviceRequest {
    return {id, kind};
  }

  test('DialogStateUpdatesCorrectly', async () => {
    createUpdateDialogElement();
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
    createUpdateDialogElement();
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
    // Body text should not have an aria-live value for non-requests.
    assertEquals(
        strictQuery(
            '#updateDialogBody', updateDialogElement.shadowRoot, HTMLDivElement)
            .ariaLive,
        '');
    assertEquals(
        getTextContent('#progress'),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(isVisible(updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar')));
    // No percentage progress bar.
    assertFalse(isVisible(
        updateDialogElement.shadowRoot.querySelector('#updateProgressBar')));
  });

  test('UpdateDialogContent', async () => {
    createUpdateDialogElement();
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
    assertTrue(isVisible(updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar')));
    // No percentage progress bar.
    assertFalse(isVisible(
        updateDialogElement.shadowRoot.querySelector('#updateProgressBar')));
  });

  test('ProgressBarAppears', async () => {
    createUpdateDialogElement();
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
    createUpdateDialogElement();
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
    // Body text should not have an aria-live value for non-requests.
    assertEquals(
        strictQuery(
            '#updateDialogBody', updateDialogElement.shadowRoot, HTMLDivElement)
            .ariaLive,
        '');
    assertEquals(
        getTextContent('#progress'),
        loadTimeData.getString('restartingFooterText'));
    // Check that the indeterminate progress is shown.
    assertTrue(isVisible(updateDialogElement.shadowRoot.querySelector(
        '#indeterminateProgressBar')));
    // No percentage progress bar.
    assertFalse(isVisible(
        updateDialogElement.shadowRoot.querySelector('#updateProgressBar')));
  });

  test('UpdateDialogContent_DeviceRequest_V2Disabled', async () => {
    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: false});
    createUpdateDialogElement();

    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is waiting for user action.
    await setInstallationProgress(70, UpdateState.kWaitingForUser);
    assertTrue(getUpdateDialog().open);

    // Device requests are not expected when the flag is disabled, so
    // throw an error.
    assertThrows(
        () => updateDialogElement?.onDeviceRequest(createDeviceRequest(
            DeviceRequestId.kDoNotPowerOff,
            DeviceRequestKind.kImmediate,
            )));
  });

  test('UpdateDialogContent_WaitingForUser_V2Enabled_ShowUpdate', async () => {
    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: true});
    createUpdateDialogElement();
    assert(updateDialogElement?.shadowRoot);

    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is waiting for user action.
    await setInstallationProgress(70, UpdateState.kWaitingForUser);
    assertTrue(getUpdateDialog().open);

    // If the v2 flag is enabled, and the status is kWaitingForUser, but the
    // element hasn't received the onDeviceRequest call yet, it should just
    // show the normal update dialog.
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
    assertEquals(70, percentBarStatus);
  });

  test(
      'UpdateDialogContent_DeviceRequest_V2Enabled_IgnoreNonImmediate',
      async () => {
        loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: true});
        createUpdateDialogElement();
        assert(updateDialogElement?.shadowRoot);

        // Start update.
        await setInstallationProgress(1, UpdateState.kUpdating);
        assertTrue(getUpdateDialog().open);

        // Dialog remains open while the device is waiting for user action.
        await setInstallationProgress(70, UpdateState.kWaitingForUser);
        assertTrue(getUpdateDialog().open);

        // Non-immediate device requests should continue to show the default
        // update screen as if no request was received.
        updateDialogElement.onDeviceRequest(createDeviceRequest(
            DeviceRequestId.kDoNotPowerOff, DeviceRequestKind.kPost));
        await flushTasks();
        assertEquals(
            getTextContent('#updateDialogTitle'),
            loadTimeData.getStringF(
                'updating',
                mojoString16ToString(updateDialogElement.update!.deviceName)));
        assertEquals(
            getTextContent('#updateDialogBody'),
            loadTimeData.getString('updatingInfo'));
        let percentBar = updateDialogElement.shadowRoot.querySelector(
                             '#updateProgressBar') as PaperProgressElement;
        let percentBarStatus = percentBar.value;
        assertEquals(70, percentBarStatus);

        updateDialogElement.onDeviceRequest(createDeviceRequest(
            DeviceRequestId.kDoNotPowerOff, DeviceRequestKind.kUnknown));
        await flushTasks();

        // Non-immediate device requests should continue to show the default
        // update screen as if no request was received.
        assertEquals(
            getTextContent('#updateDialogTitle'),
            loadTimeData.getStringF(
                'updating',
                mojoString16ToString(updateDialogElement.update!.deviceName)));
        assertEquals(
            getTextContent('#updateDialogBody'),
            loadTimeData.getString('updatingInfo'));
        percentBar = updateDialogElement.shadowRoot.querySelector(
                         '#updateProgressBar') as PaperProgressElement;
        percentBarStatus = percentBar.value;
        assertEquals(70, percentBarStatus);
      });

  test('UpdateDialogContent_DeviceRequest_V2Enabled', async () => {
    loadTimeData.overrideValues({isFirmwareUpdateUIV2Enabled: true});
    createUpdateDialogElement();
    assert(updateDialogElement?.shadowRoot);

    // Start update.
    await setInstallationProgress(1, UpdateState.kUpdating);
    assertTrue(getUpdateDialog().open);

    // Dialog remains open while the device is waiting for user action.
    await setInstallationProgress(70, UpdateState.kWaitingForUser);
    assertTrue(getUpdateDialog().open);

    // If the v2 flag is enabled, the dialog should show the associated string
    // for the given device request.
    const idToExpectedString: Map<DeviceRequestId, string> = new Map([
      [DeviceRequestId.kRemoveReplug, 'requestIdRemoveReplug'],
      [DeviceRequestId.kInsertUSBCable, 'requestIdInsertUsbCable'],
      [DeviceRequestId.kRemoveUSBCable, 'requestIdRemoveUsbCable'],
      [DeviceRequestId.kPressUnlock, 'requestIdPressUnlock'],
      [DeviceRequestId.kDoNotPowerOff, 'requestIdDoNotPowerOff'],
      [DeviceRequestId.kReplugInstall, 'requestIdReplugInstall'],
    ]);

    const deviceName =
        mojoString16ToString(updateDialogElement.update!.deviceName);

    for (const [deviceRequestID, expectedString] of idToExpectedString
             .entries()) {
      updateDialogElement.onDeviceRequest(createDeviceRequest(deviceRequestID));
      await flushTasks();

      // Title of dialog should be generic "Updating [device]"
      assertEquals(
          getTextContent('#updateDialogTitle'),
          loadTimeData.getStringF('updating', deviceName));

      // Body of dialog should correspond to the type of request.
      // In the case of the "DoNotPowerOff" request type, the device name is not
      // part of the body text, so the expected string does not include the
      // device name.
      if (deviceRequestID === DeviceRequestId.kDoNotPowerOff) {
        assertEquals(
            getTextContent('#updateDialogBody'),
            loadTimeData.getString(expectedString));
      } else {
        assertEquals(
            getTextContent('#updateDialogBody'),
            loadTimeData.getStringF(expectedString, deviceName));
      }

      assert(updateDialogElement?.shadowRoot);
      // For user requests, the dialog body should be an assertive aria-live
      // region.
      assertEquals(
          strictQuery(
              '#updateDialogBody', updateDialogElement.shadowRoot,
              HTMLDivElement)
              .ariaLive,
          'assertive');
      assertEquals(
          getTextContent('#progress'),
          loadTimeData.getStringF('waitingFooterText', 70));

      // Percentage progress should be shown when waiting for user action, but
      // the bar should be disabled.
      assertTrue(isVisible(
          updateDialogElement.shadowRoot.querySelector('#updateProgressBar')));
      assertTrue(
          !!updateDialogElement.shadowRoot.querySelector('#updateProgressBar')!
                .hasAttribute('disabled'));
      // Indeterminate progress should not be shown.
      assertFalse(isVisible(updateDialogElement.shadowRoot.querySelector(
          '#indeterminateProgressBar')));
    }
  });
});
