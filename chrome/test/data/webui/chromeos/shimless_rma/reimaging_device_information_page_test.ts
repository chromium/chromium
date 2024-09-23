// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shimless-rma/shimless_rma.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DISABLE_NEXT_BUTTON} from 'chrome://shimless-rma/events.js';
import {fakeDeviceCustomLabels, fakeDeviceRegions, fakeDeviceSkuDescriptions, fakeDeviceSkus} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {BooleanOrDefaultOptions, ReimagingDeviceInformationPage} from 'chrome://shimless-rma/reimaging_device_information_page.js';
import {FeatureLevel, StateResult} from 'chrome://shimless-rma/shimless_rma.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

interface SetDeviceInformationResult {
  serialNumber: string;
  regionIndex: number;
  customLabelIndex: number;
  skuIndex: number;
  dramPartNumber: string;
  isChassisBranded: boolean;
  hwComplianceVersion: number;
  callCounter: number;
}

const originalSerialNumber = 'serial# 0001';
const originalDramPartNumber = 'dram# 0123';
const originalRegionSelectedIndex = 0;
const originalCustomLabelSelectedIndex = 0;
const originalSkuSelectedIndex = 0;

const serialNumberSelector = '#serialNumber';
const regionSelectSelector = '#regionSelect';
const customLabelSelectSelector = '#customLabelSelect';
const skuSelectSelector = '#skuSelect';
const dramPartNumberSelector = '#dramPartNumber';

const resetSerialNumberSelector = '#resetSerialNumber';
const resetRegionSelector = '#resetRegion';
const resetCustomLabelSelector = '#resetCustomLabel';
const resetSkuSelector = '#resetSku';
const resetDramPartNumberSelector = '#resetDramPartNumber';

const complianceWarningSelector = '#complianceWarning';
const meetRequirementsSelector = '#doesMeetRequirements';
const chassisBrandedSelector = '#isChassisBranded';
const complianceStatusStringSelector = '.compliance-status-string';

function createDefaultDeviceInformationResult(): SetDeviceInformationResult {
  return {
    serialNumber: '',
    regionIndex: -1,
    customLabelIndex: -1,
    skuIndex: -1,
    dramPartNumber: '',
    isChassisBranded: false,
    hwComplianceVersion: -1,
    callCounter: -1,
  };
}

// This function returns the results of calls to `setDeviceInformation()`.
function setDeviceInfoResponse(
    fakeShimlessService: FakeShimlessRmaService,
    promiseResolver: PromiseResolver<{stateResult: StateResult}>,
    result: SetDeviceInformationResult): void {
  result.callCounter = 0;
  fakeShimlessService.setDeviceInformation =
      (resultSerialNumber, resultRegionIndex, resultSkuIndex,
       resultCustomLabelIndex, resultDramPartNumber, resultIsChassisBranded,
       resultHwComplianceVersion) => {
        ++result.callCounter;
        result.serialNumber = resultSerialNumber;
        result.regionIndex = resultRegionIndex;
        result.customLabelIndex = resultCustomLabelIndex;
        result.skuIndex = resultSkuIndex;
        result.dramPartNumber = resultDramPartNumber;
        result.isChassisBranded = resultIsChassisBranded;
        result.hwComplianceVersion = resultHwComplianceVersion;
        return promiseResolver.promise;
      };
}

suite('reimagingDeviceInformationPageTest', function() {
  let component: ReimagingDeviceInformationPage|null = null;

  const service: FakeShimlessRmaService = new FakeShimlessRmaService();

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component?.remove();
    component = null;
  });

  async function initializeReimagingDeviceInformationPage(): Promise<void> {
    service.setGetOriginalSerialNumberResult(originalSerialNumber);
    service.setGetRegionListResult(fakeDeviceRegions);
    service.setGetOriginalRegionResult(originalRegionSelectedIndex);
    service.setGetCustomLabelListResult(fakeDeviceCustomLabels);
    service.setGetOriginalCustomLabelResult(originalCustomLabelSelectedIndex);
    service.setGetSkuListResult(fakeDeviceSkus);
    service.setGetSkuDescriptionListResult(fakeDeviceSkuDescriptions);
    service.setGetOriginalSkuResult(originalSkuSelectedIndex);
    service.setGetOriginalDramPartNumberResult(originalDramPartNumber);
    service.setGetOriginalFeatureLevelResult(
        FeatureLevel.kRmadFeatureLevelUnsupported);

    assert(!component);
    component = document.createElement(ReimagingDeviceInformationPage.is);
    assert(component);
    component.allButtonsDisabled = false;
    document.body.appendChild(component);

    // `waitAfterNextRender()` required to let the dropdowns render and set
    // their initial selected value.
    await waitAfterNextRender(component);
    return flushTasks();
  }

  async function setFeatureLevelAndReinitialize(featureLevel: FeatureLevel):
      Promise<void> {
    service.setGetOriginalFeatureLevelResult(featureLevel);

    component?.remove();
    component = null;
    component = document.createElement(ReimagingDeviceInformationPage.is);
    assert(component);
    component.allButtonsDisabled = false;
    document.body.appendChild(component);

    // This is required to wait for the dropdowns to render and set their
    // initial selected index.
    await waitAfterNextRender(component);
    return flushTasks();
  }

  // Verify the page initializes with the expected components.
  test('PageInitializes', async () => {
    await initializeReimagingDeviceInformationPage();

    assert(component);
    assertEquals(
        originalSerialNumber,
        strictQuery(serialNumberSelector, component.shadowRoot, CrInputElement)
            .value);
    assertEquals(
        originalRegionSelectedIndex,
        strictQuery(
            regionSelectSelector, component.shadowRoot, HTMLSelectElement)
            .selectedIndex);
    assertEquals(
        originalCustomLabelSelectedIndex,
        strictQuery(
            customLabelSelectSelector, component.shadowRoot, HTMLSelectElement)
            .selectedIndex);
    assertEquals(
        originalSkuSelectedIndex,
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement)
            .selectedIndex);
    assertEquals(
        originalDramPartNumber,
        strictQuery(
            dramPartNumberSelector, component.shadowRoot, CrInputElement)
            .value);
    assertEquals(
        `${fakeDeviceSkus[0]}: ${fakeDeviceSkuDescriptions[0]}`,
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement)
            .value);
    assertTrue(
        strictQuery(
            resetSerialNumberSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(resetRegionSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(
            resetCustomLabelSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(resetSkuSelector, component.shadowRoot, CrButtonElement)
            .disabled);
    assertTrue(
        strictQuery(
            resetDramPartNumberSelector, component.shadowRoot, CrButtonElement)
            .disabled);
  });

  // Verify clicking the next button sends the expected values from the inputs.
  test('NextButtonReturnsInformation', async () => {
    await initializeReimagingDeviceInformationPage();


    // Set new values for all the inputs.
    assert(component);
    const newSerialNumber = 'expected serial number';
    strictQuery(serialNumberSelector, component.shadowRoot, CrInputElement)
        .value = newSerialNumber;

    const newRegionIndex = 1;
    const regionSelect = strictQuery(
        regionSelectSelector, component.shadowRoot, HTMLSelectElement);
    regionSelect.selectedIndex = newRegionIndex;
    regionSelect.dispatchEvent(new CustomEvent('change'));

    const newCustomLabelIndex = 1;
    const customLabelSelect = strictQuery(
        customLabelSelectSelector, component.shadowRoot, HTMLSelectElement);
    customLabelSelect.selectedIndex = newCustomLabelIndex;
    customLabelSelect.dispatchEvent(new CustomEvent('change'));

    const newSkuIndex = 2;
    const skuSelect =
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement);
    skuSelect.selectedIndex = newSkuIndex;
    skuSelect.dispatchEvent(new CustomEvent('change'));

    const newDramPartNumber = 'expected dram part number';
    strictQuery(dramPartNumberSelector, component.shadowRoot, CrInputElement)
        .value = newDramPartNumber;

    const setDeviceInformationResults = createDefaultDeviceInformationResult();
    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    setDeviceInfoResponse(service, resolver, setDeviceInformationResults);

    component.onNextButtonClick();
    await flushTasks();

    // Verify the new values were sent.
    assertEquals(1, setDeviceInformationResults.callCounter);
    assertEquals(newSerialNumber, setDeviceInformationResults.serialNumber);
    assertEquals(newRegionIndex, setDeviceInformationResults.regionIndex);
    assertEquals(
        newCustomLabelIndex, setDeviceInformationResults.customLabelIndex);
    assertEquals(newSkuIndex, setDeviceInformationResults.skuIndex);
    assertEquals(newDramPartNumber, setDeviceInformationResults.dramPartNumber);
    assertEquals(false, setDeviceInformationResults.isChassisBranded);
    assertEquals(0, setDeviceInformationResults.hwComplianceVersion);
  });

  // Verify clicking the reset buttons set the inputs to their original values.
  test('ModifyValuesAndClickReset', async () => {
    await initializeReimagingDeviceInformationPage();

    assert(component);
    const resetSerialNumberButton = strictQuery(
        resetSerialNumberSelector, component.shadowRoot, CrButtonElement);
    const resetRegionButton =
        strictQuery(resetRegionSelector, component.shadowRoot, CrButtonElement);
    const resetCustomLabelButton = strictQuery(
        resetCustomLabelSelector, component.shadowRoot, CrButtonElement);
    const resetSkuButton =
        strictQuery(resetSkuSelector, component.shadowRoot, CrButtonElement);
    const resetDramPartNumberButton = strictQuery(
        resetDramPartNumberSelector, component.shadowRoot, CrButtonElement);

    assertTrue(resetSerialNumberButton.disabled);
    assertTrue(resetRegionButton.disabled);
    assertTrue(resetCustomLabelButton.disabled);
    assertTrue(resetSkuButton.disabled);
    assertTrue(resetDramPartNumberButton.disabled);

    // Update all input values and expect all the reset buttons to enable.
    const serialNumberSelect =
        strictQuery(serialNumberSelector, component.shadowRoot, CrInputElement);
    serialNumberSelect.value = 'temp serial number';

    const regionSelect = strictQuery(
        regionSelectSelector, component.shadowRoot, HTMLSelectElement);
    regionSelect.selectedIndex = originalRegionSelectedIndex + 1;
    regionSelect.dispatchEvent(new CustomEvent('change'));

    const customLabelSelect = strictQuery(
        customLabelSelectSelector, component.shadowRoot, HTMLSelectElement);
    customLabelSelect.selectedIndex = originalCustomLabelSelectedIndex + 1;
    customLabelSelect.dispatchEvent(new CustomEvent('change'));

    const skuSelect =
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement);
    skuSelect.selectedIndex = originalSkuSelectedIndex + 1;
    skuSelect.dispatchEvent(new CustomEvent('change'));

    const dramPartNumberSelect = strictQuery(
        dramPartNumberSelector, component.shadowRoot, CrInputElement);
    dramPartNumberSelect.value = 'temp dram number';

    // Click all the reset buttons and expect the inputs to revert back to their
    // original values.
    assertFalse(resetSerialNumberButton.disabled);
    assertFalse(resetRegionButton.disabled);
    assertFalse(resetCustomLabelButton.disabled);
    assertFalse(resetSkuButton.disabled);
    assertFalse(resetDramPartNumberButton.disabled);
    resetSerialNumberButton.click();
    resetRegionButton.click();
    resetCustomLabelButton.click();
    resetSkuButton.click();
    resetDramPartNumberButton.click();

    // All the inputs should be back to their original values.
    assertEquals(originalSerialNumber, serialNumberSelect.value);
    assertEquals(originalRegionSelectedIndex, regionSelect.selectedIndex);
    assertEquals(
        originalCustomLabelSelectedIndex, customLabelSelect.selectedIndex);
    assertEquals(originalSkuSelectedIndex, skuSelect.selectedIndex);
    assertEquals(originalDramPartNumber, dramPartNumberSelect.value);
  });

  // Verify when `allButtonsDisabled` is set all inputs are disabled.
  test('InputsDisabled', async () => {
    await initializeReimagingDeviceInformationPage();

    assert(component);
    const serialNumberSelect =
        strictQuery(serialNumberSelector, component.shadowRoot, CrInputElement);
    const regionSelect = strictQuery(
        regionSelectSelector, component.shadowRoot, HTMLSelectElement);
    const customLabelSelect = strictQuery(
        customLabelSelectSelector, component.shadowRoot, HTMLSelectElement);
    const skuSelect =
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement);
    const dramPartNumberSelect = strictQuery(
        dramPartNumberSelector, component.shadowRoot, CrInputElement);

    assertFalse(serialNumberSelect.disabled);
    assertFalse(regionSelect.disabled);
    assertFalse(customLabelSelect.disabled);
    assertFalse(skuSelect.disabled);
    assertFalse(dramPartNumberSelect.disabled);

    component.allButtonsDisabled = true;
    assertTrue(serialNumberSelect.disabled);
    assertTrue(regionSelect.disabled);
    assertTrue(customLabelSelect.disabled);
    assertTrue(skuSelect.disabled);
    assertTrue(dramPartNumberSelect.disabled);
  });

  // Verify the next button gets disabled when the inputs has invalid values.
  test('InvalidValueDisablesTheNextButton', async () => {
    await initializeReimagingDeviceInformationPage();

    // Set the serial number to blank, ensure the next button is disabled,
    // then replace the value and see the next button enabled.
    assert(component);
    let disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    const serialNumberSelect =
        strictQuery(serialNumberSelector, component.shadowRoot, CrInputElement);
    serialNumberSelect.value = '';
    let disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    let enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    serialNumberSelect.value = 'value';
    let enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);

    // Set the region to an invalid option, ensure the next button is
    // disabled, then replace the value and see the next button enabled.
    disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    const regionSelect = strictQuery(
        regionSelectSelector, component.shadowRoot, HTMLSelectElement);
    regionSelect.selectedIndex = -1;
    regionSelect.dispatchEvent(new CustomEvent('change'));
    disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    regionSelect.selectedIndex = originalRegionSelectedIndex;
    regionSelect.dispatchEvent(new CustomEvent('change'));
    enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);

    // Set the custom label to an invalid option, ensure the next button is
    // disabled, then replace the value and see the next button enabled.
    disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    const customLabelSelect = strictQuery(
        customLabelSelectSelector, component.shadowRoot, HTMLSelectElement);
    customLabelSelect.selectedIndex = -1;
    customLabelSelect.dispatchEvent(new CustomEvent('change'));
    disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    customLabelSelect.selectedIndex = originalCustomLabelSelectedIndex;
    customLabelSelect.dispatchEvent(new CustomEvent('change'));
    enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);

    // Set the sku to an invalid option, ensure the next button is disabled,
    // then replace the value and see the next button enabled.
    disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    const skuSelect =
        strictQuery(skuSelectSelector, component.shadowRoot, HTMLSelectElement);
    skuSelect.selectedIndex = -1;
    skuSelect.dispatchEvent(new CustomEvent('change'));
    disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    skuSelect.selectedIndex = originalSkuSelectedIndex;
    skuSelect.dispatchEvent(new CustomEvent('change'));
    enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);
  });

  // Verify the next button gets disabled when the compliance questions are set
  // to their default options.
  test('NextButtonDisabledDefaultCompliance', async () => {
    await initializeReimagingDeviceInformationPage();
    // Set the feature level so that the additional questions show up.
    await setFeatureLevelAndReinitialize(FeatureLevel.kRmadFeatureLevelUnknown);

    assert(component);
    const hwComplianceVersionElement = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLSelectElement);
    const isChassisBrandedElement = strictQuery(
        chassisBrandedSelector, component.shadowRoot, HTMLSelectElement);

    // Set the values of the compliance question properties so that the
    // next button is initially enabled.
    hwComplianceVersionElement.value = BooleanOrDefaultOptions.NO;
    hwComplianceVersionElement.dispatchEvent(new CustomEvent('change'));
    isChassisBrandedElement.value = BooleanOrDefaultOptions.NO;
    isChassisBrandedElement.dispatchEvent(new CustomEvent('change'));

    // Set the compliance version to an invalid option, ensure the next
    // button is disabled, then set to valid value and see the next button
    // enabled.
    let disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    hwComplianceVersionElement.value = BooleanOrDefaultOptions.DEFAULT;
    hwComplianceVersionElement.dispatchEvent(new CustomEvent('change'));
    let disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    let enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    hwComplianceVersionElement.value = BooleanOrDefaultOptions.YES;
    hwComplianceVersionElement.dispatchEvent(new CustomEvent('change'));
    let enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);

    // Set the chassis branded version to an invalid option, ensure the next
    // button is disabled, then set to valid value and see the next button
    // enabled.
    disableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    isChassisBrandedElement.value = BooleanOrDefaultOptions.DEFAULT;
    isChassisBrandedElement.dispatchEvent(new CustomEvent('change'));
    disableEventResponse = await disableNextButtonEvent;
    assertTrue(disableEventResponse.detail);

    enableNextButtonEvent = eventToPromise(DISABLE_NEXT_BUTTON, component);
    isChassisBrandedElement.value = BooleanOrDefaultOptions.YES;
    isChassisBrandedElement.dispatchEvent(new CustomEvent('change'));
    enableEventResponse = await enableNextButtonEvent;
    assertFalse(enableEventResponse.detail);
  });

  // Verify the correct info is sent based on the current compliance questions
  // values.
  test('ResultsForComplianceCheckQuestions', async () => {
    await initializeReimagingDeviceInformationPage();
    // Set the feature level so that the additional questions show up.
    await setFeatureLevelAndReinitialize(FeatureLevel.kRmadFeatureLevelUnknown);

    assert(component);
    const hwComplianceVersionElement = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLSelectElement);
    const isChassisBrandedElement = strictQuery(
        chassisBrandedSelector, component.shadowRoot, HTMLSelectElement);

    // Set both compliance question answers to NO.
    hwComplianceVersionElement.value = BooleanOrDefaultOptions.NO;
    hwComplianceVersionElement.dispatchEvent(new CustomEvent('change'));
    isChassisBrandedElement.value = BooleanOrDefaultOptions.NO;
    isChassisBrandedElement.dispatchEvent(new CustomEvent('change'));

    const resolver = new PromiseResolver<{stateResult: StateResult}>();
    let setDeviceInformationResults = createDefaultDeviceInformationResult();
    setDeviceInfoResponse(service, resolver, setDeviceInformationResults);

    // Click the next button and verify the returned results.
    component.onNextButtonClick();
    await flushTasks();
    let expectedHwComplianceVersion = 0;
    assertEquals(
        expectedHwComplianceVersion,
        setDeviceInformationResults.hwComplianceVersion);
    assertFalse(setDeviceInformationResults.isChassisBranded);

    // Set both compliance question answers to YES.
    hwComplianceVersionElement.value = BooleanOrDefaultOptions.YES;
    hwComplianceVersionElement.dispatchEvent(new CustomEvent('change'));
    isChassisBrandedElement.value = BooleanOrDefaultOptions.YES;
    isChassisBrandedElement.dispatchEvent(new CustomEvent('change'));

    setDeviceInformationResults = createDefaultDeviceInformationResult();
    setDeviceInfoResponse(service, resolver, setDeviceInformationResults);

    // Click the next button and verify the updated results.
    component.onNextButtonClick();
    await flushTasks();
    expectedHwComplianceVersion = 1;
    assertEquals(
        expectedHwComplianceVersion,
        setDeviceInformationResults.hwComplianceVersion);
    assertTrue(setDeviceInformationResults.isChassisBranded);
  });

  // Verify the correct warnings and text are displayed based on the current
  // feature level.
  test('WarningsByFeatureLevel', async () => {
    await initializeReimagingDeviceInformationPage();
    // Set the feature level so no compliance info shows.
    await setFeatureLevelAndReinitialize(
        FeatureLevel.kRmadFeatureLevelUnsupported);

    assert(component);
    let complianceWarning = strictQuery(
        complianceWarningSelector, component.shadowRoot, HTMLElement);
    let meetRequirements = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLElement);
    let chassisBranded =
        strictQuery(chassisBrandedSelector, component.shadowRoot, HTMLElement);

    // When the FeatureLevel is set to Unsupported, no compliance-related
    // fields should be shown.
    assertFalse(isVisible(complianceWarning));
    assertFalse(isVisible(meetRequirements));
    assertFalse(isVisible(chassisBranded));

    await setFeatureLevelAndReinitialize(FeatureLevel.kRmadFeatureLevelUnknown);
    complianceWarning = strictQuery(
        complianceWarningSelector, component.shadowRoot, HTMLElement);
    meetRequirements = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLElement);
    chassisBranded =
        strictQuery(chassisBrandedSelector, component.shadowRoot, HTMLElement);
    assertFalse(isVisible(complianceWarning));
    assertTrue(isVisible(meetRequirements));
    assertTrue(isVisible(chassisBranded));

    await setFeatureLevelAndReinitialize(FeatureLevel.kRmadFeatureLevel0);
    complianceWarning = strictQuery(
        complianceWarningSelector, component.shadowRoot, HTMLElement);
    meetRequirements = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLElement);
    chassisBranded =
        strictQuery(chassisBrandedSelector, component.shadowRoot, HTMLElement);
    assertTrue(isVisible(complianceWarning));
    assertFalse(isVisible(meetRequirements));
    assertFalse(isVisible(chassisBranded));
    assertEquals(
        loadTimeData.getString('confirmDeviceInfoDeviceNotCompliant'),
        strictQuery(
            complianceStatusStringSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());

    await setFeatureLevelAndReinitialize(FeatureLevel.kRmadFeatureLevel1);
    complianceWarning = strictQuery(
        complianceWarningSelector, component.shadowRoot, HTMLElement);
    meetRequirements = strictQuery(
        meetRequirementsSelector, component.shadowRoot, HTMLElement);
    chassisBranded =
        strictQuery(chassisBrandedSelector, component.shadowRoot, HTMLElement);
    assertTrue(isVisible(complianceWarning));
    assertFalse(isVisible(meetRequirements));
    assertFalse(isVisible(chassisBranded));
    assertEquals(
        loadTimeData.getString('confirmDeviceInfoDeviceCompliant'),
        strictQuery(
            complianceStatusStringSelector, component.shadowRoot, HTMLElement)
            .textContent!.trim());
  });
});
