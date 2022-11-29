// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {fakeDeviceRegions, fakeDeviceSkus, fakeDeviceWhiteLabels} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingDeviceInformationPage} from 'chrome://shimless-rma/reimaging_device_information_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const fakeSerialNumber = 'serial# 0001';
const fakeDramPartNumber = 'dram# 0123';

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
  component.onSelectedWhiteLabelChange_('ignored');
  component.onSelectedSkuChange_('ignored');
}

suite('reimagingDeviceInformationPageTest', function() {
  /** @type {?ReimagingDeviceInformationPage} */
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
  async function initializeReimagingDeviceInformationPage() {
    assertFalse(!!component);
    service.setGetOriginalSerialNumberResult(fakeSerialNumber);
    service.setGetRegionListResult(fakeDeviceRegions);
    service.setGetOriginalRegionResult(2);
    service.setGetWhiteLabelListResult(fakeDeviceWhiteLabels);
    service.setGetOriginalWhiteLabelResult(3);
    service.setGetSkuListResult(fakeDeviceSkus);
    service.setGetOriginalSkuResult(1);
    service.setGetOriginalDramPartNumberResult(fakeDramPartNumber);

    component = /** @type {!ReimagingDeviceInformationPage} */ (
        document.createElement('reimaging-device-information-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    // A flush tasks is required to wait for the drop lists to render and set
    // the initial selected index.
    await flushTasks();

    return flushTasks();
  }

  test('ReimagingDeviceInformationPageInitializes', async () => {
    await initializeReimagingDeviceInformationPage();

    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const whiteLabelSelectComponent =
        component.shadowRoot.querySelector('#whiteLabelSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    const resetSerialNumberComponent =
        component.shadowRoot.querySelector('#resetSerialNumber');
    const resetRegionComponent =
        component.shadowRoot.querySelector('#resetRegion');
    const resetWhiteLabelComponent =
        component.shadowRoot.querySelector('#resetWhiteLabel');
    const resetSkuComponent = component.shadowRoot.querySelector('#resetSku');

    assertEquals(fakeSerialNumber, serialNumberComponent.value);
    assertEquals(2, regionSelectComponent.selectedIndex);
    assertEquals(3, whiteLabelSelectComponent.selectedIndex);
    assertEquals(1, skuSelectComponent.selectedIndex);
    assertTrue(resetSerialNumberComponent.disabled);
    assertTrue(resetRegionComponent.disabled);
    assertTrue(resetWhiteLabelComponent.disabled);
    assertTrue(resetSkuComponent.disabled);
  });

  test('ReimagingDeviceInformationPageNextReturnsInformation', async () => {
    const resolver = new PromiseResolver();
    await initializeReimagingDeviceInformationPage();

    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const whiteLabelSelectComponent =
        component.shadowRoot.querySelector('#whiteLabelSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    const dramPartNumberComponent =
        component.shadowRoot.querySelector('#dramPartNumber');
    const expectedSerialNumber = 'expected serial number';
    const expectedRegionIndex = 0;
    const expectedWhiteLabelIndex = 1;
    const expectedSkuIndex = 2;
    const expectedDramPartNumber = 'expected dram part number';
    serialNumberComponent.value = expectedSerialNumber;
    regionSelectComponent.selectedIndex = expectedRegionIndex;
    whiteLabelSelectComponent.selectedIndex = expectedWhiteLabelIndex;
    skuSelectComponent.selectedIndex = expectedSkuIndex;
    dramPartNumberComponent.value = expectedDramPartNumber;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();
    let serialNumber;
    let regionIndex;
    let whiteLabelIndex;
    let skuIndex;
    let dramPartNumber;
    let callCounter = 0;
    service.setDeviceInformation =
        (resultSerialNumber, resultRegionIndex, resultSkuIndex,
         resultWhiteLabelIndex, resultDramPartNumber) => {
          callCounter++;
          serialNumber = resultSerialNumber;
          regionIndex = resultRegionIndex;
          whiteLabelIndex = resultWhiteLabelIndex;
          skuIndex = resultSkuIndex;
          dramPartNumber = resultDramPartNumber;
          return resolver.promise;
        };

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, callCounter);
    assertEquals(expectedSerialNumber, serialNumber);
    assertEquals(expectedRegionIndex, regionIndex);
    assertEquals(expectedWhiteLabelIndex, whiteLabelIndex);
    assertEquals(expectedSkuIndex, skuIndex);
    assertEquals(expectedDramPartNumber, dramPartNumber);
    assertDeepEquals(expectedResult, savedResult);
  });

  test('ReimagingDeviceInformationPageModifySerialNumberAndReset', async () => {
    await initializeReimagingDeviceInformationPage();

    component.allButtonsDisabled = false;
    const serialNumber = fakeSerialNumber + 'new serial number';
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

  test('ReimagingDeviceInformationPageInputsDisabled', async () => {
    await initializeReimagingDeviceInformationPage();

    const serialNumberInput =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelect = component.shadowRoot.querySelector('#regionSelect');
    const skuSelect = component.shadowRoot.querySelector('#skuSelect');
    const dramSelect = component.shadowRoot.querySelector('#dramPartNumber');
    const whiteLabelSelect =
        component.shadowRoot.querySelector('#whiteLabelSelect');

    component.allButtonsDisabled = false;
    assertFalse(serialNumberInput.disabled);
    assertFalse(regionSelect.disabled);
    assertFalse(skuSelect.disabled);
    assertFalse(dramSelect.disabled);
    assertFalse(whiteLabelSelect.disabled);

    component.allButtonsDisabled = true;
    assertTrue(serialNumberInput.disabled);
    assertTrue(regionSelect.disabled);
    assertTrue(skuSelect.disabled);
    assertTrue(dramSelect.disabled);
    assertTrue(whiteLabelSelect.disabled);
  });

  test(
      'ReimagingDeviceInformationPageModifyDramPartNumberAndReset',
      async () => {
        await initializeReimagingDeviceInformationPage();

        component.allButtonsDisabled = false;
        const dramPartNumber = fakeDramPartNumber + 'new part number';
        const dramPartNumberComponent =
            component.shadowRoot.querySelector('#dramPartNumber');
        const resetDramPartNumberComponent =
            component.shadowRoot.querySelector('#resetDramPartNumber');

        assertEquals(dramPartNumberComponent.value, fakeDramPartNumber);
        assertTrue(resetDramPartNumberComponent.disabled);
        dramPartNumberComponent.value = dramPartNumber;
        await flushTasks();
        assertFalse(resetDramPartNumberComponent.disabled);
        assertEquals(dramPartNumberComponent.value, dramPartNumber);
        resetDramPartNumberComponent.click();
        await flushTasks();
        assertEquals(dramPartNumberComponent.value, fakeDramPartNumber);
        assertTrue(resetDramPartNumberComponent.disabled);
      });

  test(
      'ReimagingDeviceInformationPageSerialNumberUpdatesNextDisable',
      async () => {
        const resolver = new PromiseResolver();
        await initializeReimagingDeviceInformationPage();
        let disableNextButtonEventFired = false;
        let disableNextButton = false;
        component.addEventListener('disable-next-button', (e) => {
          disableNextButtonEventFired = true;
          disableNextButton = e.detail;
        });

        const serialNumberComponent =
            component.shadowRoot.querySelector('#serialNumber');
        serialNumberComponent.value = '';
        await flushTasks();

        assertTrue(disableNextButtonEventFired);
        assertTrue(disableNextButton);

        disableNextButtonEventFired = false;
        serialNumberComponent.value = 'valid serial number';
        await flushTasks();

        assertTrue(disableNextButtonEventFired);
        assertFalse(disableNextButton);
      });

  test('ReimagingDeviceInformationPageRegionUpdatesNextDisable', async () => {
    const resolver = new PromiseResolver();
    await initializeReimagingDeviceInformationPage();
    let disableNextButtonEventFired = false;
    let disableNextButton = false;
    component.addEventListener('disable-next-button', (e) => {
      disableNextButtonEventFired = true;
      disableNextButton = e.detail;
    });

    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    regionSelectComponent.selectedIndex = -1;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertTrue(disableNextButton);

    disableNextButtonEventFired = false;
    regionSelectComponent.selectedIndex = 1;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertFalse(disableNextButton);
  });

  test('ReimagingDeviceInformationPageSkuUpdatesNextDisable', async () => {
    const resolver = new PromiseResolver();
    await initializeReimagingDeviceInformationPage();
    let disableNextButtonEventFired = false;
    let disableNextButton = false;
    component.addEventListener('disable-next-button', (e) => {
      disableNextButtonEventFired = true;
      disableNextButton = e.detail;
    });

    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    skuSelectComponent.selectedIndex = -1;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertTrue(disableNextButton);

    disableNextButtonEventFired = false;
    skuSelectComponent.selectedIndex = 1;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertFalse(disableNextButton);
  });

  test(
      'ReimagingDeviceInformationPageDramPartNumberDoesNotUpdateNextDisable',
      async () => {
        const resolver = new PromiseResolver();
        await initializeReimagingDeviceInformationPage();
        let disableNextButtonEventFired = false;
        let disableNextButton = false;
        component.addEventListener('disable-next-button', (e) => {
          disableNextButtonEventFired = true;
          disableNextButton = e.detail;
        });

        const dramPartNumberComponent =
            component.shadowRoot.querySelector('#dramPartNumber');
        dramPartNumberComponent.value = '';
        await flushTasks();

        assertFalse(disableNextButtonEventFired);

        disableNextButtonEventFired = false;
        dramPartNumberComponent.value = 'valid dram part number';
        await flushTasks();

        assertFalse(disableNextButtonEventFired);
      });

  // TODO(gavindodd): Add tests for the selection lists when they are
  // reimplemented and bound.
  // The standard `select` object is not bound.
  // `iron-selector`, `iron-dropdown`, `cr-searchable-drop-down` and
  // `paper-dropdown-menu could not be made to work.
  //
  // test: ReimagingDeviceInformationPageModifyRegionAndReset
  // test: ReimagingDeviceInformationPageModifySkuAndReset
});
