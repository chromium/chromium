// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';

import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {ESimPageName, ESimSetupFlowResult, FAILED_ESIM_SETUP_DURATION_METRIC_NAME, SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ESimOperationResult, ProfileInstallResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.js';
import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import {FakeESimManagerRemote} from './fake_esim_manager_remote.js';
import {MockMetricsPrivate} from './mock_metrics_private.js';

const suiteSuffix = 'smdsSupportEnabled';

suite(`CrComponentsEsimFlowUiTest${suiteSuffix}`, function() {
  /** @type {string} */
  const ACTIVATION_CODE_VALID = 'LPA:1$ACTIVATION_CODE';

  let eSimPage;
  let eSimManagerRemote;
  let ironPages;
  let profileLoadingPage;
  let profileDiscoveryConsentPage;
  let profileDiscoveryPage;
  let activationCodePage;
  let confirmationCodePage;
  let finalPage;
  let networkConfigRemote;

  let focusDefaultButtonEventFired = false;
  const wifiGuidPrefix = 'wifi';

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /** @param {ESimSetupFlowResult} eSimSetupFlowResult */
  function endFlowAndVerifyResult(eSimSetupFlowResult) {
    eSimPage.remove();
    flush();
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
    const onlineNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, wifiGuidPrefix);
    onlineNetwork.connectionState = ConnectionStateType.kOnline;
    networkConfigRemote.addNetworksForTest([onlineNetwork]);
    MojoInterfaceProviderImpl.getInstance().remote_ = networkConfigRemote;
  }

  /** Takes actively online network offline. */
  function takeWifiNetworkOffline() {
    networkConfigRemote.setNetworkConnectionStateForTest(
        wifiGuidPrefix + '_guid', ConnectionStateType.kNotConnected);
  }

  function setSmdsSupportEnabled(value) {
    loadTimeData.overrideValues({
      'isSmdsSupportEnabled': value,
    });
  }

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();

    addOnlineWifiNetwork();

    chrome.metricsPrivate = new MockMetricsPrivate();
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    document.addEventListener('focus-default-button', () => {
      focusDefaultButtonEventFired = true;
    });

    setSmdsSupportEnabled(true);

    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(eSimPage);
    flush();

    ironPages = eSimPage.$$('iron-pages');
    profileLoadingPage = eSimPage.$$('#profileLoadingPage');
    profileDiscoveryConsentPage = eSimPage.$$('#profileDiscoveryConsentPage');
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
    assertTrue(!!profileDiscoveryConsentPage);
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
   * Simulates navigating forward to trigger a profile install. Asserts that the
   * button_bar and page state is enabled before navigating forward. Asserts
   * that the button_bar and page state is disabled during the install.
   * @param {HTMLElement} page
   */
  async function navigateForwardForInstall(page) {
    assertEquals(eSimPage.buttonState.forward, ButtonState.ENABLED);
    assertEquals(eSimPage.buttonState.backward, ButtonState.HIDDEN);

    eSimPage.navigateForward();

    assertEquals(eSimPage.buttonState.forward, ButtonState.DISABLED);
    assertEquals(eSimPage.buttonState.backward, ButtonState.HIDDEN);
    assertEquals(eSimPage.buttonState.cancel, ButtonState.DISABLED);

    if (page !== profileLoadingPage && page !== profileDiscoveryConsentPage &&
        page !== finalPage) {
      assertEquals(
          ESimPageName.PROFILE_INSTALLING, eSimPage.selectedESimPageName_);
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
    assertSelectedPage(ESimPageName.FINAL, finalPage);
    assertEquals(!!finalPage.$$('.error'), shouldBeShowingError);
    assertEquals(ButtonState.ENABLED, eSimPage.buttonState.forward);
    assertEquals(ButtonState.HIDDEN, eSimPage.buttonState.backward);
    assertEquals(ButtonState.HIDDEN, eSimPage.buttonState.cancel);
    assertEquals(eSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    eSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    eSimPage.navigateForward();

    await flushAsync();
    assertTrue(exitCellularSetupEventFired);
  }

  /**
   * @param {boolean} forwardButtonShouldBeEnabled
   * @param {ButtonState} backButtonState
   */
  function assertButtonState(forwardButtonShouldBeEnabled) {
    const buttonState = eSimPage.buttonState;
    assertEquals(buttonState.backward, ButtonState.HIDDEN);
    assertEquals(buttonState.cancel, ButtonState.ENABLED);
    assertEquals(
        buttonState.forward,
        forwardButtonShouldBeEnabled ? ButtonState.ENABLED :
                                       ButtonState.DISABLED);
  }

  function assertFocusDefaultButtonEventFired() {
    assertTrue(focusDefaultButtonEventFired);
    focusDefaultButtonEventFired = false;
  }

  async function assertProfileDiscoveryConsentPageAndContinue() {
    assertSelectedPage(
        ESimPageName.PROFILE_DISCOVERY_CONSENT, profileDiscoveryConsentPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ true,
        /*backButtonState=*/ ButtonState.HIDDEN);

    // When the user clicks the "scan" button, they consent to profile
    // discovery. Navigate forward to the next page.
    eSimPage.navigateForward();
    await flushAsync();
  }

  async function assertProfileDiscoveryConsentPageAndContinueManually() {
    assertSelectedPage(
        ESimPageName.PROFILE_DISCOVERY_CONSENT, profileDiscoveryConsentPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ true);

    // When the user clicks the "manually" link, they opt out of profile
    // discovery.
    profileDiscoveryConsentPage.$$('#shouldSkipDiscovery')
        .shadowRoot.querySelector('a')
        .click();
    await flushAsync();
  }

  async function assertProfileLoadingPageAndContinue() {
    assertSelectedPage(ESimPageName.PROFILE_LOADING, profileLoadingPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ false);
    assertEquals(eSimPage.header, eSimPage.i18n('profileLoadingPageTitle'));
    assertEquals(
        profileLoadingPage.loadingMessage,
        eSimPage.i18n('profileLoadingPageMessage'));
    await flushAsync();
  }

  function assertProfileDiscoveryPage() {
    assertSelectedPage(ESimPageName.PROFILE_DISCOVERY, profileDiscoveryPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled*/ true);
    assertEquals(eSimPage.header, eSimPage.i18n('profileDiscoveryPageTitle'));
  }

  function assertActivationCodePage(
      forwardButtonShouldBeEnabled, backButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(activationCodePage.$$('#activationCode').value, '');
    }
    assertSelectedPage(ESimPageName.ACTIVATION_CODE, activationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
  }

  function assertConfirmationCodePage(
      forwardButtonShouldBeEnabled, backButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(confirmationCodePage.$$('#confirmationCode').value, '');
    }
    assertSelectedPage(ESimPageName.CONFIRMATION_CODE, confirmationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
    assertEquals(eSimPage.header, eSimPage.i18n('confimationCodePageTitle'));
  }

  test('Error fetching profiles', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    const euicc = availableEuiccs.euiccs[0];

    euicc.setRequestPendingProfilesResult(ESimOperationResult.kFailure);
    eSimPage.initSubflow();
    await assertProfileDiscoveryConsentPageAndContinue();

    await flushAsync();
    endFlowAndVerifyResult(ESimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });

  [true, false].forEach(isManualSetup => {
    const suiteManualSetupSuffix =
        isManualSetup ? 'isManualSetup' : 'isNotManualSetup';
    suite(
        `Add eSIM flow with zero pending profiles ${suiteManualSetupSuffix}`,
        function() {
          let euicc;
          setup(async function() {
            eSimManagerRemote.addEuiccForTest(0);
            const availableEuiccs =
                await eSimManagerRemote.getAvailableEuiccs();
            euicc = availableEuiccs.euiccs[0];

            await flushAsync();
            eSimPage.initSubflow();

            if (isManualSetup) {
              await assertProfileDiscoveryConsentPageAndContinueManually();
            } else {
              await assertProfileDiscoveryConsentPageAndContinue();
              // await assertProfileLoadingPageAndContinue();
            }

            // Should now be at the activation code page.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.HIDDEN);
            // Insert an activation code.
            activationCodePage.$$('#activationCode').value =
                ACTIVATION_CODE_VALID;
            // Forward button should now be enabled.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.HIDDEN);
          });

          test('Invalid activation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorInvalidActivationCode);

            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.HIDDEN);

            // Install should fail and still be at activation code page.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.HIDDEN);
            assertTrue(activationCodePage.showError);

            endFlowAndVerifyResult(
                ESimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE);
          });

          test('Valid activation code', async function() {
            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.HIDDEN);

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);

            endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
          });

          test('Valid confirmation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.HIDDEN);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kSuccess);
            await enterConfirmationCode(
                /*backButtonState*/ ButtonState.ENABLED);

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);

            endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
          });

          test('Invalid confirmation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.HIDDEN);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kFailure);
            const confirmationCodeInput = await enterConfirmationCode(
                /*backButtonState*/ ButtonState.ENABLED);

            // Should still be at confirmation code page with input showing
            // error.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.ENABLED);
            assertTrue(confirmationCodeInput.invalid);

            endFlowAndVerifyResult(ESimSetupFlowResult.INSTALL_FAIL);
          });

          test('Navigate backwards from confirmation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.HIDDEN);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            confirmationCodePage.$$('#confirmationCode').value =
                'CONFIRMATION_CODE';

            eSimPage.navigateBackward();
            await flushAsync();

            // Should now be at the activation code page.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.HIDDEN);
            assertEquals(
                activationCodePage.$$('#activationCode').value,
                ACTIVATION_CODE_VALID);

            endFlowAndVerifyResult(
                ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
          });

          test('End flow before installation attempted', async function() {
            await flushAsync();
            if (isManualSetup) {
              endFlowAndVerifyResult(
                  ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
            } else {
              endFlowAndVerifyResult(ESimSetupFlowResult.CANCELLED_NO_PROFILES);
            }
          });

          test('No available network before installation', async function() {
            takeWifiNetworkOffline();
            await flushAsync();

            endFlowAndVerifyResult(ESimSetupFlowResult.NO_NETWORK);
          });
        });
  });

  suite('add eSIM flow with pending profiles', function() {
    let euicc;

    async function setupWithProfiles(profileCount) {
      assertGT(profileCount, 0);

      eSimManagerRemote.addEuiccForTest(profileCount);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0];
      eSimPage.initSubflow();

      assertFocusDefaultButtonEventFired();
      await assertProfileDiscoveryConsentPageAndContinue();

      // Should go to profile discovery page.
      assertProfileDiscoveryPage();
      assertFocusDefaultButtonEventFired();
    }

    function skipDiscovery() {
      // Simulate pressing 'Skip'.
      assertTrue(eSimPage.buttonState.forward === ButtonState.ENABLED);
      eSimPage.navigateForward();
      flush();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.ENABLED);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = ACTIVATION_CODE_VALID;
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.ENABLED);
    }

    function skipProfileList() {
      profileDiscoveryPage.$$('#profileListMessage')
          .shadowRoot.querySelector('a')
          .click();

      flushAsync();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.ENABLED);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      activationCodePage.$$('#activationCode').value = ACTIVATION_CODE_VALID;
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.ENABLED);
    }

    [1, 2].forEach(profileCount => {
      test(`Skip discovery flow (${profileCount} profiles)`, async function() {
        await setupWithProfiles(profileCount);

        skipDiscovery();

        await navigateForwardForInstall(
            activationCodePage,
            /*backButtonState*/ ButtonState.ENABLED);

        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
      });
    });

    test('Skip profile list manually', async function() {
      await setupWithProfiles(1);
      skipProfileList();
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.ENABLED);

      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);
      endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
    });

    test(
        'Skip profile list manually, after profile selection',
        async function() {
          await setupWithProfiles(1);

          const getProfilesList = () =>
              profileDiscoveryPage.shadowRoot.querySelector('iron-list');

          assertTrue(!!getProfilesList());
          assertEquals(getProfilesList().items.length, 1);
          assertFalse(!!getProfilesList().selectedItem);

          // Select a profile.
          getProfilesList()
              .querySelector('profile-discovery-list-item')
              .click();

          await flushAsync();
          assertTrue(!!getProfilesList().selectedItem);

          skipProfileList();
          await navigateForwardForInstall(
              activationCodePage,
              /*backButtonState*/ ButtonState.ENABLED);

          // Should now be at the final page.
          await assertFinalPageAndPressDoneButton(false);
          endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
        });

    [1, 2].forEach(profileCount => {
      test(
          'Navigate backwards from skip discovery flow with confirmation code',
          async function() {
            await setupWithProfiles(profileCount);

            skipDiscovery();

            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.ENABLED);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();
            confirmationCodePage.$$('#confirmationCode').value =
                'CONFIRMATION_CODE';
            assertFalse(focusDefaultButtonEventFired);

            // Simulate pressing 'Backward'.
            eSimPage.navigateBackward();
            await flushAsync();

            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();
            assertEquals(
                activationCodePage.$$('#activationCode').value,
                ACTIVATION_CODE_VALID);

            eSimPage.navigateBackward();
            await flushAsync();

            assertProfileDiscoveryPage();
            assertFocusDefaultButtonEventFired();
            assertEquals(
                eSimPage.forwardButtonLabel, 'Skip & Set up new profile');

            endFlowAndVerifyResult(
                ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
          });
    });

    async function selectProfile() {
      // Select the first profile on the list.
      const profileList = profileDiscoveryPage.$$('#profileList');
      profileList.selectItem(profileList.items[0]);
      flush();

      // The 'Forward' button should now be enabled.
      assertTrue(eSimPage.buttonState.forward === ButtonState.ENABLED);

      // Simulate pressing 'Forward'.
      await navigateForwardForInstall(
          profileDiscoveryPage,
          /*backButtonState*/ ButtonState.HIDDEN);
    }

    [1, 2].forEach(profileCount => {
      test('Select profile flow', async function() {
        await setupWithProfiles(profileCount);

        await selectProfile();

        await flushAsync();
        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
      });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Select profile with valid confirmation code flow', async function() {
            await setupWithProfiles(profileCount);

            const availableEuiccs =
                await eSimManagerRemote.getAvailableEuiccs();
            const euicc = availableEuiccs.euiccs[0];
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await selectProfile();

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kSuccess);
            await enterConfirmationCode(
                /*backButtonState*/ ButtonState.ENABLED);

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);
            assertFocusDefaultButtonEventFired();

            endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
          });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Navigate backwards from select profile with confirmation code flow',
          async function() {
            await setupWithProfiles(profileCount);

            const availableEuiccs =
                await eSimManagerRemote.getAvailableEuiccs();
            const euicc = availableEuiccs.euiccs[0];
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await selectProfile();
            await flushAsync();

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            confirmationCodePage.$$('#confirmationCode').value =
                'CONFIRMATION_CODE';

            eSimPage.navigateBackward();
            await flushAsync();

            assertProfileDiscoveryPage();
            assertEquals(eSimPage.forwardButtonLabel, 'Next');

            endFlowAndVerifyResult(
                ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
          });
    });

    [1, 2].forEach(profileCount => {
      test('End flow before installation attempted', async function() {
        await setupWithProfiles(profileCount);

        await flushAsync();
        endFlowAndVerifyResult(ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
      });
    });

    [1, 2].forEach(profileCount => {
      test('No available network before installation', async function() {
        await setupWithProfiles(profileCount);

        takeWifiNetworkOffline();
        await flushAsync();

        endFlowAndVerifyResult(ESimSetupFlowResult.NO_NETWORK);
      });
    });

    [1, 2].forEach(profileCount => {
      test('No available network after installation', async function() {
        await setupWithProfiles(profileCount);

        await selectProfile();
        await flushAsync();
        // Right after installation, internet connection is lost and the
        // dialog closes, NO_NETWORK should not be reported.
        takeWifiNetworkOffline();
        await flushAsync();

        endFlowAndVerifyResult(ESimSetupFlowResult.SUCCESS);
      });
    });
  });

  test('Show final page with error if no EUICC', async function() {
    eSimPage.initSubflow();
    await assertProfileDiscoveryConsentPageAndContinue();
    await flushAsync();
    await assertFinalPageAndPressDoneButton(/*shouldBeShowingError=*/ true);

    endFlowAndVerifyResult(ESimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });
});
