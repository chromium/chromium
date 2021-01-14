// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {ButtonState} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {ESimPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';
// #import {assertEquals, assertTrue} from '../../../chai_assert.js';
// #import {FakeESimManagerRemote} from './fake_esim_manager_remote.m.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// clang-format on

suite('CrComponentsEsimFlowUiTest', function() {
  let eSimPage;
  let eSimManagerRemote;
  let ironPages;
  let profileLoadingPage;
  let profileDiscoveryPage;
  let activationCodePage;
  let confirmationCodePage;
  let finalPage;

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    document.body.appendChild(eSimPage);
    Polymer.dom.flush();

    ironPages = eSimPage.$$('iron-pages');
    profileLoadingPage = eSimPage.$$('#profileLoadingPage');
    profileDiscoveryPage = eSimPage.$$('#profileDiscoveryPage');
    activationCodePage = eSimPage.$$('#activationCodePage');
    confirmationCodePage = eSimPage.$$('#confirmationCodePage');
    finalPage = eSimPage.$$('#finalPage');

    assertTrue(!!profileLoadingPage);
    assertTrue(!!profileDiscoveryPage);
    assertTrue(!!activationCodePage);
    assertTrue(!!confirmationCodePage);
    assertTrue(!!finalPage);
  });

  function assertSelectedPage(pageName, page) {
    assertEquals(ironPages.selected, pageName);
    assertEquals(ironPages.selected, page.id);
  }

  function enterConfirmationCode() {
    const confirmationCodeInput = confirmationCodePage.$$('#confirmationCode');
    confirmationCodeInput.value = 'CONFIRMATION_CODE';
    assertFalse(confirmationCodeInput.invalid);

    // Next button should now be enabled.
    assertTrue(
        eSimPage.buttonState.next ===
        cellularSetup.ButtonState.SHOWN_AND_ENABLED);

    eSimPage.navigateForward();
    return confirmationCodeInput;
  }

  suite('No eSIM profiles flow', function() {
    let euicc;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(0);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0];
      eSimPage.initSubflow();

      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Should now be at the activation code page.
      assertSelectedPage(
          cellular_setup.ESimPageName.ACTIVATION_CODE, activationCodePage);
      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';

      // Next button should now be enabled.
      assertTrue(
          eSimPage.buttonState.next ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);
    });

    test('Invalid activation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorInvalidActivationCode);

      eSimPage.navigateForward();

      await flushAsync();

      // Install should fail and still be at activation code page.
      assertSelectedPage(
          cellular_setup.ESimPageName.ACTIVATION_CODE, activationCodePage);
      assertTrue(activationCodePage.$$('#scanSuccessContainer').hidden);
      assertFalse(activationCodePage.$$('#scanFailureContainer').hidden);
    });

    test('Valid activation code', async function() {
      eSimPage.navigateForward();

      await flushAsync();

      // Should go to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    });

    test('Valid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      eSimPage.navigateForward();

      await flushAsync();

      // Confirmation code page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      enterConfirmationCode();

      await flushAsync();

      // Should go to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    });

    test('Invalid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      eSimPage.navigateForward();

      await flushAsync();

      // Confirmation code page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput = enterConfirmationCode();

      await flushAsync();

      // Should still be at confirmation code page with input showing error.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);
      assertTrue(confirmationCodeInput.invalid);
    });
  });

  suite('Single eSIM profile flow', function() {
    let profile;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(1);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      const profileList = await availableEuiccs.euiccs[0].getProfileList();
      profile = profileList.profiles[0];
      eSimPage.initSubflow();
    });

    test('Successful install', async function() {
      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Should go directly to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
      assertFalse(!!finalPage.$$('.error'));
    });

    test('Unsuccessful install', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);

      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Should go directly to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
      assertTrue(!!finalPage.$$('.error'));
    });

    test('Valid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Confirmation code page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      enterConfirmationCode();

      await flushAsync();

      // Should go to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    });

    test('Invalid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Confirmation code page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput = enterConfirmationCode();

      await flushAsync();

      // Should still be at confirmation code page with input showing error.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);
      assertTrue(confirmationCodeInput.invalid);
    });
  });

  suite('Multiple eSIM profiles flow', function() {
    setup(async function() {
      eSimManagerRemote.addEuiccForTest(2);
      eSimPage.initSubflow();

      // Loading page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);

      await flushAsync();

      // Should go to profile discovery page.
      assertSelectedPage(
          cellular_setup.ESimPageName.PROFILE_DISCOVERY, profileDiscoveryPage);
    });

    test('Skip discovery flow', async function() {
      // Simulate pressing 'Skip'.
      assertTrue(
          eSimPage.buttonState.skipDiscovery ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      eSimPage.navigateForward();
      Polymer.dom.flush();

      // Should now be at the activation code page.
      assertSelectedPage(
          cellular_setup.ESimPageName.ACTIVATION_CODE, activationCodePage);

      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';

      // Simulate pressing 'Next'.
      assertTrue(
          eSimPage.buttonState.next ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      eSimPage.navigateForward();
      await flushAsync();

      // Should now be at the final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    });

    test('Select profile flow', async function() {
      // Select the first profile on the list.
      const profileList = profileDiscoveryPage.$$('#profileList');
      profileList.selectItem(profileList.items[0]);
      Polymer.dom.flush();

      // The 'Next' button should now be enabled.
      assertTrue(
          eSimPage.buttonState.next ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      assertTrue(
          eSimPage.buttonState.skipDiscovery ===
          cellularSetup.ButtonState.HIDDEN);

      // Simulate pressing 'Next'.
      eSimPage.navigateForward();
      await flushAsync();

      // Should now be at the final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
      assertFalse(!!finalPage.$$('.error'));
    });

    test('Select profile with valid confirmation code flow', async function() {
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      const profileList = await availableEuiccs.euiccs[0].getProfileList();
      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      // Select the first profile on the list.
      const profileListUI = profileDiscoveryPage.$$('#profileList');
      profileListUI.selectItem(profileListUI.items[0]);
      Polymer.dom.flush();

      // The 'Next' button should now be enabled.
      assertTrue(
          eSimPage.buttonState.next ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);
      assertTrue(
          eSimPage.buttonState.skipDiscovery ===
          cellularSetup.ButtonState.HIDDEN);

      // Simulate pressing 'Next'.
      eSimPage.navigateForward();
      await flushAsync();

      // Confirmation code page should be showing.
      assertSelectedPage(
          cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);

      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      confirmationCodePage.$$('#confirmationCode').value = 'CONFIRMATION_CODE';

      // Next button should now be enabled.
      assertTrue(
          eSimPage.buttonState.next ===
          cellularSetup.ButtonState.SHOWN_AND_ENABLED);

      eSimPage.navigateForward();

      await flushAsync();

      // Should go to final page.
      assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    });
  });
});
