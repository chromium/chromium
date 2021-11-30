// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeDeviceRegions, fakeDeviceSkus} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingDeviceInformationPage} from 'chrome://shimless-rma/reimaging_device_information_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

let fakeSerialNumber = 'serial# 0001';

// TODO(gavindodd) how to update selectedIndex and trigger on-change
// automatically.
/**
 * onSelected*Change is not triggered automatically and the functions are
 * protected. It is not possible to suppress visibility inline so this helper
 * function wraps them.
 * @suppress {visibility}
 */
function suppressedComponentOnSelectedChange_(component) {
  component.onSelectedRegionChange_('ignored');
  component.onSelectedSkuChange_('ignored');
}

export function reimagingDeviceInformationPageTest() {
  /** @type {?ReimagingDeviceInformationPage} */
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
   * @param {string} serialNumber
   * @return {!Promise}
   */
  function initializeReimagingDeviceInformationPage(serialNumber) {
    assertFalse(!!component);
    service.setGetOriginalSerialNumberResult(serialNumber);
    service.setGetRegionListResult(fakeDeviceRegions);
    service.setGetOriginalRegionResult(2);
    service.setGetSkuListResult(fakeDeviceSkus);
    service.setGetOriginalSkuResult(1);

    component = /** @type {!ReimagingDeviceInformationPage} */ (
        document.createElement('reimaging-device-information-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ReimagingDeviceInformationPageInitializes', async () => {
    await initializeReimagingDeviceInformationPage(fakeSerialNumber);
    // A flush tasks is required to wait for the drop lists to render and set
    // the initial selected index.
    await flushTasks();
    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    const resetSerialNumberComponent =
        component.shadowRoot.querySelector('#resetSerialNumber');
    const resetRegionComponent =
        component.shadowRoot.querySelector('#resetRegion');
    const resetSkuComponent = component.shadowRoot.querySelector('#resetSku');

    assertEquals(fakeSerialNumber, serialNumberComponent.value);
    assertEquals(2, regionSelectComponent.selectedIndex);
    assertEquals(1, skuSelectComponent.selectedIndex);
    assertTrue(resetSerialNumberComponent.disabled);
    assertTrue(resetRegionComponent.disabled);
    assertTrue(resetSkuComponent.disabled);
  });

  test('ReimagingDeviceInformationPageInitializes', async () => {
    const resolver = new PromiseResolver();
    await initializeReimagingDeviceInformationPage(fakeSerialNumber);
    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    let expectedSerialNumber = 'expected serial number';
    let expectedRegionIndex = 0;
    let expectedSkuIndex = 2;
    serialNumberComponent.value = expectedSerialNumber;
    regionSelectComponent.selectedIndex = expectedRegionIndex;
    skuSelectComponent.selectedIndex = expectedSkuIndex;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();
    let serialNumber;
    let regionIndex;
    let skuIndex;
    let callCounter = 0;
    service.setDeviceInformation =
        (resultSerialNumber, resultRegionIndex, resultSkuIndex) => {
          callCounter++;
          serialNumber = resultSerialNumber;
          regionIndex = resultRegionIndex;
          skuIndex = resultSkuIndex;
          return resolver.promise;
        };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(callCounter, 1);
    assertEquals(serialNumber, expectedSerialNumber);
    assertEquals(regionIndex, expectedRegionIndex);
    assertEquals(skuIndex, expectedSkuIndex);
    assertDeepEquals(savedResult, expectedResult);
  });

  test('ReimagingDeviceInformationPageModifySerialNumberAndReset', async () => {
    await initializeReimagingDeviceInformationPage(fakeSerialNumber);
    let serialNumber = fakeSerialNumber + 'new serial number';
    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const resetSerialNumberComponent =
        component.shadowRoot.querySelector('#resetSerialNumber');

    assertEquals(serialNumberComponent.value, fakeSerialNumber);
    assertTrue(resetSerialNumberComponent.disabled);
    serialNumberComponent.value = serialNumber;
    await flushTasks();
    assertFalse(resetSerialNumberComponent.disabled);
    assertEquals(serialNumberComponent.value, serialNumber);
    resetSerialNumberComponent.click();
    await flushTasks();
    assertEquals(serialNumberComponent.value, fakeSerialNumber);
    assertTrue(resetSerialNumberComponent.disabled);
  });

  // TODO(gavindodd): Add tests for the selection lists when they are
  // reimplemented and bound.
  // The standard `select` object is not bound.
  // `iron-selector`, `iron-dropdown`, `cr-searchable-drop-down` and
  // `paper-dropdown-menu could not be made to work.
  //
  // test: ReimagingDeviceInformationPageModifyRegionAndReset
  // test: ReimagingDeviceInformationPageModifySkuAndReset
}
