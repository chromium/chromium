// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeInstallationProgress} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {UpdateProgressObserverRemote} from 'chrome://accessory-update/firmware_update_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

export function fakeUpdateControllerTest() {
  /** @type {?FakeUpdateController} */
  let controller = null;

  setup(() => controller = new FakeUpdateController());

  teardown(() => {
    controller.reset();
    controller = null;
  });

  /**
   * @suppress {visibility}
   * @return {!Set<string>}
   */
  function getCompletedFirmwareUpdates() {
    return controller.completedFirmwareUpdates_;
  }

  test('StartUpdate', async () => {
    const deviceId = '1';
    controller.setUpdateIntervalInMs(0);
    controller.setDeviceIdForUpdateInProgress(deviceId);
    // Keep track of which observation we should get.
    let onStatusChangedCallCount = 0;

    const updateProgressObserverRemote =
        /** @type {!UpdateProgressObserverRemote} */ ({
          onStatusChanged: (update) => {
            // Only expect 3 calls.
            assertTrue(onStatusChangedCallCount <= 2);
            assertEquals(
                fakeInstallationProgress[onStatusChangedCallCount].percentage,
                update.percentage);
            assertEquals(
                fakeInstallationProgress[onStatusChangedCallCount].state,
                update.state);
            onStatusChangedCallCount++;
          },
        });

    controller.addObserver(updateProgressObserverRemote);
    controller.beginUpdate(deviceId, /*filepath*/ {path: 'test1.cab'});
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    assertFalse(controller.isUpdateInProgress());
    assertTrue(getCompletedFirmwareUpdates().has(deviceId));
  });
}
