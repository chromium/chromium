// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingFirmwareUpdatePageElement} from 'chrome://shimless-rma/reimaging_firmware_update_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function reimagingFirmwareUpdatePageTest() {
  /** @type {?ReimagingFirmwareUpdatePageElement} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  suiteSetup(() => {
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @param {boolean} reimageRequired
   * @return {!Promise}
   */
  function initializeReimagingFirmwareUpdatePage(reimageRequired) {
    assertFalse(!!component);

    service.setReimageRequiredResult(reimageRequired);
    component = /** @type {!ReimagingFirmwareUpdatePageElement} */ (
        document.createElement('reimaging-firmware-update-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ReimagingFirmwareUpdatePageRequiredInitializes', async () => {
    await initializeReimagingFirmwareUpdatePage(true);
    await flushTasks();
    const downloadReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageDownload');
    const usbReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageUsb');
    const skipReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageSkip');

    assertFalse(downloadReimageComponent.checked);
    assertFalse(usbReimageComponent.checked);
    assertFalse(skipReimageComponent.checked);
    assertTrue(skipReimageComponent.hidden);
  });

  test('ReimagingFirmwareUpdatePageNotRequiredInitializes', async () => {
    await initializeReimagingFirmwareUpdatePage(false);
    const downloadReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageDownload');
    const usbReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageUsb');
    const skipReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageSkip');

    assertFalse(downloadReimageComponent.checked);
    assertFalse(usbReimageComponent.checked);
    assertFalse(skipReimageComponent.checked);
    assertFalse(skipReimageComponent.hidden);
  });

  test('ReimagingFirmwareUpdatePageOneChoiceOnly', async () => {
    await initializeReimagingFirmwareUpdatePage(true);
    const downloadReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageDownload');
    const usbReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageUsb');

    downloadReimageComponent.click();
    await flushTasks;

    assertTrue(downloadReimageComponent.checked);
    assertFalse(usbReimageComponent.checked);

    usbReimageComponent.click();
    await flushTasks;

    assertFalse(downloadReimageComponent.checked);
    assertTrue(usbReimageComponent.checked);
  });

  test('SelectDownloadImage', async () => {
    const resolver = new PromiseResolver();
    let callCounter = 0;
    await initializeReimagingFirmwareUpdatePage(true);
    service.reimageFromUsb = () => assertTrue(false);
    service.reimageSkipped = () => assertTrue(false);
    service.reimageFromDownload = () => {
      callCounter++;
      return resolver.promise;
    };
    const downloadReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageDownload');

    downloadReimageComponent.click();
    await flushTasks();
    assertTrue(downloadReimageComponent.checked);

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('SelectUsbImage', async () => {
    const resolver = new PromiseResolver();
    let callCounter = 0;
    await initializeReimagingFirmwareUpdatePage(true);
    service.reimageFromDownload = () => assertTrue(false);
    service.reimageSkipped = () => assertTrue(false);
    service.reimageFromUsb = () => {
      callCounter++;
      return resolver.promise;
    };
    const usbReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageUsb');

    usbReimageComponent.click();
    await flushTasks();
    assertTrue(usbReimageComponent.checked);

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('SelectSkipImage', async () => {
    const resolver = new PromiseResolver();
    let callCounter = 0;
    await initializeReimagingFirmwareUpdatePage(false);
    service.reimageFromDownload = () => assertTrue(false);
    service.reimageFromUsb = () => assertTrue(false);
    service.reimageSkipped = () => {
      callCounter++;
      return resolver.promise;
    };
    const skipReimageComponent =
        component.shadowRoot.querySelector('#firmwareReimageSkip');

    skipReimageComponent.click();
    await flushTasks();
    assertTrue(skipReimageComponent.checked);

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertDeepEquals(savedResult, expectedResult);
  });
}
