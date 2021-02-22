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
// #import {FakeBarcodeDetector} from './fake_barcode_detector.m.js';
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

  let focusDefaultButtonEventFired = false;

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    document.addEventListener('focus-default-button', () => {
      focusDefaultButtonEventFired = true;
    });

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

  /**
   * Simulates navigating forward to trigger a profile install.
   * Asserts that the button_bar and page state is enabled and not busy before
   * navigating forward. Asserts that the button_bar and page state is disabled
   * and busy during the install.
   */
  async function navigateForwardForInstall(page) {
    const checkShowBusyState =
        (page !== profileDiscoveryPage && page !== finalPage);
    assertEquals(
        eSimPage.buttonState.forward, cellularSetup.ButtonState.ENABLED);
    assertEquals(
        eSimPage.buttonState.backward, cellularSetup.ButtonState.ENABLED);
    if (checkShowBusyState) {
      assertFalse(page.showBusy);
    }
    eSimPage.navigateForward();

    assertEquals(
        eSimPage.buttonState.forward, cellularSetup.ButtonState.DISABLED);
    assertEquals(
        eSimPage.buttonState.backward, cellularSetup.ButtonState.DISABLED);
    if (checkShowBusyState) {
      assertTrue(page.showBusy);
    }

    await flushAsync();
  }

  async function enterConfirmationCode() {
    const confirmationCodeInput = confirmationCodePage.$$('#confirmationCode');
    confirmationCodeInput.value = 'CONFIRMATION_CODE';
    assertFalse(confirmationCodeInput.invalid);

    // Forward button should now be enabled.
    assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ true);

    await navigateForwardForInstall(confirmationCodePage);
    return confirmationCodeInput;
  }

  async function assertFinalPageAndPressDoneButton(shouldBeShowingError) {
    assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    assertEquals(!!finalPage.$$('.error'), shouldBeShowingError);
    assertEquals(
        cellularSetup.ButtonState.ENABLED, eSimPage.buttonState.forward);
    assertEquals(undefined, eSimPage.buttonState.backward);
    assertEquals(undefined, eSimPage.buttonState.cancel);
    assertEquals(eSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    eSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    eSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);
  }

  function assertButtonState(forwardButtonShouldBeEnabled) {
    const buttonState = eSimPage.buttonState;
    assertEquals(buttonState.backward, cellularSetup.ButtonState.ENABLED);
    assertEquals(buttonState.cancel, cellularSetup.ButtonState.ENABLED);
    assertEquals(
        buttonState.forward,
        forwardButtonShouldBeEnabled ? cellularSetup.ButtonState.ENABLED :
                                       cellularSetup.ButtonState.DISABLED);
  }

  function assertFocusDefaultButtonEventFired() {
    assertTrue(focusDefaultButtonEventFired);
    focusDefaultButtonEventFired = false;
  }

  async function assertProfileLoadingPageAndContinue() {
    assertSelectedPage(
        cellular_setup.ESimPageName.PROFILE_LOADING, profileLoadingPage);
    assertButtonState(/*forwardButtonShouldBeEnabled=*/ false);
    await flushAsync();
  }

  function assertProfileDiscoveryPage() {
    assertSelectedPage(
        cellular_setup.ESimPageName.PROFILE_DISCOVERY, profileDiscoveryPage);
    assertButtonState(/*forwardButtonShouldBeEnabled=*/ true);
  }

  function assertActivationCodePage(forwardButtonShouldBeEnabled) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(activationCodePage.$$('#activationCode').value, '');
    }
    assertSelectedPage(
        cellular_setup.ESimPageName.ACTIVATION_CODE, activationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
  }

  function assertConfirmationCodePage(forwardButtonShouldBeEnabled) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(confirmationCodePage.$$('#confirmationCode').value, '');
    }
    assertSelectedPage(
        cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
  }

  suite('No eSIM profiles flow', function() {
    let euicc;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(0);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0];

      activationCodePage.barcodeDetectorClass_ = FakeBarcodeDetector;
      activationCodePage.initBarcodeDetector();
      await flushAsync();
      eSimPage.initSubflow();

      await assertProfileLoadingPageAndContinue();

      // Should now be at the activation code page.
      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';

      // Forward button should now be enabled.
      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
    });

    test('Invalid activation code', async function() {
      activationCodePage.barcodeDetectorClass_ = FakeBarcodeDetector;
      activationCodePage.initBarcodeDetector();
      await flushAsync();

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorInvalidActivationCode);

      await navigateForwardForInstall(activationCodePage);

      // Install should fail and still be at activation code page.
      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
      assertTrue(activationCodePage.$$('#scanSuccessContainer').hidden);
      assertFalse(activationCodePage.$$('#scanFailureContainer').hidden);
    });

    test('Valid activation code', async function() {
      await navigateForwardForInstall(activationCodePage);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test('Valid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(activationCodePage);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode();

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test('Invalid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(activationCodePage);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput = await enterConfirmationCode();

      // Should still be at confirmation code page with input showing error.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
      assertTrue(confirmationCodeInput.invalid);
    });

    test('Navigate backwards from confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(activationCodePage);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
      confirmationCodePage.$$('#confirmationCode').value = 'CONFIRMATION_CODE';

      assertTrue(eSimPage.attemptBackwardNavigation());
      await flushAsync();

      // Should now be at the activation code page.
      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
      assertEquals(
          activationCodePage.$$('#activationCode').value, 'ACTIVATION_CODE');

      // Navigating backwards should return false since we're at the beginning.
      assertFalse(eSimPage.attemptBackwardNavigation());
      await flushAsync();
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
      await assertProfileLoadingPageAndContinue();

      // Should go directly to final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test('Unsuccessful install', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);

      await assertProfileLoadingPageAndContinue();

      // Should go directly to final page.
      await assertFinalPageAndPressDoneButton(true);
    });

    test('Valid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode();

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test('Invalid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput = await enterConfirmationCode();

      // Should still be at confirmation code page with input showing error.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
      assertTrue(confirmationCodeInput.invalid);
    });

    test('Navigate backwards from confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
      confirmationCodePage.$$('#confirmationCode').value = 'CONFIRMATION_CODE';

      // Navigating backwards should return false since we're at the beginning.
      assertFalse(eSimPage.attemptBackwardNavigation());
      await flushAsync();
    });
  });

  suite('Multiple eSIM profiles flow', function() {
    let euicc;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(2);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0];
      eSimPage.initSubflow();

      assertFocusDefaultButtonEventFired();
      await assertProfileLoadingPageAndContinue();

      // Should go to profile discovery page.
      assertProfileDiscoveryPage();
      assertFocusDefaultButtonEventFired();
    });

    function skipDiscovery() {
      // Simulate pressing 'Skip'.
      assertTrue(
          eSimPage.buttonState.forward === cellularSetup.ButtonState.ENABLED);
      eSimPage.navigateForward();
      Polymer.dom.flush();

      // Should now be at the activation code page.
      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
    }

    test('Skip discovery flow', async function() {
      skipDiscovery();

      await navigateForwardForInstall(activationCodePage);

      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test(
        'Navigate backwards from skip discovery flow with confirmation code',
        async function() {
          skipDiscovery();

          euicc.setProfileInstallResultForTest(
              chromeos.cellularSetup.mojom.ProfileInstallResult
                  .kErrorNeedsConfirmationCode);

          await navigateForwardForInstall(activationCodePage);

          // Confirmation code page should be showing.
          assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
          assertFocusDefaultButtonEventFired();
          confirmationCodePage.$$('#confirmationCode').value =
              'CONFIRMATION_CODE';
          assertFalse(focusDefaultButtonEventFired);

          // Simulate pressing 'Backward'.
          assertTrue(eSimPage.attemptBackwardNavigation());
          await flushAsync();

          assertActivationCodePage(/*forwardButtonShouldBeEnabled=*/ true);
          assertFocusDefaultButtonEventFired();
          assertEquals(
              activationCodePage.$$('#activationCode').value,
              'ACTIVATION_CODE');

          assertTrue(eSimPage.attemptBackwardNavigation());
          await flushAsync();

          assertProfileDiscoveryPage();
          assertFocusDefaultButtonEventFired();
          assertEquals(
              eSimPage.forwardButtonLabel, 'Skip & Set up new profile');

          // Navigating backwards should return false since we're at the
          // beginning.
          assertFalse(eSimPage.attemptBackwardNavigation());
        });

    async function selectProfile() {
      // Select the first profile on the list.
      const profileList = profileDiscoveryPage.$$('#profileList');
      profileList.selectItem(profileList.items[0]);
      Polymer.dom.flush();

      // The 'Forward' button should now be enabled.
      assertTrue(
          eSimPage.buttonState.forward === cellularSetup.ButtonState.ENABLED);

      // Simulate pressing 'Forward'.
      await navigateForwardForInstall(profileDiscoveryPage);
    }

    test('Select profile flow', async function() {
      await selectProfile();

      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);
    });

    test('Select profile with valid confirmation code flow', async function() {
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      const profileList = await availableEuiccs.euiccs[0].getProfileList();
      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await selectProfile();

      // Confirmation code page should be showing.
      assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
      assertFocusDefaultButtonEventFired();

      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode();

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);
      assertFocusDefaultButtonEventFired();
    });

    test(
        'Navigate backwards from select profile with confirmation code flow',
        async function() {
          const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
          const profileList = await availableEuiccs.euiccs[0].getProfileList();
          profileList.profiles[0].setProfileInstallResultForTest(
              chromeos.cellularSetup.mojom.ProfileInstallResult
                  .kErrorNeedsConfirmationCode);

          await selectProfile();
          await flushAsync();

          // Confirmation code page should be showing.
          assertConfirmationCodePage(/*forwardButtonShouldBeEnabled=*/ false);
          confirmationCodePage.$$('#confirmationCode').value =
              'CONFIRMATION_CODE';

          assertTrue(eSimPage.attemptBackwardNavigation());
          await flushAsync();

          assertProfileDiscoveryPage();
          assertEquals(eSimPage.forwardButtonLabel, 'Next');

          // Navigating backwards should return false since we're at the
          // beginning.
          assertFalse(eSimPage.attemptBackwardNavigation());
        });
  });
});
