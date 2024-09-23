// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HidPreservingBluetoothStateControllerInterface} from 'chrome://resources/ash/common/bluetooth/hid_preserving_bluetooth_state_controller.mojom-webui.js';
import type {CrosBluetoothConfigInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {assertTrue} from '../chai_assert.js';

/**
 * @fileoverview Fake implementation of HidPreservingBluetoothStateController
 * for testing.
 */

export class FakeHidPreservingBluetoothStateController implements
    HidPreservingBluetoothStateControllerInterface {
  bluetoothConfig: CrosBluetoothConfigInterface|undefined;
  shouldShowWarningDialog: boolean = false;
  pendingBluetoothEnabledRequest: boolean = false;
  dialogShownCount: number = 0;

  setBluetoothConfigForTesting(testBluetoothConfig?:
                                   CrosBluetoothConfigInterface): void {
    this.bluetoothConfig = testBluetoothConfig;
  }

  tryToSetBluetoothEnabledState(enabled: boolean) {
    if (this.shouldShowWarningDialog && !enabled) {
      this.dialogShownCount++;
      this.pendingBluetoothEnabledRequest = enabled;
      return;
    }

    this.setBluetoothEnabledState(enabled);
  }

  setShouldShowWarningDialog(shouldShowWarningDialog: boolean) {
    this.shouldShowWarningDialog = shouldShowWarningDialog;
  }

  completeShowDialog(showDialogResult: boolean) {
    assertTrue(this.shouldShowWarningDialog);

    if (!showDialogResult) {
      return;
    }

    this.setBluetoothEnabledState(this.pendingBluetoothEnabledRequest);
  }

  setBluetoothEnabledState(enabled: boolean) {
    assertTrue(!!this.bluetoothConfig);
    this.bluetoothConfig!.setBluetoothEnabledState(enabled);
  }

  getDialogShownCount(): number {
    return this.dialogShownCount;
  }
}
