// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeInstallationProgress} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {UpdateProgressObserverRemote} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('FakeUpdateController', () => {
  let controller: FakeUpdateController|null = null;

  setup(() => controller = new FakeUpdateController());

  teardown(() => {
    assert(controller);
    controller.reset();
    controller = null;
  });

  test('StartUpdate', async () => {
    assert(controller);
    const deviceId = '1';
    controller.setUpdateIntervalInMs(0);
    controller.setDeviceIdForUpdateInProgress(deviceId);
    // Keep track of which observation we should get.
    let onStatusChangedCallCount = 0;

    const updateProgressObserverRemote = {
      onStatusChanged: (update) => {
        // Only expect 3 calls.
        assertTrue(onStatusChangedCallCount <= 2);
        assertTrue(!!fakeInstallationProgress[onStatusChangedCallCount]);
        assertEquals(
            fakeInstallationProgress[onStatusChangedCallCount]!.percentage,
            update.percentage);
        assertEquals(
            fakeInstallationProgress[onStatusChangedCallCount]!.state,
            update.state);
        onStatusChangedCallCount++;
      },
    } as UpdateProgressObserverRemote;

    controller.addUpdateProgressObserver(updateProgressObserverRemote);
    const filePath: FilePath = {path: 'test1.cab'};
    controller.beginUpdate(deviceId, filePath);
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    assertFalse(controller.getIsUpdateInProgressForTesting());
    assertTrue(
        controller.getCompletedFirmwareUpdatesForTesting().has(deviceId));
  });
});
