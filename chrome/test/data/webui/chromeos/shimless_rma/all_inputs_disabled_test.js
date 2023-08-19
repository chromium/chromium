// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CriticalErrorPage} from 'chrome://shimless-rma/critical_error_page.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseDestinationPageElement} from 'chrome://shimless-rma/onboarding_choose_destination_page.js';
import {OnboardingChooseWpDisableMethodPage} from 'chrome://shimless-rma/onboarding_choose_wp_disable_method_page.js';
import {OnboardingEnterRsuWpDisableCodePage} from 'chrome://shimless-rma/onboarding_enter_rsu_wp_disable_code_page.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {OnboardingNetworkPage} from 'chrome://shimless-rma/onboarding_network_page.js';
import {OnboardingSelectComponentsPageElement} from 'chrome://shimless-rma/onboarding_select_components_page.js';
import {OnboardingUpdatePageElement} from 'chrome://shimless-rma/onboarding_update_page.js';
import {OnboardingWaitForManualWpDisablePage} from 'chrome://shimless-rma/onboarding_wait_for_manual_wp_disable_page.js';
import {OnboardingWpDisableCompletePage} from 'chrome://shimless-rma/onboarding_wp_disable_complete_page.js';
import {ReimagingCalibrationFailedPage} from 'chrome://shimless-rma/reimaging_calibration_failed_page.js';
import {ReimagingCalibrationRunPage} from 'chrome://shimless-rma/reimaging_calibration_run_page.js';
import {ReimagingCalibrationSetupPage} from 'chrome://shimless-rma/reimaging_calibration_setup_page.js';
import {ReimagingDeviceInformationPage} from 'chrome://shimless-rma/reimaging_device_information_page.js';
import {UpdateRoFirmwarePage} from 'chrome://shimless-rma/reimaging_firmware_update_page.js';
import {ReimagingProvisioningPage} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {StateComponentMapping} from 'chrome://shimless-rma/shimless_rma.js';
import {CalibrationSetupInstruction, State} from 'chrome://shimless-rma/shimless_rma_types.js';
import {WrapupFinalizePage} from 'chrome://shimless-rma/wrapup_finalize_page.js';
import {WrapupRepairCompletePage} from 'chrome://shimless-rma/wrapup_repair_complete_page.js';
import {WrapupRestockPage} from 'chrome://shimless-rma/wrapup_restock_page.js';
import {WrapupWaitForManualWpEnablePage} from 'chrome://shimless-rma/wrapup_wait_for_manual_wp_enable_page.js';

import {assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

suite('allInputsDisabledTest', function() {
  const INPUT_TYPES =
      ['cr-input', 'cr-button', 'cr-radio-group', 'cr-slider', 'cr-toggle'];

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    document.body.innerHTML = trustedTypes.emptyHTML;
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
    setupFakeService();
  });

  teardown(() => {
    service.reset();
  });

  // Sets the bare minimum junk data on the fake service so the components can
  // load.
  function setupFakeService() {
    // kUpdateOs
    service.setGetCurrentOsVersionResult(null);

    // kEnterRSUWPDisableCode
    service.setGetRsuDisableWriteProtectChallengeResult('');
    service.setGetRsuDisableWriteProtectHwidResult('');

    // kUpdateDeviceInformation
    service.setGetOriginalSerialNumberResult('');
    service.setGetRegionListResult([]);
    service.setGetOriginalRegionResult(0);
    service.setGetWhiteLabelListResult([]);
    service.setGetOriginalWhiteLabelResult(0);
    service.setGetSkuListResult([]);
    service.setGetOriginalSkuResult(0);
    service.setGetOriginalDramPartNumberResult('');

    // kCheckCalibration
    service.setGetCalibrationComponentListResult([]);

    // kSetupCalibration
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    // kConfigureNetwork
    service.setCheckForOsUpdatesResult('fake version');
  }

  // Test that the set of inputs specified in |INPUT_TYPES| are disabled on each
  // page when |allButtonsDisabled| is set.
  test('AllInputsDisabled', async () => {
    Object.entries(StateComponentMapping).forEach(state => {
      const component = document.createElement(state[1].componentIs);
      assertTrue(!!component);
      document.body.appendChild(component);

      component.allButtonsDisabled = true;
      for (const inputType of INPUT_TYPES) {
        for (const inputElement of component.shadowRoot.querySelectorAll(
                 inputType)) {
          // Skip buttons in the dialogs because they're not expected to be
          // disabled.
          if (inputElement.closest('cr-dialog')) {
            continue;
          }

          assertTrue(
              inputElement.disabled,
              'Component: ' + component.nodeName +
                  ' has an undisabled input. Input Type: ' + inputType +
                  ' Id: ' + inputElement.id);
        }
      }

      document.body.removeChild(component);
    });
  });
});
