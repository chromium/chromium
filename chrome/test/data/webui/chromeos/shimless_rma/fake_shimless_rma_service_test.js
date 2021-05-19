// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {CalibrationComponent, CalibrationObserver, ComponentRepairState, ComponentType, ErrorObserver, HardwareWriteProtectionStateObserver, PowerCableStateObserver, ProvisioningObserver, ProvisioningStep, RmadErrorCode, RmaState, ShimlessRmaServiceInterface} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertGE, assertLE} from '../../chai_assert.js';

export function fakeShimlessRmaServiceTestSuite() {
  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    service = new FakeShimlessRmaService();
  });

  teardown(() => {
    service = null;
  });


  test('GetCurrentStateDefaultRmaNotRequired', () => {
    return service.getCurrentState().then((state) => {
      assertEquals(state.currentState, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetCurrentStateWelcomeOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.currentState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetCurrentStateWelcomeError', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kMissingComponent},
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.currentState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kMissingComponent);
    });
  });

  test('GetNextStateDefaultRmaNotRequired', () => {
    return service.getNextState().then((state) => {
      assertEquals(state.nextState, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetNextStateConfigureNetworkOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.getNextState().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetNextStateConfigureNetworkTransitionFailed', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.getNextState().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kTransitionFailed);
    });
  });

  test('GetPrevStateDefaultRmaNotRequired', () => {
    return service.getPrevState().then((state) => {
      assertEquals(state.prevState, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetPrevStateWelcomeOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    service.getNextState().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
    return service.getPrevState().then((state) => {
      assertEquals(state.prevState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetPrevStateWelcomeTransitionFailed', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.getPrevState().then((state) => {
      assertEquals(state.prevState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kTransitionFailed);
    });
  });

  test('AbortRmaDefaultUndefined', () => {
    return service.abortRma().then((error) => {
      assertEquals(error, undefined);
    });
  });

  test('SetAbortRmaResultUpdatesResult', () => {
    service.setAbortRmaResult(RmadErrorCode.kRequestInvalid);
    return service.abortRma().then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetCurrentChromeVersionDefaultUndefined', () => {
    return service.getCurrentChromeVersion().then((version) => {
      assertEquals(version, undefined);
    });
  });

  test('SetGetCurrentChromeVersionResultUpdatesResult', () => {
    service.setGetCurrentChromeVersionResult('1234.56.78');
    return service.getCurrentChromeVersion().then((version) => {
      assertEquals(version.version, '1234.56.78');
    });
  });

  test('UpdateChromeDefaultUndefined', () => {
    return service.updateChrome().then((error) => {
      assertEquals(error, undefined);
    });
  });

  test('SetUpdateChromeResultUpdatesResult', () => {
    service.setUpdateChromeResult(RmadErrorCode.kRequestInvalid);
    return service.updateChrome().then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetSameOwnerOk', () => {
    let states = [
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetSameOwnerWhenRmaNotRequired', () => {
    return service.setSameOwner().then((state) => {
      assertEquals(state.nextState, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('SetSameOwnerWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetDifferentOwnerOk', () => {
    let states = [
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetDifferentOwnerWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ManualDisableWriteProtectAvailableDefaultUndefined', () => {
    return service.manualDisableWriteProtectAvailable().then((available) => {
      assertEquals(available, undefined);
    });
  });

  test('SetManualDisableWriteProtectAvailableResultUpdatesResult', () => {
    service.setManualDisableWriteProtectAvailableResult(true);
    return service.manualDisableWriteProtectAvailable().then((available) => {
      assertEquals(available.available, true);
    });
  });

  test('ManuallyDisableWriteProtectOk', () => {
    let states = [
      {
        state: RmaState.kChooseWriteProtectDisableMethod,
        error: RmadErrorCode.kOk
      },
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.manuallyDisableWriteProtect().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ManuallyDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.manuallyDisableWriteProtect().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('RsuDisableWriteProtectOk', () => {
    let states = [
      {
        state: RmaState.kChooseWriteProtectDisableMethod,
        error: RmadErrorCode.kOk
      },
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.rsuDisableWriteProtect('ignored').then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('RsuDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.rsuDisableWriteProtect('ignored').then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetComponentListDefaultEmpty', () => {
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, []);
    });
  });

  test('SetGetComponentListResultUpdatesResult', () => {
    let expected_components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairState.kOriginal
      },
      {
        component: ComponentType.kTrackpad,
        state: ComponentRepairState.kMissing
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, expected_components);
    });
  });

  test('ToggleComponentReplacedOriginalBecomesReplaced', () => {
    let expected_components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairState.kOriginal
      },
      {
        component: ComponentType.kTrackpad,
        state: ComponentRepairState.kMissing
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.toggleComponentReplaced(ComponentType.kKeyboard)
        .then((state) => {
          assertEquals(state.repairState, ComponentRepairState.kReplaced);
          // Confirm components list updated.
          assertEquals(state.repairState, expected_components[0].state);
        });
  });

  test('ToggleComponentReplacedReplacedBecomesOriginal', () => {
    let expected_components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairState.kOriginal
      },
      {
        component: ComponentType.kTrackpad,
        state: ComponentRepairState.kMissing
      },
      {
        component: ComponentType.kPowerButton,
        state: ComponentRepairState.kReplaced
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.toggleComponentReplaced(ComponentType.kPowerButton)
        .then((state) => {
          assertEquals(state.repairState, ComponentRepairState.kOriginal);
          // Confirm components list updated.
          assertEquals(state.repairState, expected_components[2].state);
        });
  });

  test('ToggleComponentReplacedMissingUnchanged', () => {
    let expected_components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairState.kOriginal
      },
      {
        component: ComponentType.kTrackpad,
        state: ComponentRepairState.kMissing
      },
      {
        component: ComponentType.kPowerButton,
        state: ComponentRepairState.kOriginal
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.toggleComponentReplaced(ComponentType.kTrackpad)
        .then((state) => {
          assertEquals(state.repairState, ComponentRepairState.kMissing);
        });
  });

  test('ReimageSkippedOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageSkipped().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageSkippedWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageSkipped().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReimageFromDownloadOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromDownload().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageFromDownloadWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromDownload().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReimageFromUsbOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromUsb().then((state) => {
      assertEquals(state.nextState, RmaState.kUpdateChrome);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageFromUsbWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateChrome, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromUsb().then((state) => {
      assertEquals(state.nextState, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetRegionListDefaultUndefined', () => {
    return service.getRegionList().then((regions) => {
      assertEquals(regions, undefined);
    });
  });

  test('SetGetRegionListResultUpdatesResult', () => {
    let regionList = ['America', 'Asia', 'Europe'];
    service.setGetRegionListResult(regionList);
    return service.getRegionList().then((regions) => {
      assertDeepEquals(regions.regions, regionList);
    });
  });

  test('GetSkuListDefaultUndefined', () => {
    return service.getSkuList().then((skus) => {
      assertEquals(skus, undefined);
    });
  });

  test('SetGetSkuListResultUpdatesResult', () => {
    let skuList = ['skuA', 'skuB', 'skuC'];
    service.setGetSkuListResult(skuList);
    return service.getSkuList().then((skus) => {
      assertDeepEquals(skus.skus, skuList);
    });
  });

  test('GetOriginalSerialNumberDefaultUndefined', () => {
    return service.getOriginalSerialNumber().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetOriginalSerialNumberResultUpdatesResult', () => {
    let expected_serial_number = '123456789';
    service.setGetOriginalSerialNumberResult(expected_serial_number);
    return service.getOriginalSerialNumber().then((serial_number) => {
      assertEquals(serial_number.serialNumber, expected_serial_number);
    });
  });

  test('GetSerialNumberDefaultEmpty', () => {
    return service.getSerialNumber().then((serialNumber) => {
      assertEquals(serialNumber.serialNumber, '');
    });
  });

  test('SetGetOriginalSerialNumberResultUpdatesGetSerialNumberResult', () => {
    let expected_serial_number = '123456789';
    service.setGetOriginalSerialNumberResult(expected_serial_number);
    return service.getSerialNumber().then((serial_number) => {
      assertEquals(serial_number.serialNumber, expected_serial_number);
    });
  });

  test('SetSerialNumberDefaultOk', () => {
    return service.setSerialNumber('123456789').then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
  });

  test('SetSerialNumberOkUpdatesGetSerialNumberResult', () => {
    let original_serial_number = '123456789';
    let expected_serial_number = '987654321';
    service.setGetOriginalSerialNumberResult(original_serial_number);
    service.setSetSerialNumberResult(RmadErrorCode.kOk);
    service.setSerialNumber(expected_serial_number).then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
    return service.getSerialNumber().then((serial_number) => {
      assertEquals(serial_number.serialNumber, expected_serial_number);
    });
  });

  test('SetSerialNumberErrorDoesNotUpdateGetSerialNumberResult', () => {
    let original_serial_number = '123456789';
    let new_serial_number = '987654321';
    service.setGetOriginalSerialNumberResult(original_serial_number);
    service.setSetSerialNumberResult(RmadErrorCode.kRequestInvalid);
    service.setSerialNumber(new_serial_number).then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
    return service.getSerialNumber().then((serial_number) => {
      assertEquals(serial_number.serialNumber, original_serial_number);
    });
  });

  test('GetOriginalRegionDefaultUndefined', () => {
    return service.getOriginalRegion().then((region) => {
      assertEquals(region, undefined);
    });
  });

  test('SetGetOriginalRegionResultUpdatesResult', () => {
    let expected_region = 1;
    service.setGetOriginalRegionResult(expected_region);
    return service.getOriginalRegion().then((region) => {
      assertEquals(region.regionIndex, expected_region);
    });
  });

  test('GetRegionDefaultZero', () => {
    return service.getRegion().then((region) => {
      assertEquals(region.regionIndex, 0);
    });
  });

  test('SetGetOriginalRegionResultUpdatesGetRegionResult', () => {
    let expected_region = 1;
    service.setGetOriginalRegionResult(expected_region);
    return service.getRegion().then((region) => {
      assertEquals(region.regionIndex, expected_region);
    });
  });

  test('SetRegionDefaultOk', () => {
    return service.setRegion(1).then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
  });

  test('SetRegionOkUpdatesGetRegionResult', () => {
    let original_region = 1;
    let expected_region = 2;
    service.setGetOriginalRegionResult(original_region);
    service.setSetRegionResult(RmadErrorCode.kOk);
    service.setRegion(expected_region).then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
    return service.getRegion().then((region) => {
      assertEquals(region.regionIndex, expected_region);
    });
  });

  test('SetRegionErrorDoesNotUpdateGetRegionResult', () => {
    let original_region = 1;
    let expected_region = 2;
    service.setGetOriginalRegionResult(original_region);
    service.setSetRegionResult(RmadErrorCode.kRequestInvalid);
    service.setRegion(expected_region).then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
    return service.getRegion().then((region) => {
      assertEquals(region.regionIndex, original_region);
    });
  });

  test('GetOriginalSkuDefaultUndefined', () => {
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku, undefined);
    });
  });

  test('SetGetOriginalSkuResultUpdatesResult', () => {
    let expected_sku = 1;
    service.setGetOriginalSkuResult(expected_sku);
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku.skuIndex, expected_sku);
    });
  });

  test('GetSkuDefaultZero', () => {
    return service.getSku().then((sku) => {
      assertEquals(sku.skuIndex, 0);
    });
  });

  test('SetGetOriginalSkuResultUpdatesGetSkuResult', () => {
    let expected_sku = 1;
    service.setGetOriginalSkuResult(expected_sku);
    return service.getSku().then((sku) => {
      assertEquals(sku.skuIndex, expected_sku);
    });
  });

  test('SetSkuDefaultOk', () => {
    return service.setSku(1).then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
  });

  test('SetSkuOkUpdatesGetSkuResult', () => {
    let original_sku = 1;
    let expected_sku = 2;
    service.setGetOriginalSkuResult(original_sku);
    service.setSetSkuResult(RmadErrorCode.kOk);
    service.setSku(expected_sku).then((error) => {
      assertEquals(error.error, RmadErrorCode.kOk);
    });
    return service.getSku().then((sku) => {
      assertEquals(sku.skuIndex, expected_sku);
    });
  });

  test('SetSkuErrorDoesNotUpdateGetSkuResult', () => {
    let original_sku = 1;
    let expected_sku = 2;
    service.setGetOriginalSkuResult(original_sku);
    service.setSetSkuResult(RmadErrorCode.kRequestInvalid);
    service.setSku(expected_sku).then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
    return service.getSku().then((sku) => {
      assertEquals(sku.skuIndex, original_sku);
    });
  });

  test('CutoffBatteryDefaultUndefined', () => {
    return service.cutoffBattery().then((error) => {
      assertEquals(error, undefined);
    });
  });

  test('SetCutoffBatteryUpdatesResult', () => {
    service.setCutoffBatteryResult(RmadErrorCode.kRequestInvalid);
    return service.cutoffBattery().then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ObserveError', () => {
    /** @type {!ErrorObserver} */
    const errorObserver = /** @type {!ErrorObserver} */ ({
      /**
       * Implements ErrorObserver.onError()
       * @param {!RmadErrorCode} error
       */
      onError(error) {
        assertEquals(error, RmadErrorCode.kRequestInvalid);
      }
    });
    service.observeError(errorObserver);
    return service.triggerErrorObserver(RmadErrorCode.kRequestInvalid, 0);
  });

  test('ObserveCalibrationUpdate', () => {
    /** @type {!CalibrationObserver} */
    const calibrationObserver = /** @type {!CalibrationObserver} */ ({
      /**
       * Implements CalibrationObserver.onCalibrationUpdated()
       * @param {!CalibrationComponent} component
       * @param {number} progress
       */
      onCalibrationUpdated(component, progress) {
        assertEquals(component, CalibrationComponent.kAccelerometer);
        assertEquals(progress, 0.5);
      }
    });
    service.observeCalibration(calibrationObserver);
    return service.triggerCalibrationObserver(
        CalibrationComponent.kAccelerometer, 0.5, 0);
  });

  test('ObserveProvisioningUpdate', () => {
    /** @type {!ProvisioningObserver} */
    const provisioningObserver = /** @type {!ProvisioningObserver} */ ({
      /**
       * Implements ProvisioningObserver.onProvisioningUpdated()
       * @param {!ProvisioningStep} step
       * @param {number} progress
       */
      onProvisioningUpdated(step, progress) {
        assertEquals(step, ProvisioningStep.kTwiddleSettings);
        assertEquals(progress, 0.25);
      }
    });
    service.observeProvisioning(provisioningObserver);
    return service.triggerProvisioningObserver(
        ProvisioningStep.kTwiddleSettings, 0.25, 0);
  });

  test('ObserveHardwareWriteProtectionStateChange', () => {
    /** @type {!HardwareWriteProtectionStateObserver} */
    const hardwareWriteProtectionStateObserver =
        /** @type {!HardwareWriteProtectionStateObserver} */ ({
          /**
           * Implements
           * HardwareWriteProtectionStateObserver.
           *     onHardwareWriteProtectionStateChanged()
           * @param {boolean} enable
           */
          onHardwareWriteProtectionStateChanged(enable) {
            assertEquals(enable, true);
          }
        });
    service.observeHardwareWriteProtectionState(
        hardwareWriteProtectionStateObserver);
    return service.triggerHardwareWriteProtectionObserver(true, 0);
  });

  test('ObservePowerCableStateChange', () => {
    /** @type {!PowerCableStateObserver} */
    const powerCableStateObserver = /** @type {!PowerCableStateObserver} */ ({
      /**
       * Implements PowerCableStateObserver.onPowerCableStateChanged()
       * @param {boolean} enable
       */
      onPowerCableStateChanged(enable) {
        assertEquals(enable, true);
      }
    });
    service.observePowerCableState(powerCableStateObserver);
    return service.triggerPowerCableObserver(true, 0);
  });
}
