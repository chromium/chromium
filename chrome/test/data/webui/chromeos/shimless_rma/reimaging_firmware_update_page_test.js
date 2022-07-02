// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {UpdateRoFirmwarePage} from 'chrome://shimless-rma/reimaging_firmware_update_page.js';
import {UpdateRoFirmwareStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingFirmwareUpdatePageTest() {
  /** @type {?UpdateRoFirmwarePage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = '';
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /** @return {!Promise} */
  function initializeReimagingFirmwareUpdatePage() {
    assertFalse(!!component);

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

  test('RoFirmwareUpdateStartingDisablesNext', async () => {
    await initializeReimagingFirmwareUpdatePage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'RO Firmware update is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('RoFirmwareUpdateInProgressDisablesNext', async () => {
    await initializeReimagingFirmwareUpdatePage();
    service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kUpdating, 0);
    await flushTasks();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'RO Firmware update is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('RoFirmwareUpdateEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeReimagingFirmwareUpdatePage();
    service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kComplete, 0);
    await flushTasks();
    service.roFirmwareUpdateComplete = () => {
      return resolver.promise;
    };

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertDeepEquals(savedResult, expectedResult);
  });
}
