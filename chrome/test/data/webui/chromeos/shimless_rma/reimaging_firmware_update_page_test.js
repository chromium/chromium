// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {UpdateRoFirmwarePage} from 'chrome://shimless-rma/reimaging_firmware_update_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {UpdateRoFirmwareStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('reimagingFirmwareUpdatePageTest', function() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimlessRmaComponent = null;

  /** @type {?UpdateRoFirmwarePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    shimlessRmaComponent.remove();
    shimlessRmaComponent = null;
    component.remove();
    component = null;
    service.reset();
  });

  /** @return {!Promise} */
  function initializeReimagingFirmwareUpdatePage() {
    assertFalse(!!component);

    shimlessRmaComponent =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    component = /** @type {!UpdateRoFirmwarePage} */ (
        document.createElement('reimaging-firmware-update-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('RoFirmwareUpdatePageInitializes', async () => {
    await initializeReimagingFirmwareUpdatePage();
    await flushTasks();
    const updateStatus =
        component.shadowRoot.querySelector('#firmwareUpdateStatus');
    assertFalse(updateStatus.hidden);
    assertEquals('', updateStatus.textContent.trim());
  });

  test(
      'UnpluggingUsbDoesntTriggerTransitionToNextPageIfUpdateIsNotComplete',
      async () => {
        const resolver = new PromiseResolver();

        let callCount = 0;
        service.roFirmwareUpdateComplete = () => {
          callCount++;
          return resolver.promise;
        };

        await initializeReimagingFirmwareUpdatePage();
        service.triggerExternalDiskObserver(false, 0);
        await flushTasks();
        assertEquals(0, callCount);

        service.triggerUpdateRoFirmwareObserver(
            UpdateRoFirmwareStatus.kUpdating, 0);
        await flushTasks();

        service.triggerExternalDiskObserver(false, 0);
        await flushTasks();
        assertEquals(0, callCount);
      });

  test(
      'UnpluggingUsbTriggersTransitionToNextPageIfUpdateIsComplete',
      async () => {
        const resolver = new PromiseResolver();
        await initializeReimagingFirmwareUpdatePage();

        const firmwareTitle = component.shadowRoot.querySelector('#titleText');
        assertEquals(
            loadTimeData.getString('firmwareUpdateInstallImageTitleText'),
            firmwareTitle.textContent.trim());

        let callCount = 0;
        service.roFirmwareUpdateComplete = () => {
          callCount++;
          return resolver.promise;
        };

        // Complete the update.
        service.triggerUpdateRoFirmwareObserver(
            UpdateRoFirmwareStatus.kComplete, 0);
        await flushTasks();

        // Make sure that the transition doesn't happen until the USB is
        // unplugged.
        assertEquals(0, callCount);

        // Confirm the page title changes after firmware install completes.
        assertEquals(
            loadTimeData.getString('firmwareUpdateInstallCompleteTitleText'),
            firmwareTitle.textContent.trim());

        // Unplug the USB and verify that the transition happened.
        service.triggerExternalDiskObserver(false, 0);
        await flushTasks();
        assertEquals(1, callCount);
      });
});
