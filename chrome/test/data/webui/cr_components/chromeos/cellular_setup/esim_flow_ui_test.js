// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {ButtonState} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {ESimPageName, ESimSetupFlowResult, ESIM_SETUP_RESULT_METRIC_NAME, SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME, FAILED_ESIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_flow_ui.m.js';
// #import {assertEquals, assertTrue} from '../../../chai_assert.js';
// #import {FakeESimManagerRemote} from './fake_esim_manager_remote.m.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// #import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.m.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {LoadingPageState} from 'chrome://resources/cr_components/chromeos/cellular_setup/setup_loading_page.m.js';
// #import {MockMetricsPrivate} from './mock_metrics_private.m.js';
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
  let networkConfigRemote;

  let focusDefaultButtonEventFired = false;
  let wifiGuidPrefix = 'wifi';

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /** @param {ESimSetupFlowResult} eSimSetupFlowResult */
  function endFlowAndVerifyResult(eSimSetupFlowResult) {
    eSimPage.remove();
    Polymer.dom.flush();
    assertEquals(
        chrome.metricsPrivate.getHistogramEnumValueCount(eSimSetupFlowResult),
        1);

    if (eSimSetupFlowResult === ESimSetupFlowResult.SUCCESS) {
      assertEquals(
          chrome.metricsPrivate.getHistogramCount(
              FAILED_ESIM_SETUP_DURATION_METRIC_NAME),
          0);
      assertEquals(
          chrome.metricsPrivate.getHistogramCount(
              SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
          1);
      return;
    }

    assertEquals(
        chrome.metricsPrivate.getHistogramCount(
            FAILED_ESIM_SETUP_DURATION_METRIC_NAME),
        1);
    assertEquals(
        chrome.metricsPrivate.getHistogramCount(
            SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
        0);
  }

  /** Adds an actively online wifi network and esim network. */
  function addOnlineWifiNetwork() {
    const onlineNetwork = OncMojo.getDefaultNetworkState(
        chromeos.networkConfig.mojom.NetworkType.kWiFi, wifiGuidPrefix);
    onlineNetwork.connectionState =
        chromeos.networkConfig.mojom.ConnectionStateType.kOnline;
    networkConfigRemote.addNetworksForTest([onlineNetwork]);
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ =
        networkConfigRemote;
  }

  /** Takes actively online network offline. */
  function takeWifiNetworkOffline() {
    networkConfigRemote.setNetworkConnectionStateForTest(
        wifiGuidPrefix + '_guid',
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected);
  }

  test('Error fetching profiles', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    const euicc = availableEuiccs.euiccs[0];

    euicc.setRequestPendingProfilesResult(
        chromeos.cellularSetup.mojom.ESimOperationResult.kFailure);
    eSimPage.initSubflow();

    await flushAsync();
    endFlowAndVerifyResult(ESimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();

    addOnlineWifiNetwork();

    chrome.metricsPrivate = new MockMetricsPrivate();
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

    // Captures the function that is called every time the interval timer
    // timeouts.
    const setIntervalFunction = (fn, milliseconds) => {
      intervalFunction = fn;
      return 1;
    };

    // In tests, pausing the video can have race conditions with previous
    // requests to play the video due to the speed of execution. Avoid this by
    // mocking the play and pause actions.
    const playVideoFunction = () => {};
    const stopStreamFunction = (stream) => {};
    await activationCodePage.setFakesForTesting(
        FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
        playVideoFunction, stopStreamFunction);

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
   * @param {HTMLElement} page
   * @param {cellularSetup.ButtonState} previousBackButtonState
   */
  async function navigateForwardForInstall(page, previousBackButtonState) {
    const checkShowBusyState =
        (page !== profileDiscoveryPage && page !== finalPage);
    assertEquals(
        eSimPage.buttonState.forward, cellularSetup.ButtonState.ENABLED);
    assertEquals(eSimPage.buttonState.backward, previousBackButtonState);
    if (checkShowBusyState) {
      assertFalse(page.showBusy);
    }

    // If back button is hidden before installation began, the new back button
    // state should also be hidden, if it was enabled new back button state
    // should be disabled while installation is taking place.
    let newBackButtonState = cellularSetup.ButtonState.HIDDEN;
    if (previousBackButtonState === cellularSetup.ButtonState.ENABLED) {
      newBackButtonState = cellularSetup.ButtonState.DISABLED;
    }
    eSimPage.navigateForward();

    assertEquals(
        eSimPage.buttonState.forward, cellularSetup.ButtonState.DISABLED);
    assertEquals(eSimPage.buttonState.backward, newBackButtonState);
    if (checkShowBusyState) {
      assertTrue(page.showBusy);
    }

    await flushAsync();
  }

  async function enterConfirmationCode(backButtonState) {
    const confirmationCodeInput = confirmationCodePage.$$('#confirmationCode');
    confirmationCodeInput.value = 'CONFIRMATION_CODE';
    assertFalse(confirmationCodeInput.invalid);

    // Forward button should now be enabled.
    assertConfirmationCodePage(
        /*forwardButtonShouldBeEnabled=*/ true,
        /*backButtonState*/ backButtonState);

    await navigateForwardForInstall(
        confirmationCodePage, /*backButtonState*/ backButtonState);
    return confirmationCodeInput;
  }

  async function assertFinalPageAndPressDoneButton(shouldBeShowingError) {
    assertSelectedPage(cellular_setup.ESimPageName.FINAL, finalPage);
    assertEquals(!!finalPage.$$('.error'), shouldBeShowingError);
    assertEquals(
        cellularSetup.ButtonState.ENABLED, eSimPage.buttonState.forward);
    assertEquals(
        cellularSetup.ButtonState.HIDDEN, eSimPage.buttonState.backward);
    assertEquals(cellularSetup.ButtonState.HIDDEN, eSimPage.buttonState.cancel);
    assertEquals(eSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    eSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    eSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);
  }

  function assertButtonState(forwardButtonShouldBeEnabled, backButtonState) {
    const buttonState = eSimPage.buttonState;
    assertEquals(buttonState.backward, backButtonState);
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
    assertButtonState(
        /*forwardButtonShouldBeEnabled*/ false,
        /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
    await flushAsync();
  }

  function assertProfileDiscoveryPage() {
    assertSelectedPage(
        cellular_setup.ESimPageName.PROFILE_DISCOVERY, profileDiscoveryPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled*/ true,
        /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
  }

  function assertActivationCodePage(
      forwardButtonShouldBeEnabled, backButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(activationCodePage.$$('#activationCode').value, '');
    }
    assertSelectedPage(
        cellular_setup.ESimPageName.ACTIVATION_CODE, activationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled, backButtonState);
  }

  function assertConfirmationCodePage(
      forwardButtonShouldBeEnabled, backButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(confirmationCodePage.$$('#confirmationCode').value, '');
    }
    assertSelectedPage(
        cellular_setup.ESimPageName.CONFIRMATION_CODE, confirmationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled, backButtonState);
  }

  suite('No eSIM profiles flow', function() {
    let euicc;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(0);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0];

      await flushAsync();
      eSimPage.initSubflow();

      await assertProfileLoadingPageAndContinue();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';

      // Forward button should now be enabled.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
    });

    test('Invalid activation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorInvalidActivationCode);

      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Install should fail and still be at activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
      assertTrue(activationCodePage.showError);

      endFlowAndVerifyResult(
          ESimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE);
    });

    test('Valid activation code', async function() {
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test('Valid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode(
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test('Invalid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput = await enterConfirmationCode(
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      // Should still be at confirmation code page with input showing error.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
      assertTrue(confirmationCodeInput.invalid);

      endFlowAndVerifyResult(ESimSetupFlowResult.INSTALL_FAIL);
    });

    test('Navigate backwards from confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
      confirmationCodePage.$$('#confirmationCode').value = 'CONFIRMATION_CODE';

      eSimPage.navigateBackward();
      await flushAsync();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
      assertEquals(
          activationCodePage.$$('#activationCode').value, 'ACTIVATION_CODE');

      endFlowAndVerifyResult(
          ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
    });

    test('End flow before installation attempted', async function() {
      await flushAsync();
      endFlowAndVerifyResult(ESimSetupFlowResult.CANCELLED_NO_PROFILES);
    });

    test('No available network before installation', async function() {
      takeWifiNetworkOffline();
      await flushAsync();

      endFlowAndVerifyResult(ESimSetupFlowResult.NO_NETWORK);
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
      await flushAsync();
      // Should go directly to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test('Unsuccessful install', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);

      await assertProfileLoadingPageAndContinue();
      await flushAsync();
      // Should go directly to final page.
      await assertFinalPageAndPressDoneButton(true);

      endFlowAndVerifyResult(ESimSetupFlowResult.INSTALL_FAIL);
    });

    test('Valid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();
      await flushAsync();
      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode(
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test('Invalid confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();
      await flushAsync();
      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);

      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);
      const confirmationCodeInput =
          await enterConfirmationCode(cellularSetup.ButtonState.HIDDEN);

      // Should still be at confirmation code page with input showing error.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
      assertTrue(confirmationCodeInput.invalid);

      endFlowAndVerifyResult(ESimSetupFlowResult.INSTALL_FAIL);
    });

    test('Navigate backwards from confirmation code', async function() {
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await assertProfileLoadingPageAndContinue();
      await flushAsync();
      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
      confirmationCodePage.$$('#confirmationCode').value = 'CONFIRMATION_CODE';

      endFlowAndVerifyResult(
          ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
    });

    test('End flow before single profile fetched', function() {
      // Note that if there is only a single profile, it is automatically
      // installed. If the fetch is not complete by the time the user exits the
      // dialog, a |CANCELLED_WITHOUT_ERROR| is emitted.
      endFlowAndVerifyResult(ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
    });

    test('No available network after installation', async function() {
      // Right after installation, internet connection is lost and the dialog
      // closes, NO_NETWORK should not be reported.
      // flushAsync is needed because installation has a slight delay to
      // simulate time taken to install.
      await flushAsync();
      takeWifiNetworkOffline();
      await flushAsync();

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
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
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = 'ACTIVATION_CODE';
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
    }

    test('Skip discovery flow', async function() {
      skipDiscovery();

      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test(
        'Navigate backwards from skip discovery flow with confirmation code',
        async function() {
          skipDiscovery();

          euicc.setProfileInstallResultForTest(
              chromeos.cellularSetup.mojom.ProfileInstallResult
                  .kErrorNeedsConfirmationCode);

          await navigateForwardForInstall(
              activationCodePage,
              /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

          // Confirmation code page should be showing.
          assertConfirmationCodePage(
              /*forwardButtonShouldBeEnabled*/ false,
              /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
          assertFocusDefaultButtonEventFired();
          confirmationCodePage.$$('#confirmationCode').value =
              'CONFIRMATION_CODE';
          assertFalse(focusDefaultButtonEventFired);

          // Simulate pressing 'Backward'.
          eSimPage.navigateBackward();
          await flushAsync();

          assertActivationCodePage(
              /*forwardButtonShouldBeEnabled*/ true,
              /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
          assertFocusDefaultButtonEventFired();
          assertEquals(
              activationCodePage.$$('#activationCode').value,
              'ACTIVATION_CODE');

          eSimPage.navigateBackward();
          await flushAsync();

          assertProfileDiscoveryPage();
          assertFocusDefaultButtonEventFired();
          assertEquals(
              eSimPage.forwardButtonLabel, 'Skip & Set up new profile');

          endFlowAndVerifyResult(
              ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
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
      await navigateForwardForInstall(
          profileDiscoveryPage,
          /*backButtonState*/ cellularSetup.ButtonState.HIDDEN);
    }

    test('Select profile flow', async function() {
      await selectProfile();

      await flushAsync();
      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test('Select profile with valid confirmation code flow', async function() {
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      const profileList = await availableEuiccs.euiccs[0].getProfileList();
      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode);

      await selectProfile();

      // Confirmation code page should be showing.
      assertConfirmationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
      assertFocusDefaultButtonEventFired();

      profileList.profiles[0].setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess);
      await enterConfirmationCode(
          /*backButtonState*/ cellularSetup.ButtonState.ENABLED);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);
      assertFocusDefaultButtonEventFired();

      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
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
          assertConfirmationCodePage(
              /*forwardButtonShouldBeEnabled*/ false,
              /*backButtonState*/ cellularSetup.ButtonState.ENABLED);
          confirmationCodePage.$$('#confirmationCode').value =
              'CONFIRMATION_CODE';

          eSimPage.navigateBackward();
          await flushAsync();

          assertProfileDiscoveryPage();
          assertEquals(eSimPage.forwardButtonLabel, 'Next');

          endFlowAndVerifyResult(
              ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
        });


    test('End flow before installation attempted', async function() {
      await flushAsync();
      endFlowAndVerifyResult(ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
    });

    test('No available network before installation', async function() {
      takeWifiNetworkOffline();
      await flushAsync();

      endFlowAndVerifyResult(ESimSetupFlowResult.NO_NETWORK);
    });
  });

  test(
      'Show cellular disconnect warning if connected to pSIM network',
      async function() {
        const pSimNetwork = OncMojo.getDefaultNetworkState(
            chromeos.networkConfig.mojom.NetworkType.kCellular, 'cellular');
        pSimNetwork.connectionState =
            chromeos.networkConfig.mojom.ConnectionStateType.kConnected;
        networkConfigRemote.addNetworksForTest([pSimNetwork]);
        network_config.MojoInterfaceProviderImpl.getInstance().remote_ =
            networkConfigRemote;
        await flushAsync();

        assertEquals(
            profileLoadingPage.state,
            LoadingPageState.CELLULAR_DISCONNECT_WARNING);

        // Disconnect from the network.
        networkConfigRemote.removeNetworkForTest(pSimNetwork);
        await flushAsync();

        // The warning should still be showing.
        assertEquals(
            profileLoadingPage.state,
            LoadingPageState.CELLULAR_DISCONNECT_WARNING);
      });

  test('Show final page with error if no EUICC', async function() {
    eSimPage.initSubflow();
    await assertProfileLoadingPageAndContinue();
    await flushAsync();
    await assertFinalPageAndPressDoneButton(/*shouldBeShowingError=*/ true);

    endFlowAndVerifyResult(ESimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });
});
