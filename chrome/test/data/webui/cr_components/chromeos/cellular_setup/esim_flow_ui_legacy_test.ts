// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';

import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import type {EsimFlowUiElement} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import {EsimPageName, EsimSetupFlowResult, FAILED_ESIM_SETUP_DURATION_METRIC_NAME, SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import type {ProfileDiscoveryListPageLegacyElement} from 'chrome://resources/ash/common/cellular_setup/profile_discovery_list_page_legacy.js';
import type {ActivationCodePageElement} from 'chrome://resources/ash/common/cellular_setup/activation_code_page.js';
import type {ConfirmationCodePageLegacyElement} from 'chrome://resources/ash/common/cellular_setup/confirmation_code_page_legacy.js';
import type {FinalPageElement} from 'chrome://resources/ash/common/cellular_setup/final_page.js';
import type {SetupLoadingPageElement} from 'chrome://resources/ash/common/cellular_setup/setup_loading_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ESimOperationResult, ProfileInstallResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {assertEquals, assertTrue, assertFalse, assertGT} from 'chrome://webui-test/chai_assert.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import type {IronPagesElement} from '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.js';
import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import {FakeESimManagerRemote} from './fake_esim_manager_remote.js';
import type {FakeEuicc, FakeProfile} from './fake_esim_manager_remote.js';
import {MockMetricsPrivate} from './mock_metrics_private.js';

type ActivationOrConfirmationPage = ActivationCodePageElement|ConfirmationCodePageLegacyElement;

const suiteSuffix = 'smdsSupportDisabled';

suite(`CrComponentsEsimFlowUiTest${suiteSuffix}`, function() {
  const ACTIVATION_CODE_VALID = 'LPA:1$ACTIVATION_CODE';

  let eSimPage: EsimFlowUiElement;
  let eSimManagerRemote: FakeESimManagerRemote;
  let ironPages: IronPagesElement|null;
  let profileLoadingPage: SetupLoadingPageElement|null;
  let profileDiscoveryPageLegacy: ProfileDiscoveryListPageLegacyElement|null;
  let activationCodePage: ActivationCodePageElement|null;
  let confirmationCodePageLegacy: ConfirmationCodePageLegacyElement|null;
  let finalPage: FinalPageElement|null;
  let networkConfigRemote: FakeNetworkConfig;
  let metrics: MockMetricsPrivate;
  let focusDefaultButtonEventFired = false;
  const wifiGuidPrefix = 'wifi';

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function endFlowAndVerifyResult(esimSetupFlowResult: EsimSetupFlowResult) {
    eSimPage.remove();
    flush();
    assertEquals(
        metrics.getHistogramEnumValueCount(esimSetupFlowResult),
        1);

    if (esimSetupFlowResult === EsimSetupFlowResult.SUCCESS) {
      assertEquals(
          metrics.getHistogramCount(
              FAILED_ESIM_SETUP_DURATION_METRIC_NAME),
          0);
      assertEquals(
          metrics.getHistogramCount(
              SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
          1);
      return;
    }

    assertEquals(
        metrics.getHistogramCount(
            FAILED_ESIM_SETUP_DURATION_METRIC_NAME),
        1);
    assertEquals(
        metrics.getHistogramCount(
            SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
        0);
  }

  /** Adds an actively online wifi network and esim network. */
  function addOnlineWifiNetwork() {
    const onlineNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, wifiGuidPrefix);
    onlineNetwork.connectionState = ConnectionStateType.kOnline;
    networkConfigRemote.addNetworksForTest([onlineNetwork]);
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigRemote);
  }

  /** Takes actively online network offline. */
  function takeWifiNetworkOffline() {
    networkConfigRemote.setNetworkConnectionStateForTest(
        wifiGuidPrefix + '_guid', ConnectionStateType.kNotConnected);
  }

  test('Dialog refreshes installed profiles when opened', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    assertTrue(!!availableEuiccs.euiccs[0]);
    const euicc: FakeEuicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;
    assertTrue(!!euicc);
    assertEquals(0, euicc.getRefreshInstalledProfilesCount());

    eSimPage.initSubflow();
    await flushAsync();
    assertEquals(1, euicc.getRefreshInstalledProfilesCount());
  });

  test('Error fetching profiles', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    const euicc: FakeEuicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;

    euicc.setRequestPendingProfilesResult(ESimOperationResult.kFailure);
    eSimPage.initSubflow();

    await flushAsync();
    endFlowAndVerifyResult(EsimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();

    addOnlineWifiNetwork();

    metrics = new MockMetricsPrivate();
    chrome.metricsPrivate =
        metrics as unknown as typeof chrome.metricsPrivate;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    document.addEventListener('focus-default-button', () => {
      focusDefaultButtonEventFired = true;
    });

    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(eSimPage);
    flush();

    ironPages = eSimPage.shadowRoot!.querySelector('iron-pages');
    profileLoadingPage =
        eSimPage.shadowRoot!.querySelector('#profileLoadingPage');
    profileDiscoveryPageLegacy =
        eSimPage.shadowRoot!.querySelector('#profileDiscoveryPageLegacy');
    activationCodePage =
        eSimPage.shadowRoot!.querySelector('#activationCodePage');
    confirmationCodePageLegacy =
        eSimPage.shadowRoot!.querySelector('#confirmationCodePageLegacy');
    finalPage = eSimPage.shadowRoot!.querySelector('#finalPage');

    // Captures the function that is called every time the interval timer
    // timeouts.
    const setIntervalFunction = (_fn: Function, _milliseconds: number) => {
      return 1;
    };

    // In tests, pausing the video can have race conditions with previous
    // requests to play the video due to the speed of execution. Avoid this by
    // mocking the play and pause actions.
    const playVideoFunction = () => {};
    const stopStreamFunction = (_stream: MediaStream) => {};

    assertTrue(!!profileLoadingPage);
    assertTrue(!!profileDiscoveryPageLegacy);
    assertTrue(!!activationCodePage);
    assertTrue(!!confirmationCodePageLegacy);
    assertTrue(!!finalPage);

    await activationCodePage.setFakesForTesting(
        FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
        playVideoFunction, stopStreamFunction);
  });

  suiteSetup(() => {
    loadTimeData.overrideValues({isSmdsSupportEnabled: false});
  });

  function assertSelectedPage(pageName: EsimPageName, page: HTMLElement|null) {
    assertTrue(!!ironPages);
    assertTrue(!!page);
    assertEquals(ironPages.selected, pageName);
    assertEquals(ironPages.selected, page.id);
  }

  /**
   * Simulates navigating forward to trigger a profile install.
   * Asserts that the button_bar and page state is enabled and not busy before
   * navigating forward. Asserts that the button_bar and page state is
   * disabled and busy during the install.
   */
  async function navigateForwardForInstall(page: HTMLElement|null, previousBackButtonState: ButtonState) {
    assertTrue(!!page);
    const checkShowBusyState =
        (page !== profileDiscoveryPageLegacy && page !== finalPage);
    assertEquals(eSimPage.buttonState.forward, ButtonState.ENABLED);
    assertEquals(eSimPage.buttonState.backward, previousBackButtonState);
    if (checkShowBusyState) {
      assertFalse((page as ActivationOrConfirmationPage).showBusy);
    }

    // If back button is hidden before installation began, the new back button
    // state should also be hidden, if it was enabled new back button state
    // should be disabled while installation is taking place.
    let newBackButtonState = ButtonState.HIDDEN;
    if (previousBackButtonState === ButtonState.ENABLED) {
      newBackButtonState = ButtonState.DISABLED;
    }
    eSimPage.navigateForward();

    assertEquals(eSimPage.buttonState.forward, ButtonState.DISABLED);
    assertEquals(eSimPage.buttonState.cancel, ButtonState.DISABLED);
    assertEquals(eSimPage.buttonState.backward, newBackButtonState);
    if (checkShowBusyState) {
      assertTrue((page as ActivationOrConfirmationPage).showBusy);
    }

    await flushAsync();
  }

  function getActivationCodeInput(activationCodePage: ActivationCodePageElement|null)
      : CrInputElement {
    assertTrue(!!activationCodePage);
    const activationCode =
        activationCodePage.shadowRoot!.querySelector<CrInputElement>('#activationCode');
    assertTrue(!!activationCode);
    return activationCode;
  }

  function getConfirmationCodeInput(confirmationCodePageLegacy: ConfirmationCodePageLegacyElement|null)
      : CrInputElement {
    assertTrue(!!confirmationCodePageLegacy);
    const confirmationCode =
        confirmationCodePageLegacy.shadowRoot!.querySelector<CrInputElement>('#confirmationCode');
    assertTrue(!!confirmationCode);
    return confirmationCode;
  }

  async function enterConfirmationCode(backButtonState: ButtonState) {
    const confirmationCodeInput = getConfirmationCodeInput(confirmationCodePageLegacy);
    confirmationCodeInput.value = 'CONFIRMATION_CODE';
    assertFalse(confirmationCodeInput.invalid);

    // Forward button should now be enabled.
    assertConfirmationCodePageLegacy(
        /*forwardButtonShouldBeEnabled=*/ true,
        /*backButtonState*/ backButtonState);

    assertTrue(!!confirmationCodePageLegacy);
    await navigateForwardForInstall(
        confirmationCodePageLegacy, /*backButtonState*/ backButtonState);
    return confirmationCodeInput;
  }

  async function assertFinalPageAndPressDoneButton(shouldBeShowingError: boolean) {
    assertSelectedPage(EsimPageName.FINAL, finalPage);
    assertEquals(
        !!finalPage!.shadowRoot!.querySelector('.error'), shouldBeShowingError);
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

  function assertButtonState(forwardButtonShouldBeEnabled: boolean, backButtonState: ButtonState) {
    const buttonState = eSimPage.buttonState;
    assertEquals(buttonState.backward, backButtonState);
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

  async function assertProfileLoadingPageAndContinue() {
    assertSelectedPage(EsimPageName.PROFILE_LOADING, profileLoadingPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ false,
        /*backButtonState=*/ ButtonState.HIDDEN);
    await flushAsync();
  }

  function assertProfileDiscoveryPageLegacy() {
    assertSelectedPage(
        EsimPageName.PROFILE_DISCOVERY_LEGACY, profileDiscoveryPageLegacy);
    assertButtonState(
        /*forwardButtonShouldBeEnabled*/ true,
        /*backButtonState*/ ButtonState.HIDDEN);
  }

  function assertActivationCodePage(
      forwardButtonShouldBeEnabled: boolean, backButtonState: ButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(getActivationCodeInput(activationCodePage).value, '');
    }
    assertSelectedPage(EsimPageName.ACTIVATION_CODE, activationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled, backButtonState);
  }

  function assertConfirmationCodePageLegacy(
      forwardButtonShouldBeEnabled: boolean, backButtonState: ButtonState) {
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(getConfirmationCodeInput(confirmationCodePageLegacy).value, '');
    }
    assertSelectedPage(
        EsimPageName.CONFIRMATION_CODE_LEGACY, confirmationCodePageLegacy);
    assertButtonState(forwardButtonShouldBeEnabled, backButtonState);
  }

  suite('Add eSIM flow with zero pending profiles', function() {
    let euicc: FakeEuicc;

    setup(async function() {
      eSimManagerRemote.addEuiccForTest(0);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;

      await flushAsync();
      eSimPage.initSubflow();

      await assertProfileLoadingPageAndContinue();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.HIDDEN);
      // Insert an activation code.
      const activationCode = getActivationCodeInput(activationCodePage);
      activationCode.value = ACTIVATION_CODE_VALID;

      // Forward button should now be enabled.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.HIDDEN);
    });

    test('Invalid activation code', async function() {
      euicc.setProfileInstallResultForTest(
          ProfileInstallResult.kErrorInvalidActivationCode);

      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.HIDDEN);

      // Install should fail and still be at activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.HIDDEN);
      assertTrue(activationCodePage.showError);

      endFlowAndVerifyResult(
          EsimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE);
    });

    test('Valid activation code', async function() {
      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.HIDDEN);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
    });

    test('Valid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          ProfileInstallResult.kErrorNeedsConfirmationCode);

      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePageLegacy(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.ENABLED);

      euicc.setProfileInstallResultForTest(ProfileInstallResult.kSuccess);
      await enterConfirmationCode(
          /*backButtonState*/ ButtonState.ENABLED);

      // Should go to final page.
      await assertFinalPageAndPressDoneButton(false);

      endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
    });

    test('Invalid confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          ProfileInstallResult.kErrorNeedsConfirmationCode);

      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePageLegacy(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.ENABLED);

      euicc.setProfileInstallResultForTest(ProfileInstallResult.kFailure);
      const confirmationCodeInput = await enterConfirmationCode(
          /*backButtonState*/ ButtonState.ENABLED);

      // Should still be at confirmation code page with input showing error.
      assertConfirmationCodePageLegacy(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.ENABLED);
      assertTrue(confirmationCodeInput.invalid);

      endFlowAndVerifyResult(EsimSetupFlowResult.INSTALL_FAIL);
    });

    test('Navigate backwards from confirmation code', async function() {
      euicc.setProfileInstallResultForTest(
          ProfileInstallResult.kErrorNeedsConfirmationCode);

      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(
          activationCodePage,
          /*backButtonState*/ ButtonState.HIDDEN);

      // Confirmation code page should be showing.
      assertConfirmationCodePageLegacy(
          /*forwardButtonShouldBeEnabled*/ false,
          /*backButtonState*/ ButtonState.ENABLED);
      const confirmationCode = getConfirmationCodeInput(confirmationCodePageLegacy);
      confirmationCode.value = 'CONFIRMATION_CODE';

      eSimPage.navigateBackward();
      await flushAsync();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.HIDDEN);
      assertEquals(getActivationCodeInput(activationCodePage).value,
                   ACTIVATION_CODE_VALID);

      endFlowAndVerifyResult(
          EsimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
    });

    test('End flow before installation attempted', async function() {
      await flushAsync();
      endFlowAndVerifyResult(EsimSetupFlowResult.CANCELLED_NO_PROFILES);
    });

    test('No available network before installation', async function() {
      takeWifiNetworkOffline();
      await flushAsync();

      endFlowAndVerifyResult(EsimSetupFlowResult.NO_NETWORK);
    });
  });

  suite('add eSIM flow with pending profiles', function() {
    let euicc: FakeEuicc;

    async function setupWithProfiles(profileCount: number) {
      assertGT(profileCount, 0);

      eSimManagerRemote.addEuiccForTest(profileCount);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      euicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;
      eSimPage.initSubflow();

      assertFocusDefaultButtonEventFired();
      await assertProfileLoadingPageAndContinue();

      // Should go to profile discovery page.
      assertProfileDiscoveryPageLegacy();
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
      const activationCode = getActivationCodeInput(activationCodePage);
      activationCode.value = ACTIVATION_CODE_VALID;
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true,
          /*backButtonState*/ ButtonState.ENABLED);
    }

    [1, 2].forEach(profileCount => {
      test(`Skip discovery flow (${profileCount} profiles)`, async function() {
        await setupWithProfiles(profileCount);

        skipDiscovery();

        assertTrue(!!activationCodePage);
        await navigateForwardForInstall(
            activationCodePage,
            /*backButtonState*/ ButtonState.ENABLED);

        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Navigate backwards from skip discovery flow with confirmation code',
          async function() {
            await setupWithProfiles(profileCount);

            skipDiscovery();

            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            assertTrue(!!activationCodePage);
            await navigateForwardForInstall(
                activationCodePage,
                /*backButtonState*/ ButtonState.ENABLED);

            // Confirmation code page should be showing.
            assertConfirmationCodePageLegacy(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();
            const confirmationCode = getConfirmationCodeInput(confirmationCodePageLegacy);
            confirmationCode.value = 'CONFIRMATION_CODE';
            assertFalse(focusDefaultButtonEventFired);

            // Simulate pressing 'Backward'.
            eSimPage.navigateBackward();
            await flushAsync();

            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();
            assertEquals(getActivationCodeInput(activationCodePage).value,
                         ACTIVATION_CODE_VALID);

            eSimPage.navigateBackward();
            await flushAsync();

            assertProfileDiscoveryPageLegacy();
            assertFocusDefaultButtonEventFired();
            assertEquals(
                eSimPage.forwardButtonLabel, 'Skip & set up new profile');

            endFlowAndVerifyResult(
                EsimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
          });
    });

    async function selectProfile() {
      assertTrue(!!profileDiscoveryPageLegacy);
      // Select the first profile on the list.
      const profileList =
          profileDiscoveryPageLegacy.shadowRoot!.querySelector<IronListElement>('#profileList');
      assertTrue(!!profileList);
      assertTrue(!!profileList.items);
      profileList.selectItem(profileList.items[0]);
      flush();

      // The 'Forward' button should now be enabled.
      assertTrue(eSimPage.buttonState.forward === ButtonState.ENABLED);

      // Simulate pressing 'Forward'.
      await navigateForwardForInstall(
          profileDiscoveryPageLegacy,
          /*backButtonState*/ ButtonState.HIDDEN);
    }

    async function getFakeProfile(): Promise<FakeProfile> {
      const availableEuiccs =
          await eSimManagerRemote.getAvailableEuiccs();
      assertTrue(!!availableEuiccs.euiccs[0]);
      const profileList = await availableEuiccs.euiccs[0].getProfileList();
      assertTrue(!!profileList);
      return profileList.profiles[0] as unknown as FakeProfile;
    }

    [1, 2].forEach(profileCount => {
      test('Select profile flow', async function() {
        await setupWithProfiles(profileCount);

        await selectProfile();

        await flushAsync();
        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Select profile with valid confirmation code flow', async function() {
            await setupWithProfiles(profileCount);

            const profile = await getFakeProfile();
            profile.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await selectProfile();

            // Confirmation code page should be showing.
            assertConfirmationCodePageLegacy(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            assertFocusDefaultButtonEventFired();

            profile.setProfileInstallResultForTest(
                ProfileInstallResult.kSuccess);
            await enterConfirmationCode(
                /*backButtonState*/ ButtonState.ENABLED);

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);
            assertFocusDefaultButtonEventFired();

            endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
          });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Navigate backwards from select profile with confirmation code flow',
          async function() {
            await setupWithProfiles(profileCount);

            const profile = await getFakeProfile();
            profile.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await selectProfile();
            await flushAsync();

            // Confirmation code page should be showing.
            assertConfirmationCodePageLegacy(
                /*forwardButtonShouldBeEnabled*/ false,
                /*backButtonState*/ ButtonState.ENABLED);
            const confirmationCode = getConfirmationCodeInput(confirmationCodePageLegacy);
            confirmationCode.value = 'CONFIRMATION_CODE';

            eSimPage.navigateBackward();
            await flushAsync();

            assertProfileDiscoveryPageLegacy();
            assertEquals(eSimPage.forwardButtonLabel, 'Next');

            endFlowAndVerifyResult(
                EsimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE);
          });
    });

    [1, 2].forEach(profileCount => {
      test('End flow before installation attempted', async function() {
        await setupWithProfiles(profileCount);

        await flushAsync();
        endFlowAndVerifyResult(EsimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
      });
    });

    [1, 2].forEach(profileCount => {
      test('No available network before installation', async function() {
        await setupWithProfiles(profileCount);

        takeWifiNetworkOffline();
        await flushAsync();

        endFlowAndVerifyResult(EsimSetupFlowResult.NO_NETWORK);
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

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });
  });

  test(
      'Show cellular disconnect warning if connected to pSIM network',
      async function() {
        assertTrue(!!profileLoadingPage);
        assertEquals(
            profileLoadingPage.loadingMessage,
            eSimPage.i18n('eSimProfileDetectMessage'));

        const pSimNetwork =
            OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
        pSimNetwork.connectionState = ConnectionStateType.kConnected;
        networkConfigRemote.addNetworksForTest([pSimNetwork]);
        MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
            networkConfigRemote);
        await flushAsync();

        assertEquals(
            profileLoadingPage.loadingMessage,
            eSimPage.i18n(
                'eSimProfileDetectDuringActiveCellularConnectionMessage'));

        // Disconnect from the network.
        networkConfigRemote.removeNetworkForTest(pSimNetwork);
        await flushAsync();

        // The warning should still be showing.
        assertEquals(
            profileLoadingPage.loadingMessage,
            eSimPage.i18n(
                'eSimProfileDetectDuringActiveCellularConnectionMessage'));
      });

  test('Show final page with error if no EUICC', async function() {
    eSimPage.initSubflow();
    await assertProfileLoadingPageAndContinue();
    await flushAsync();
    await assertFinalPageAndPressDoneButton(/*shouldBeShowingError=*/ true);

    endFlowAndVerifyResult(EsimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });
});
