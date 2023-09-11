// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {fakeDeviceCustomLabels, fakeDeviceRegions, fakeDeviceSkus} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {BooleanOrDefaultOptions, ReimagingDeviceInformationPage} from 'chrome://shimless-rma/reimaging_device_information_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {FeatureLevel} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

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
  component.onSelectedCustomLabelChange_('ignored');
  component.onSelectedSkuChange_('ignored');
  component.onIsChassisBrandedChange_('ignored');
  component.onDoesMeetRequirementsChange_('ignored');
}

/**
 * This function is used to verify the results of calls to
 * ShimlessRMAService.setDeviceInformation.
 * @param {Object} fakeShimlessService the service that will
 *     have a mocked "setDeviceInformation".
 * @param {PromiseResolver} promiseResolver
 * @param {Object} result Empty object to store call results into.
 */
function setSetDeviceInformationForMockService(
    fakeShimlessService, promiseResolver, result) {
  result.callCounter = 0;
  fakeShimlessService.setDeviceInformation =
      (resultSerialNumber, resultRegionIndex, resultSkuIndex,
       resultCustomLabelIndex, resultDramPartNumber, resultIsChassisBranded,
       resultHwComplianceVersion) => {
        result.callCounter++;
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
  /** @type {?ReimagingDeviceInformationPage} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  function initializeReimagingDeviceInformationPage() {
    assertFalse(!!component);
    service.setGetOriginalSerialNumberResult(fakeSerialNumber);
    service.setGetRegionListResult(fakeDeviceRegions);
    service.setGetOriginalRegionResult(2);
    service.setGetCustomLabelListResult(fakeDeviceCustomLabels);
    service.setGetOriginalCustomLabelResult(3);
    service.setGetSkuListResult(fakeDeviceSkus);
    service.setGetOriginalSkuResult(1);
    service.setGetOriginalDramPartNumberResult(fakeDramPartNumber);
    service.setGetOriginalFeatureLevelResult(
        FeatureLevel.kRmadFeatureLevelUnsupported);
  }

  /** @return {!Promise} */
  async function initializeComponent() {
    component = /** @type {!ReimagingDeviceInformationPage} */ (
        document.createElement('reimaging-device-information-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    // A flush tasks is required to wait for the drop lists to render and set
    // the initial selected index.
    await flushTasks();

    return flushTasks();
  }

  test('PageInitializes', async () => {
    initializeReimagingDeviceInformationPage();
    await initializeComponent();
    await waitAfterNextRender(component);

    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const customLabelSelectComponent =
        component.shadowRoot.querySelector('#customLabelSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    const resetSerialNumberComponent =
        component.shadowRoot.querySelector('#resetSerialNumber');
    const resetRegionComponent =
        component.shadowRoot.querySelector('#resetRegion');
    const resetCustomLabelComponent =
        component.shadowRoot.querySelector('#resetCustomLabel');
    const resetSkuComponent = component.shadowRoot.querySelector('#resetSku');

    assertEquals(fakeSerialNumber, serialNumberComponent.value);
    assertEquals(2, regionSelectComponent.selectedIndex);
    assertEquals(3, customLabelSelectComponent.selectedIndex);
    assertEquals(1, skuSelectComponent.selectedIndex);
    assertTrue(resetSerialNumberComponent.disabled);
    assertTrue(resetRegionComponent.disabled);
    assertTrue(resetCustomLabelComponent.disabled);
    assertTrue(resetSkuComponent.disabled);
  });

  test('NextReturnsInformation', async () => {
    const resolver = new PromiseResolver();
    initializeReimagingDeviceInformationPage();
    await initializeComponent();

    const serialNumberComponent =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelectComponent =
        component.shadowRoot.querySelector('#regionSelect');
    const customLabelSelectComponent =
        component.shadowRoot.querySelector('#customLabelSelect');
    const skuSelectComponent = component.shadowRoot.querySelector('#skuSelect');
    const dramPartNumberComponent =
        component.shadowRoot.querySelector('#dramPartNumber');
    const expectedSerialNumber = 'expected serial number';
    const expectedRegionIndex = 0;
    const expectedCustomLabelIndex = 1;
    const expectedSkuIndex = 2;
    const expectedDramPartNumber = 'expected dram part number';
    serialNumberComponent.value = expectedSerialNumber;
    regionSelectComponent.selectedIndex = expectedRegionIndex;
    customLabelSelectComponent.selectedIndex = expectedCustomLabelIndex;
    skuSelectComponent.selectedIndex = expectedSkuIndex;
    dramPartNumberComponent.value = expectedDramPartNumber;
    // TODO(gavindodd) how to update selectedIndex and trigger on-change
    // automatically.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    const setDeviceInformationResults = {};
    setSetDeviceInformationForMockService(
        service, resolver, setDeviceInformationResults);

    const expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, setDeviceInformationResults.callCounter);
    assertEquals(
        expectedSerialNumber, setDeviceInformationResults.serialNumber);
    assertEquals(expectedRegionIndex, setDeviceInformationResults.regionIndex);
    assertEquals(
        expectedCustomLabelIndex, setDeviceInformationResults.customLabelIndex);
    assertEquals(expectedSkuIndex, setDeviceInformationResults.skuIndex);
    assertEquals(
        expectedDramPartNumber, setDeviceInformationResults.dramPartNumber);
    assertEquals(false, setDeviceInformationResults.isChassisBranded);
    assertEquals(0, setDeviceInformationResults.hwComplianceVersion);
    assertDeepEquals(expectedResult, savedResult);
  });

  test('ModifySerialNumberAndReset', async () => {
    initializeReimagingDeviceInformationPage();
    await initializeComponent();

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

  test('InputsDisabled', async () => {
    initializeReimagingDeviceInformationPage();
    await initializeComponent();

    const serialNumberInput =
        component.shadowRoot.querySelector('#serialNumber');
    const regionSelect = component.shadowRoot.querySelector('#regionSelect');
    const skuSelect = component.shadowRoot.querySelector('#skuSelect');
    const dramSelect = component.shadowRoot.querySelector('#dramPartNumber');
    const customLabelSelect =
        component.shadowRoot.querySelector('#customLabelSelect');

    component.allButtonsDisabled = false;
    assertFalse(serialNumberInput.disabled);
    assertFalse(regionSelect.disabled);
    assertFalse(skuSelect.disabled);
    assertFalse(dramSelect.disabled);
    assertFalse(customLabelSelect.disabled);

    component.allButtonsDisabled = true;
    assertTrue(serialNumberInput.disabled);
    assertTrue(regionSelect.disabled);
    assertTrue(skuSelect.disabled);
    assertTrue(dramSelect.disabled);
    assertTrue(customLabelSelect.disabled);
  });

  test(
      'ModifyDramPartNumberAndReset', async () => {
        initializeReimagingDeviceInformationPage();
        await initializeComponent();

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
      'SerialNumberUpdatesNextDisable', async () => {
        const resolver = new PromiseResolver();
        initializeReimagingDeviceInformationPage();
        await initializeComponent();
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

  test('RegionUpdatesNextDisable', async () => {
    const resolver = new PromiseResolver();
    initializeReimagingDeviceInformationPage();
    await initializeComponent();
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

  test('SkuUpdatesNextDisable', async () => {
    const resolver = new PromiseResolver();
    initializeReimagingDeviceInformationPage();
    await initializeComponent();
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

  test('CustomLabelUpdatesNextDisable', async () => {
    const resolver = new PromiseResolver();
    initializeReimagingDeviceInformationPage();
    await initializeComponent();
    let disableNextButtonEventFired = false;
    let disableNextButton = false;
    component.addEventListener('disable-next-button', (e) => {
      disableNextButtonEventFired = true;
      disableNextButton = e.detail;
    });

    const customLabelSelectComponent =
        component.shadowRoot.querySelector('#customLabelSelect');
    customLabelSelectComponent.selectedIndex = -1;
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertTrue(disableNextButton);

    disableNextButtonEventFired = false;
    customLabelSelectComponent.selectedIndex = 1;
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired);
    assertFalse(disableNextButton);
  });

  test(
      'DramPartNumberDoesNotUpdateNextDisable', async () => {
        const resolver = new PromiseResolver();
        initializeReimagingDeviceInformationPage();
        await initializeComponent();
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

  test(
      'NextButtonState_IsChassisBranded', async () => {
        // Set the compliance check flag so that the additional questions show
        // up.
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();

        // Set the feature level so that the additional questions show up.
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevelUnknown);

        await initializeComponent();

        const hwComplianceVersionElement =
            component.shadowRoot.querySelector('#doesMeetRequirements');
        const isChassisBrandedElement =
            component.shadowRoot.querySelector('#isChassisBranded');

        // Set the values of the compliance question properties so that the
        // next button is initially enabled.
        hwComplianceVersionElement.value = BooleanOrDefaultOptions.NO;
        isChassisBrandedElement.value = BooleanOrDefaultOptions.NO;
        suppressedComponentOnSelectedChange_(component);

        let disableNextButtonEventFired = false;
        let isNextButtonDisabled = false;
        component.addEventListener('disable-next-button', (e) => {
          disableNextButtonEventFired = true;
          isNextButtonDisabled = e.detail;
        });

        isChassisBrandedElement.value = BooleanOrDefaultOptions.YES;
        suppressedComponentOnSelectedChange_(component);
        await flushTasks();

        assertTrue(disableNextButtonEventFired, 'Event should have fired.');
        assertFalse(isNextButtonDisabled, 'Next button should be enabled.');

        // Reset the value of the tracking variable.
        disableNextButtonEventFired = false;

        // Reset the index back to its default value, which should disable
        // the next button.
        isChassisBrandedElement.value = BooleanOrDefaultOptions.DEFAULT;
        suppressedComponentOnSelectedChange_(component);
        await flushTasks();

        assertTrue(disableNextButtonEventFired, 'Event should have fired.');
        assertTrue(isNextButtonDisabled, 'Next button should be disabled.');
      });

  test(
      'NextButtonState_HwComplianceVersion', async () => {
        // Set the compliance check flag so that the additional questions show
        // up.
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();

        // Set the feature level so that the additional questions show up.
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevelUnknown);

        await initializeComponent();

        const hwComplianceVersionElement =
            component.shadowRoot.querySelector('#doesMeetRequirements');
        const isChassisBrandedElement =
            component.shadowRoot.querySelector('#isChassisBranded');

        // Set the values of the compliance question properties so that the
        // next button is initially enabled.
        hwComplianceVersionElement.value = BooleanOrDefaultOptions.NO;
        isChassisBrandedElement.value = BooleanOrDefaultOptions.NO;
        suppressedComponentOnSelectedChange_(component);

        let disableNextButtonEventFired = false;
        let isNextButtonDisabled = false;
        component.addEventListener('disable-next-button', (e) => {
          disableNextButtonEventFired = true;
          isNextButtonDisabled = e.detail;
        });

        // Manually update the property to trigger the observer.
        hwComplianceVersionElement.value = BooleanOrDefaultOptions.YES;
        suppressedComponentOnSelectedChange_(component);
        await flushTasks();

        assertTrue(disableNextButtonEventFired, 'Event should have fired.');
        assertFalse(isNextButtonDisabled, 'Next button should be enabled.');

        // Reset the value of the tracking variable.
        disableNextButtonEventFired = false;

        // Reset the index back to its default value, which should disable
        // the next button.
        hwComplianceVersionElement.value = BooleanOrDefaultOptions.DEFAULT;
        suppressedComponentOnSelectedChange_(component);
        await flushTasks();

        assertTrue(disableNextButtonEventFired, 'Event should have fired.');
        assertTrue(isNextButtonDisabled, 'Next button should be disabled.');
      });

  /**
   * This function tests that the given featureLevel doesn't affect the state of
   * the Next button.
   * @param {FeatureLevel} featureLevel The feature level to test. Any value
   *     besides kRmadFeatureLevelUnknown will work for this test, since all
   *     values (besides that one) hide the compliance questions.
   */
  async function expectComplianceQuestionsToNotAffectNextButtonState(
      featureLevel) {
    // Unknown FeatureLevel should not be passed into this function.
    assert(featureLevel !== FeatureLevel.kRmadFeatureLevelUnknown);

    // Set the compliance check flag so that the additional questions show
    // up if the feature level is unknown.
    loadTimeData.overrideValues({complianceCheckEnabled: true});

    initializeReimagingDeviceInformationPage();

    // Set the feature level based on parameter.
    service.setGetOriginalFeatureLevelResult(featureLevel);

    await initializeComponent();

    // We can't access the Next button directly, so setup an observer to save
    // the button's current state when it changes.
    let disableNextButtonEventFired = false;
    let isNextButtonDisabled = false;
    component.addEventListener('disable-next-button', (e) => {
      disableNextButtonEventFired = true;
      isNextButtonDisabled = e.detail;
    });

    // Manually update a property to trigger the observer and thus give us a way
    // to check the state of the Next button.
    component.shadowRoot.querySelector('#serialNumber').value = 'other value';
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertTrue(disableNextButtonEventFired, 'Event should have fired.');
    assertFalse(isNextButtonDisabled, 'Next button should be enabled.');
  }

  test(
      'NextButtonState_QuestionsNotShown_Unsupported', async () => {
        await expectComplianceQuestionsToNotAffectNextButtonState(
            FeatureLevel.kRmadFeatureLevelUnsupported);
      });

  test(
      'NextButtonState_QuestionsNotShown_FeatureLevel0', async () => {
        await expectComplianceQuestionsToNotAffectNextButtonState(
            FeatureLevel.kRmadFeatureLevel0);
      });

  test(
      'NextButtonState_QuestionsNotShown_FeatureLevel1', async () => {
        await expectComplianceQuestionsToNotAffectNextButtonState(
            FeatureLevel.kRmadFeatureLevel1);
      });

  test(
      'ResultsForComplianceCheckQuestions', async () => {
        // Set the compliance check flag so that the additional questions show
        // up.
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();

        // Set the feature level so that the additional questions show up.
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevelUnknown);

        await initializeComponent();

        const hwComplianceVersionElement =
            component.shadowRoot.querySelector('#doesMeetRequirements');
        const isChassisBrandedElement =
            component.shadowRoot.querySelector('#isChassisBranded');

        // Note that we purposefully don't test the default option for either
        // field, since that disables the Next button entirely.
        const scenarios = [
          {
            initialHwComplianceVersionValue: BooleanOrDefaultOptions.NO,
            expectedHwComplianceVersion: 0,
            initialIsChassisBrandedValue: BooleanOrDefaultOptions.NO,
            expectedIsChassisBranded: false,
          },
          {
            initialHwComplianceVersionValue: BooleanOrDefaultOptions.YES,
            expectedHwComplianceVersion: 1,
            initialIsChassisBrandedValue: BooleanOrDefaultOptions.YES,
            expectedIsChassisBranded: true,
          },
        ];

        for (const scenario of scenarios) {
          hwComplianceVersionElement.value =
              scenario.initialHwComplianceVersionValue;
          isChassisBrandedElement.value = scenario.initialIsChassisBrandedValue;
          suppressedComponentOnSelectedChange_(component);

          const setDeviceInformationResults = {};
          const resolver = new PromiseResolver();
          setSetDeviceInformationForMockService(
              service, resolver, setDeviceInformationResults);

          component.onNextButtonClick();
          resolver.resolve({});
          await flushTasks();
          assertEquals(
              scenario.expectedHwComplianceVersion,
              setDeviceInformationResults.hwComplianceVersion);
          assertEquals(
              scenario.expectedIsChassisBranded,
              setDeviceInformationResults.isChassisBranded);
        }
      });


  test('ComplianceCheckDisabled', async () => {
    loadTimeData.overrideValues({complianceCheckEnabled: false});

    initializeReimagingDeviceInformationPage();
    await initializeComponent();

    // Expect compliance-related fields to not be present when flag is off.
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#complianceWarning')));
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#isChassisBranded')));
    assertFalse(
        isVisible(component.shadowRoot.querySelector('#doesMeetRequirements')));

    // Next button should be enabled by default if the compliance check is
    // disabled.
    let isNextButtonDisabled = false;
    component.addEventListener('disable-next-button', (e) => {
      isNextButtonDisabled = e.detail;
    });

    // Trigger Next button update.
    suppressedComponentOnSelectedChange_(component);
    await flushTasks();

    assertFalse(isNextButtonDisabled, 'Next button should be enabled.');
  });

  test(
      'ComplianceCheckEnabled_Unsupported', async () => {
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevelUnsupported);
        await initializeComponent();

        // When the FeatureLevel is set to Unsupported, no compliance-related
        // fields should be shown.
        assertFalse(isVisible(
            component.shadowRoot.querySelector('#complianceWarning')));
        assertFalse(
            isVisible(component.shadowRoot.querySelector('#isChassisBranded')));
        assertFalse(isVisible(
            component.shadowRoot.querySelector('#doesMeetRequirements')));
      });

  test(
      'ComplianceCheckEnabled_Unknown', async () => {
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevelUnknown);
        await initializeComponent();

        // When the FeatureLevel is set to Unknown, the two compliance-related
        // questions should be shown.
        assertFalse(isVisible(
            component.shadowRoot.querySelector('#complianceWarning')));
        assertTrue(
            isVisible(component.shadowRoot.querySelector('#isChassisBranded')));
        assertTrue(isVisible(
            component.shadowRoot.querySelector('#doesMeetRequirements')));
      });

  test(
      'ComplianceCheckEnabled_Level0', async () => {
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevel0);
        await initializeComponent();

        // When the FeatureLevel is set to Level 0, the compliance warning
        // should be shown, and the string should indicate that the device is
        // not compliant.
        assertTrue(isVisible(
            component.shadowRoot.querySelector('#complianceWarning')));
        assertFalse(
            isVisible(component.shadowRoot.querySelector('#isChassisBranded')));
        assertFalse(isVisible(
            component.shadowRoot.querySelector('#doesMeetRequirements')));

        const complianceStatusString =
            component.shadowRoot.querySelector('.compliance-status-string');
        assertEquals(
            complianceStatusString.textContent.trim(),
            component.i18n('confirmDeviceInfoDeviceNotCompliant'));
      });

  test(
      'ComplianceCheckEnabled_Level1', async () => {
        loadTimeData.overrideValues({complianceCheckEnabled: true});

        initializeReimagingDeviceInformationPage();
        service.setGetOriginalFeatureLevelResult(
            FeatureLevel.kRmadFeatureLevel1);
        await initializeComponent();

        // When the FeatureLevel is set to Level 0, the compliance warning
        // should be shown, and the string should indicate that the device is
        // compliant.
        assertTrue(isVisible(
            component.shadowRoot.querySelector('#complianceWarning')));
        assertFalse(
            isVisible(component.shadowRoot.querySelector('#isChassisBranded')));
        assertFalse(isVisible(
            component.shadowRoot.querySelector('#doesMeetRequirements')));

        const complianceStatusString =
            component.shadowRoot.querySelector('.compliance-status-string');
        assertEquals(
            complianceStatusString.textContent.trim(),
            component.i18n('confirmDeviceInfoDeviceCompliant'));
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
