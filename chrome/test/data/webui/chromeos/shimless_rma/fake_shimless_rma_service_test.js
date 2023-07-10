// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeCalibrationComponentsWithFails} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, ComponentRepairStatus, ComponentType, ErrorObserverRemote, FinalizationError, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningError, ProvisioningObserverRemote, ProvisioningStatus, RmadErrorCode, ShutdownMethod, State, UpdateErrorCode, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('fakeShimlessRmaServiceTestSuite', function() {
  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    service = new FakeShimlessRmaService();
  });

  teardown(() => {
    service = null;
  });

  test('GetCurrentStateDefaultRmaNotRequired', () => {
    return service.getCurrentState().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUnknown);
      assertEquals(error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetCurrentStateWelcomeOk', () => {
    const states = [
      {
        state: State.kWelcomeScreen,
        canExit: true,
        canGoBack: false,
        error: RmadErrorCode.kOk,
      },
    ];
    service.setStates(states);

    return service.getCurrentState().then(
        ({stateResult: {state, canExit, canGoBack, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertTrue(canExit);
          assertFalse(canGoBack);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('GetCurrentStateWelcomeError', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kMissingComponent},
    ];
    service.setStates(states);

    return service.getCurrentState().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kWelcomeScreen);
      assertEquals(error, RmadErrorCode.kMissingComponent);
    });
  });

  test('TransitionPreviousStateWelcomeOk', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    service.beginFinalization().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUpdateOs);
      assertEquals(error, RmadErrorCode.kOk);
    });
    return service.transitionPreviousState().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('TransitionPreviousStateWelcomeTransitionFailed', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.transitionPreviousState().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kTransitionFailed);
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
    service.setGetCurrentOsVersionResult(null);
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version.version, null);
    });
  });

  test('SetGetCurrentOsVersionResultUpdatesResult', () => {
    service.setGetCurrentOsVersionResult('1234.56.78');
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version.version, '1234.56.78');
    });
  });

  test('SetUpdateOsResultTrueUpdatesResult', () => {
    service.setUpdateOsResult(true);

    return service.updateOs().then((result) => {
      assertEquals(result.updateStarted, true);
    });
  });

  test('SetUpdateOsResultFalseUpdatesResult', () => {
    service.setUpdateOsResult(false);

    return service.updateOs().then((result) => {
      assertEquals(result.updateStarted, false);
    });
  });

  test('UpdateOsSkippedOk', () => {
    const states = [
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOsSkipped().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kChooseDestination);
      assertEquals(error, RmadErrorCode.kOk);
    });
  });

  test('UpdateOsSkippedWhenRmaNotRequired', () => {
    return service.updateOsSkipped().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUnknown);
      assertEquals(error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('UpdateOsSkippedWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOsSkipped().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kWelcomeScreen);
      assertEquals(error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetSameOwnerOk', () => {
    const states = [
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUpdateOs);
      assertEquals(error, RmadErrorCode.kOk);
    });
  });

  test('SetSameOwnerWhenRmaNotRequired', () => {
    return service.setSameOwner().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUnknown);
      assertEquals(error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('SetSameOwnerWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kWelcomeScreen);
      assertEquals(error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetDifferentOwnerOk', () => {
    const states = [
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUpdateOs);
      assertEquals(error, RmadErrorCode.kOk);
    });
  });

  test('SetDifferentOwnerWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kWelcomeScreen);
      assertEquals(error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ChooseManuallyDisableWriteProtectOk', () => {
    const states = [
      {state: State.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
      {state: State.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
      {state: State.kWaitForManualWPDisable, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWaitForManualWPDisable);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('ChooseManuallyDisableWriteProtectWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kRequestInvalid);
        });
  });

  test('ChooseRsuDisableWriteProtectOk', () => {
    const states = [
      {state: State.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kUpdateOs);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('ChooseRsuDisableWriteProtectWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kRequestInvalid);
        });
  });

  test('GetRsuDisableWriteProtectChallengeUndefined', () => {
    return service.getRsuDisableWriteProtectChallenge().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetRsuDisableWriteProtectChallengeResultUpdatesResult', () => {
    const expectedChallenge = '9876543210';
    service.setGetRsuDisableWriteProtectChallengeResult(expectedChallenge);
    return service.getRsuDisableWriteProtectChallenge().then((challenge) => {
      assertEquals(challenge.challenge, expectedChallenge);
    });
  });

  test('SetRsuDisableWriteProtectCodeOk', () => {
    const states = [
      {state: State.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kUpdateOs);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('SetRsuDisableWriteProtectCodeWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kRequestInvalid);
        });
  });

  test('GetComponentListDefaultEmpty', () => {
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, []);
    });
  });

  test('SetGetComponentListResultUpdatesResult', () => {
    const expectedComponents = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal,
      },
      {
        component: ComponentType.kTouchpad,
        state: ComponentRepairStatus.kMissing,
      },
    ];
    service.setGetComponentListResult(expectedComponents);
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, expectedComponents);
    });
  });

  test('SetComponentListOk', () => {
    const components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal,
      },
    ];
    const states = [
      {state: State.kSelectComponents, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components)
        .then(({stateResult: {state, error}}) => {
          assertEquals(state, State.kUpdateOs);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('SetComponentListWrongStateFails', () => {
    const components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal,
      },
    ];
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components)
        .then(({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kRequestInvalid);
        });
  });

  test('ReworkMainboardOk', () => {
    const states = [
      {state: State.kSelectComponents, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kUpdateOs);
      assertEquals(error, RmadErrorCode.kOk);
    });
  });

  test('ReworkMainboardWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then(({stateResult: {state, error}}) => {
      assertEquals(state, State.kWelcomeScreen);
      assertEquals(error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetWriteProtectDisableCompleteActionDefaultUndefined', () => {
    return service.getWriteProtectDisableCompleteAction().then((res) => {
      assertEquals(undefined, res);
    });
  });

  test('SetGetWriteProtectDisableCompleteStateUpdatesAction', () => {
    service.setGetWriteProtectDisableCompleteAction(
        WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen);
    return service.getWriteProtectDisableCompleteAction().then((res) => {
      assertEquals(
          WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen,
          res.action);
    });
  });

  test('ReimageRoFirmwareUpdateCompleteOk', () => {
    const states = [
      {state: State.kUpdateRoFirmware, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.roFirmwareUpdateComplete().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kUpdateOs);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('ReimageRoFirmwareUpdateCompleteWrongStateFails', () => {
    const states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.roFirmwareUpdateComplete().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kWelcomeScreen);
          assertEquals(error, RmadErrorCode.kRequestInvalid);
        });
  });

  test('GetRegionListDefaultUndefined', () => {
    return service.getRegionList().then((regions) => {
      assertEquals(regions, undefined);
    });
  });

  test('SetGetRegionListResultUpdatesResult', () => {
    const regionList = ['America', 'Asia', 'Europe'];
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
    const skuList = [1, 202, 33];
    service.setGetSkuListResult(skuList);
    return service.getSkuList().then((skus) => {
      assertDeepEquals(skus.skus, skuList);
    });
  });

  test('GetWhiteLabelListDefaultUndefined', () => {
    return service.getWhiteLabelList().then((whiteLabels) => {
      assertEquals(whiteLabels, undefined);
    });
  });

  test('SetGetWhiteLabelListResultUpdatesResult', () => {
    const whiteLabelList =
        ['White-label 10', 'White-label 0', 'White-label 9999'];
    service.setGetWhiteLabelListResult(whiteLabelList);
    return service.getWhiteLabelList().then((whiteLabels) => {
      assertDeepEquals(whiteLabels.whiteLabels, whiteLabelList);
    });
  });

  test('GetOriginalSerialNumberDefaultUndefined', () => {
    return service.getOriginalSerialNumber().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetOriginalSerialNumberResultUpdatesResult', () => {
    const expectedSerialNumber = '123456789';
    service.setGetOriginalSerialNumberResult(expectedSerialNumber);
    return service.getOriginalSerialNumber().then((serialNumber) => {
      assertEquals(serialNumber.serialNumber, expectedSerialNumber);
    });
  });

  test('GetOriginalRegionDefaultUndefined', () => {
    return service.getOriginalRegion().then((region) => {
      assertEquals(region, undefined);
    });
  });

  test('SetGetOriginalRegionResultUpdatesResult', () => {
    const expectedRegion = 1;
    service.setGetOriginalRegionResult(expectedRegion);
    return service.getOriginalRegion().then((region) => {
      assertEquals(region.regionIndex, expectedRegion);
    });
  });

  test('GetOriginalSkuDefaultUndefined', () => {
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku, undefined);
    });
  });

  test('SetGetOriginalSkuResultUpdatesResult', () => {
    const expectedSku = 1;
    service.setGetOriginalSkuResult(expectedSku);
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku.skuIndex, expectedSku);
    });
  });

  test('GetOriginalWhiteLabelDefaultUndefined', () => {
    return service.getOriginalWhiteLabel().then((whiteLabel) => {
      assertEquals(whiteLabel, undefined);
    });
  });

  test('SetGetOriginalWhiteLabelResultUpdatesResult', () => {
    const expectedWhiteLabel = 1;
    service.setGetOriginalWhiteLabelResult(expectedWhiteLabel);
    return service.getOriginalWhiteLabel().then((whiteLabel) => {
      assertEquals(whiteLabel.whiteLabelIndex, expectedWhiteLabel);
    });
  });

  test('GetOriginalDramPartNumberDefaultUndefined', () => {
    return service.getOriginalDramPartNumber().then((dramPartNumber) => {
      assertEquals(dramPartNumber, undefined);
    });
  });

  test('SetGetOriginalDramPartNumberResultUpdatesResult', () => {
    const expectedDramPartNumber = '123-456-789';
    service.setGetOriginalDramPartNumberResult(expectedDramPartNumber);
    return service.getOriginalDramPartNumber().then((dramPartNumber) => {
      assertEquals(dramPartNumber.dramPartNumber, expectedDramPartNumber);
    });
  });

  test('SetDeviceInformationOk', () => {
    const states = [
      {state: State.kUpdateDeviceInformation, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service
        .setDeviceInformation('serial number', 1, 2, 3, '123-456-789', false, 1)
        .then(({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('GetCalibrationComponentList', () => {
    const states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    const expectedCalibrationComponents = [
      /** @type {!CalibrationComponentStatus} */
      ({
        component: ComponentType.kLidAccelerometer,
        status: CalibrationStatus.kCalibrationInProgress,
        progress: 0.5,
      }),
      /** @type {!CalibrationComponentStatus} */
      ({
        component: ComponentType.kBaseAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      }),
    ];
    service.setGetCalibrationComponentListResult(expectedCalibrationComponents);

    return service.getCalibrationComponentList().then((result) => {
      assertDeepEquals(expectedCalibrationComponents, result.components);
    });
  });

  test('GetCalibrationInstructions', () => {
    const states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    return service.getCalibrationSetupInstructions().then((result) => {
      assertDeepEquals(
          CalibrationSetupInstruction
              .kCalibrationInstructionPlaceBaseOnFlatSurface,
          result.instructions);
    });
  });

  test('StartCalibrationOk', () => {
    const states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    return service.startCalibration(fakeCalibrationComponentsWithFails)
        .then(({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('RunCalibrationStepOk', () => {
    const states = [
      {state: State.kSetupCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.runCalibrationStep().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('ContinueCalibrationOk', () => {
    const states = [
      {state: State.kRunCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.continueCalibration().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('CalibrationCompleteOk', () => {
    const states = [
      {state: State.kRunCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.calibrationComplete().then(
        ({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
        });
  });

  test('GetLog', () => {
    const states = [{state: State.kRepairComplete, error: RmadErrorCode.kOk}];
    service.setStates(states);
    const expectedLog = 'fake log';
    service.setGetLogResult(expectedLog);
    return service.getLog().then((res) => {
      assertEquals(expectedLog, res.log);
    });
  });

  test('SaveLog', () => {
    const states = [{state: State.kRepairComplete, error: RmadErrorCode.kOk}];
    service.setStates(states);
    const expectedSavePath = {'path': 'fake/save/path'};
    service.setSaveLogResult(expectedSavePath);
    return service.saveLog().then((res) => {
      assertEquals(expectedSavePath, res.savePath);
    });
  });

  test('SetGetPowerwashRequiredResultTrueUpdatesResult', () => {
    service.setGetPowerwashRequiredResult(true);

    return service.getPowerwashRequired().then((result) => {
      assertEquals(true, result.powerwashRequired);
    });
  });

  test('SetGetPowerwashRequiredResultFalseUpdatesResult', () => {
    service.setGetPowerwashRequiredResult(false);

    return service.getPowerwashRequired().then((result) => {
      assertEquals(false, result.powerwashRequired);
    });
  });

  test('EndRma', () => {
    const states = [
      {state: State.kRepairComplete, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRma(ShutdownMethod.kReboot)
        .then(({stateResult: {state, error}}) => {
          assertEquals(state, State.kChooseDestination);
          assertEquals(error, RmadErrorCode.kOk);
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
      },
    });
    service.observeError(errorObserver);
    return service.triggerErrorObserver(RmadErrorCode.kRequestInvalid, 0);
  });

  test('ObserveOsUpdate', () => {
    /** @type {!OsUpdateObserverRemote} */
    const osUpdateObserver = /** @type {!OsUpdateObserverRemote} */ ({
      /**
       * Implements OsUpdateObserverRemote.onOsUpdateProgressUpdated()
       * @param {!OsUpdateOperation} operation
       * @param {number} progress
       */
      onOsUpdateProgressUpdated(operation, progress) {
        assertEquals(operation, OsUpdateOperation.kDownloading);
        assertEquals(progress, 0.75);
      },
    });
    service.observeOsUpdateProgress(osUpdateObserver);
    return service.triggerOsUpdateObserver(
        OsUpdateOperation.kDownloading, 0.75, UpdateErrorCode.kSuccess, 0);
  });

  test('ObserveRoFirmwareUpdate', () => {
    /** @type {!UpdateRoFirmwareObserverRemote} */
    const roFirmwareUpdateObserver =
        /** @type {!UpdateRoFirmwareObserverRemote} */ ({
          /**
           * Implements
           * UpdateRoFirmwareObserver.onUpdateRoFirmwareStatusChanged()
           * @param {!UpdateRoFirmwareStatus} status
           */
          onUpdateRoFirmwareStatusChanged(status) {
            assertEquals(UpdateRoFirmwareStatus.kDownloading, status);
          },
        });
    service.observeRoFirmwareUpdateProgress(roFirmwareUpdateObserver);
    return service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kDownloading, 0);
  });

  test('ObserveCalibrationUpdated', () => {
    /** @type {!CalibrationObserverRemote} */
    const calibrationObserver = /** @type {!CalibrationObserverRemote} */ ({
      /**
       * Implements CalibrationObserverRemote.onCalibrationUpdated()
       * @param {!CalibrationComponentStatus} calibrationStatus
       */
      onCalibrationUpdated(calibrationStatus) {
        assertEquals(
            calibrationStatus.component, ComponentType.kBaseAccelerometer);
        assertEquals(
            calibrationStatus.status, CalibrationStatus.kCalibrationComplete);
        assertEquals(calibrationStatus.progress, 0.5);
      },
    });
    service.observeCalibrationProgress(calibrationObserver);
    return service.triggerCalibrationObserver(
        /** @type {!CalibrationComponentStatus} */
        ({
          component: ComponentType.kBaseAccelerometer,
          status: CalibrationStatus.kCalibrationComplete,
          progress: 0.5,
        }),
        0);
  });

  test('ObserveCalibrationStepComplete', () => {
    /** @type {!CalibrationObserverRemote} */
    const calibrationObserver = /** @type {!CalibrationObserverRemote} */ ({
      /**
       * Implements CalibrationObserverRemote.onCalibrationUpdated()
       * @param {!CalibrationOverallStatus} status
       */
      onCalibrationStepComplete(status) {
        assertEquals(
            status, CalibrationOverallStatus.kCalibrationOverallComplete);
      },
    });
    service.observeCalibrationProgress(calibrationObserver);
    return service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, 0);
  });

  test('ObserveProvisioningUpdated', () => {
    /** @type {!ProvisioningObserverRemote} */
    const provisioningObserver = /** @type {!ProvisioningObserverRemote} */ ({
      /**
       * Implements ProvisioningObserverRemote.onProvisioningUpdated()
       * @param {!ProvisioningStatus} status
       * @param {number} progress
       */
      onProvisioningUpdated(status, progress) {
        assertEquals(status, ProvisioningStatus.kInProgress);
        assertEquals(progress, 0.25);
      },
    });
    service.observeProvisioningProgress(provisioningObserver);
    return service.triggerProvisioningObserver(
        ProvisioningStatus.kInProgress, 0.25, ProvisioningError.kUnknown, 0);
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
          },
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
          },
        });
    service.observePowerCableState(powerCableStateObserver);
    return service.triggerPowerCableObserver(true, 0);
  });

  test('ObserveHardwareVerificationStatus', () => {
    /** @type {!HardwareVerificationStatusObserverRemote} */
    const observer =
        /** @type {!HardwareVerificationStatusObserverRemote} */ ({
          /**
           * Implements
           * HardwareVerificationStatusObserverRemote.
           *      onHardwareVerificationResult()
           * @param {boolean} isCompliant
           * @param {string} errorMessage
           */
          onHardwareVerificationResult(isCompliant, errorMessage) {
            assertEquals(true, isCompliant);
            assertEquals('ok', errorMessage);
          },
        });
    service.observeHardwareVerificationStatus(observer);
    return service.triggerHardwareVerificationStatusObserver(true, 'ok', 0);
  });

  test('ObserveFinalizationStatus', () => {
    /** @type {!FinalizationObserverRemote} */
    const finalizationObserver =
        /** @type {!FinalizationObserverRemote} */ ({
          /**
           * Implements
           * FinalizationObserverRemote.onFinalizationUpdated()
           * @param {!FinalizationStatus} status
           * @param {number} progress
           */
          onFinalizationUpdated(status, progress) {
            assertEquals(FinalizationStatus.kInProgress, status);
            assertEquals(0.5, progress);
          },
        });
    service.observeFinalizationStatus(finalizationObserver);
    return service.triggerFinalizationObserver(
        FinalizationStatus.kInProgress, 0.5, FinalizationError.kUnknown, 0);
  });
});
