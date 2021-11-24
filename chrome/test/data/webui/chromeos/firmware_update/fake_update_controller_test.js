// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeInstallationProgress} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {UpdateProgressObserver} from 'chrome://accessory-update/firmware_update_types.js';

import {assertDeepEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

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
    // Keep track of which observation we should get.
    let observerCallCount = 0;
    /** @type {!UpdateProgressObserver} */
    const updateProgressObserverRemote = {
      onProgressChanged: (installationProgress) => {
        // Only expect 3 calls.
        assertTrue(observerCallCount <= 2);
        assertDeepEquals(
            fakeInstallationProgress[observerCallCount++],
            installationProgress);
      }
    };

    controller.startUpdate(deviceId, updateProgressObserverRemote);
    await flushTasks();
    return controller
        .getStartUpdatePromiseForTesting()
        // Use flushTasks to process the 3 fake installation progress
        // observations.
        .then(() => flushTasks())
        .then(() => flushTasks())
        .then(() => flushTasks())
        // Trigger stops when update is completed.
        .then(() => {
          assertFalse(controller.isUpdateInProgress());
          assertTrue(getCompletedFirmwareUpdates().has(deviceId));
        });
  });
}
