// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {UpdateRoFirmwarePage} from 'chrome://shimless-rma/reimaging_firmware_update_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {StateResult, UpdateRoFirmwareStatus} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('reimagingFirmwareUpdatePageTest', function() {
  // ShimlessRma is needed to handle the 'transition-state' event used when
  // the firmware update is complete.
  let shimlessRmaComponent: ShimlessRma|null = null;

  let component: UpdateRoFirmwarePage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
    shimlessRmaComponent?.remove();
    shimlessRmaComponent = null;
  });

  function initializeReimagingFirmwareUpdatePage(): Promise<void> {
    assert(!shimlessRmaComponent);
    shimlessRmaComponent = document.createElement(ShimlessRma.is);
    assert(shimlessRmaComponent);
    document.body.appendChild(shimlessRmaComponent);

    assert(!component);
    component = document.createElement(UpdateRoFirmwarePage.is);
    assert(component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify initialize page.
  test('RoFirmwareUpdatePageInitializes', async () => {
    await initializeReimagingFirmwareUpdatePage();

    assert(component);
    const updateStatus =
        strictQuery('#firmwareUpdateStatus', component.shadowRoot, HTMLElement);
    assertFalse(updateStatus.hidden);
    assertEquals('', updateStatus.textContent!.trim());
  });

  // Verify unplugging the USB while still updating won't trigger update
  // complete transition.
  test('UnplugUsbWhileUpdating', async () => {
    await initializeReimagingFirmwareUpdatePage();

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let callCount = 0;
    service.roFirmwareUpdateComplete = () => {
      ++callCount;
      return resolver.promise;
    };

    // Simulate unplugging the USB and expect no complete call.
    service.triggerExternalDiskObserver(
        /* detected= */ false, /* delayMs= */ 0);
    await flushTasks();
    assertEquals(0, callCount);

    // Simulate unplugging the USB from an updating state and still expect
    // no complete call.
    service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kUpdating, /* delayMs= */ 0);
    service.triggerExternalDiskObserver(
        /* detected= */ false, /* delayMs= */ 0);
    await flushTasks();
    assertEquals(0, callCount);
  });

  // Verify unplugging the USB when the update is complete triggers the update
  // complete transition.
  test('UnplugUsbWhileComplete', async () => {
    service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kUpdating, /* delayMs= */ 0);
    await initializeReimagingFirmwareUpdatePage();

    assert(component);
    const firmwareTitle =
        strictQuery('#titleText', component.shadowRoot, HTMLElement);
    assertEquals(
        loadTimeData.getString('firmwareUpdateInstallImageTitleText'),
        firmwareTitle.textContent!.trim());

    let callCount = 0;
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    service.roFirmwareUpdateComplete = () => {
      ++callCount;
      return resolver.promise;
    };

    // Complete the update.
    service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kComplete, /* delayMs= */ 0);
    await flushTasks();

    // Make sure that the transition doesn't happen until the USB is
    // unplugged.
    assertEquals(0, callCount);

    // Confirm the page title changes after firmware install completes.
    assertEquals(
        loadTimeData.getString('firmwareUpdateInstallCompleteTitleText'),
        firmwareTitle.textContent!.trim());

    // Unplug the USB and verify that the transition happened.
    service.triggerExternalDiskObserver(
        /* detected= */ false, /* delayMs= */ 0);
    await flushTasks();
    assertEquals(1, callCount);
  });
});
