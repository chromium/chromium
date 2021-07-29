// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {CalibrationComponent, CalibrationObserverRemote, ComponentRepairStatus, ComponentType, ErrorObserverRemote, HardwareWriteProtectionStateObserverRemote, PowerCableStateObserverRemote, ProvisioningObserverRemote, ProvisioningStep, RmadErrorCode, RmaState, ShimlessRmaServiceInterface} from 'chrome://shimless-rma/shimless_rma_types.js';

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
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetCurrentStateWelcomeOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetCurrentStateWelcomeError', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kMissingComponent},
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kMissingComponent);
    });
  });

  test('TransitionNextStateDefaultRmaNotRequired', () => {
    return service.transitionNextState().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('TransitionNextStateConfigureNetworkOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.transitionNextState().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('TransitionNextStateConfigureNetworkTransitionFailed', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.transitionNextState().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kTransitionFailed);
    });
  });

  test('TransitionPreviousStateDefaultRmaNotRequired', () => {
    return service.transitionPreviousState().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('TransitionPreviousStateWelcomeOk', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    service.transitionNextState().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
    return service.transitionPreviousState().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('TransitionPreviousStateWelcomeTransitionFailed', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.transitionPreviousState().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
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

  test('GetCurrentOsVersionDefaultUndefined', () => {
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version, undefined);
    });
  });

  test('SetGetCurrentOsVersionResultUpdatesResult', () => {
    service.setGetCurrentOsVersionResult('1234.56.78');
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version.version, '1234.56.78');
    });
  });

  test('UpdateOsOk', () => {
    let states = [
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOs().then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('UpdateOsWhenRmaNotRequired', () => {
    return service.updateOs().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('UpdateOsWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOs().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('UpdateOsSkippedOk', () => {
    let states = [
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOsSkipped().then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('UpdateOsSkippedWhenRmaNotRequired', () => {
    return service.updateOs().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('UpdateOsSkippedWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOs().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetSameOwnerOk', () => {
    let states = [
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetSameOwnerWhenRmaNotRequired', () => {
    return service.setSameOwner().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('SetSameOwnerWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetDifferentOwnerOk', () => {
    let states = [
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetDifferentOwnerWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ChooseManuallyDisableWriteProtectOk', () => {
    let states = [
      {
        state: RmaState.kChooseWriteProtectDisableMethod,
        error: RmadErrorCode.kOk
      },
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ChooseManuallyDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ChooseRsuDisableWriteProtectOk', () => {
    let states = [
      {
        state: RmaState.kChooseWriteProtectDisableMethod,
        error: RmadErrorCode.kOk
      },
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ChooseRsuDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetRsuDisableWriteProtectChallengeUndefined', () => {
    return service.getRsuDisableWriteProtectChallenge().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetRsuDisableWriteProtectChallengeResultUpdatesResult', () => {
    let expected_challenge = '9876543210';
    service.setGetRsuDisableWriteProtectChallengeResult(expected_challenge);
    return service.getRsuDisableWriteProtectChallenge().then((challenge) => {
      assertEquals(challenge.challenge, expected_challenge);
    });
  });

  test('SetRsuDisableWriteProtectCodeOk', () => {
    let states = [
      {state: RmaState.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetRsuDisableWriteProtectCodeWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
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
        state: ComponentRepairStatus.kOriginal
      },
      {
        component: ComponentType.kTouchpad,
        state: ComponentRepairStatus.kMissing
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, expected_components);
    });
  });

  test('SetComponentListOk', () => {
    let components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal
      },
    ];
    let states = [
      {state: RmaState.kSelectComponents, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components).then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetComponentListWrongStateFails', () => {
    let components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal
      },
    ];
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components).then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReworkMainboardOk', () => {
    let states = [
      {state: RmaState.kSelectComponents, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReworkMainboardWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReimageSkippedOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageSkipped().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageSkippedWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageSkipped().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReimageFromDownloadOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromDownload().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageFromDownloadWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromDownload().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReimageFromUsbOk', () => {
    let states = [
      {state: RmaState.kChooseFirmwareReimageMethod, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromUsb().then((state) => {
      assertEquals(state.state, RmaState.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageFromUsbWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reimageFromUsb().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
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

  test('SetDeviceInformationOk', () => {
    let states = [
      {state: RmaState.kUpdateDeviceInformation, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDeviceInformation('serial number', 1, 2).then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('FinalizeAndRebootOk', () => {
    let states = [
      {state: RmaState.kRepairComplete, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.finalizeAndReboot().then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('FinalizeAndRebootWhenRmaNotRequired', () => {
    return service.finalizeAndReboot().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('FinalizeAndRebootWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.finalizeAndReboot().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('FinalizeAndShutdownOk', () => {
    let states = [
      {state: RmaState.kRepairComplete, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.finalizeAndShutdown().then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('FinalizeAndShutdownWhenRmaNotRequired', () => {
    return service.finalizeAndShutdown().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('FinalizeAndShutdownWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.finalizeAndShutdown().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('CutoffBatteryOk', () => {
    let states = [
      {state: RmaState.kRepairComplete, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.cutoffBattery().then((state) => {
      assertEquals(state.state, RmaState.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('CutoffBatteryWhenRmaNotRequired', () => {
    return service.cutoffBattery().then((state) => {
      assertEquals(state.state, RmaState.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('CutoffBatteryWrongStateFails', () => {
    let states = [
      {state: RmaState.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: RmaState.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.cutoffBattery().then((state) => {
      assertEquals(state.state, RmaState.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ObserveError', () => {
    /** @type {!ErrorObserverRemote} */
    const errorObserver = /** @type {!ErrorObserverRemote} */ ({
      /**
       * Implements ErrorObserverRemote.onError()
       * @param {!RmadErrorCode} error
       */
      onError(error) {
        assertEquals(error, RmadErrorCode.kRequestInvalid);
      }
    });
    service.observeError(errorObserver);
    return service.triggerErrorObserver(RmadErrorCode.kRequestInvalid, 0);
  });

  test('ObserveCalibrationUpdated', () => {
    /** @type {!CalibrationObserverRemote} */
    const calibrationObserver = /** @type {!CalibrationObserverRemote} */ ({
      /**
       * Implements CalibrationObserverRemote.onCalibrationUpdated()
       * @param {!CalibrationComponent} component
       * @param {number} progress
       */
      onCalibrationUpdated(component, progress) {
        assertEquals(component, CalibrationComponent.kAccelerometer);
        assertEquals(progress, 0.5);
      }
    });
    service.observeCalibrationProgress(calibrationObserver);
    return service.triggerCalibrationObserver(
        CalibrationComponent.kAccelerometer, 0.5, 0);
  });

  test('ObserveProvisioningUpdated', () => {
    /** @type {!ProvisioningObserverRemote} */
    const provisioningObserver = /** @type {!ProvisioningObserverRemote} */ ({
      /**
       * Implements ProvisioningObserverRemote.onProvisioningUpdated()
       * @param {!ProvisioningStep} step
       * @param {number} progress
       */
      onProvisioningUpdated(step, progress) {
        assertEquals(step, ProvisioningStep.kInProgress);
        assertEquals(progress, 0.25);
      }
    });
    service.observeProvisioningProgress(provisioningObserver);
    return service.triggerProvisioningObserver(
        ProvisioningStep.kInProgress, 0.25, 0);
  });

  test('ObserveHardwareWriteProtectionStateChange', () => {
    /** @type {!HardwareWriteProtectionStateObserverRemote} */
    const hardwareWriteProtectionStateObserver =
        /** @type {!HardwareWriteProtectionStateObserverRemote} */ ({
          /**
           * Implements
           * HardwareWriteProtectionStateObserverRemote.
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
    /** @type {!PowerCableStateObserverRemote} */
    const powerCableStateObserver =
        /** @type {!PowerCableStateObserverRemote} */ ({
          /**
           * Implements PowerCableStateObserverRemote.onPowerCableStateChanged()
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
