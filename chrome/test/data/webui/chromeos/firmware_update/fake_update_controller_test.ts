// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeDeviceRequest, fakeInstallationProgress} from 'chrome://accessory-update/fake_data.js';
import {FakeUpdateController} from 'chrome://accessory-update/fake_update_controller.js';
import {DeviceRequestObserverRemote, UpdateProgressObserverRemote, UpdateState} from 'chrome://accessory-update/firmware_update.mojom-webui.js';
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

  test('StartUpdateWithRequest', async () => {
    assert(controller);
    // The fake device with id '4' is the one that triggers a request flow.
    const deviceId = '4';
    controller.setUpdateIntervalInMs(0);
    controller.setDeviceIdForUpdateInProgress(deviceId);

    // Save the most recent update to verify the correct update state is active
    // when a request is received.
    let lastUpdateState: UpdateState|undefined;

    const updateProgressObserverRemote: UpdateProgressObserverRemote = {
      onStatusChanged: (update) => {
        lastUpdateState = update.state;
      },
    } as UpdateProgressObserverRemote;

    const deviceRequestObserverRemote: DeviceRequestObserverRemote = {
      onDeviceRequest: (request) => {
        assertEquals(UpdateState.kWaitingForUser, lastUpdateState);
        assertEquals(fakeDeviceRequest.id, request.id);
        assertEquals(fakeDeviceRequest.kind, request.kind);
      },
    } as DeviceRequestObserverRemote;

    controller.addUpdateProgressObserver(updateProgressObserverRemote);
    controller.addDeviceRequestObserver(deviceRequestObserverRemote);

    const filePath: FilePath = {path: 'test1.cab'};
    controller.beginUpdate(deviceId, filePath);
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    assertFalse(controller.getIsUpdateInProgressForTesting());
    assertTrue(
        controller.getCompletedFirmwareUpdatesForTesting().has(deviceId));
    assertEquals(UpdateState.kSuccess, lastUpdateState);
  });

  test('StartUpdateWithRequestAndFailure', async () => {
    assert(controller);
    // The fake device with id '5' is the one that triggers a request flow, but
    // fails right after the WaitingForUser status (simulating a timeout).
    const deviceId = '5';
    controller.setUpdateIntervalInMs(0);
    controller.setDeviceIdForUpdateInProgress(deviceId);

    // Save the most recent update to verify the correct update state is active
    // when a request is received.
    let lastUpdateState: UpdateState|undefined;

    const updateProgressObserverRemote: UpdateProgressObserverRemote = {
      onStatusChanged: (update) => {
        lastUpdateState = update.state;
      },
    } as UpdateProgressObserverRemote;

    const deviceRequestObserverRemote: DeviceRequestObserverRemote = {
      onDeviceRequest: (request) => {
        assertEquals(UpdateState.kWaitingForUser, lastUpdateState);
        assertEquals(fakeDeviceRequest.id, request.id);
        assertEquals(fakeDeviceRequest.kind, request.kind);
      },
    } as DeviceRequestObserverRemote;

    controller.addUpdateProgressObserver(updateProgressObserverRemote);
    controller.addDeviceRequestObserver(deviceRequestObserverRemote);

    const filePath: FilePath = {path: 'test1.cab'};
    controller.beginUpdate(deviceId, filePath);
    // Allow firmware update to complete.
    await controller.getUpdateCompletedPromiseForTesting();
    assertFalse(controller.getIsUpdateInProgressForTesting());
    assertTrue(
        controller.getCompletedFirmwareUpdatesForTesting().has(deviceId));
    assertEquals(UpdateState.kFailed, lastUpdateState);
  });
});
