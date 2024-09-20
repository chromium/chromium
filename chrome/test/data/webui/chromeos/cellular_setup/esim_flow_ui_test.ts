// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';

import type {IronPagesElement} from '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import type {ActivationCodePageElement} from 'chrome://resources/ash/common/cellular_setup/activation_code_page.js';
import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import type {ConfirmationCodePageElement} from 'chrome://resources/ash/common/cellular_setup/confirmation_code_page.js';
import type {EsimFlowUiElement} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import {EsimPageName, EsimSetupFlowResult, FAILED_ESIM_SETUP_DURATION_METRIC_NAME, SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME} from 'chrome://resources/ash/common/cellular_setup/esim_flow_ui.js';
import type {FinalPageElement} from 'chrome://resources/ash/common/cellular_setup/final_page.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import type {ProfileDiscoveryConsentPageElement} from 'chrome://resources/ash/common/cellular_setup/profile_discovery_consent_page.js';
import type {ProfileDiscoveryListItemElement} from 'chrome://resources/ash/common/cellular_setup/profile_discovery_list_item.js';
import type {ProfileDiscoveryListPageElement} from 'chrome://resources/ash/common/cellular_setup/profile_discovery_list_page.js';
import type {SetupLoadingPageElement} from 'chrome://resources/ash/common/cellular_setup/setup_loading_page.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {ESimOperationResult, ProfileInstallResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {FakeBarcodeDetector, FakeImageCapture} from './fake_barcode_detector.js';
import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';
import type {FakeEuicc} from './fake_esim_manager_remote.js';
import {FakeESimManagerRemote} from './fake_esim_manager_remote.js';
import {MockMetricsPrivate} from './mock_metrics_private.js';


suite(`CrComponentsEsimFlowUiTest`, function() {
  const ACTIVATION_CODE_VALID = 'LPA:1$ACTIVATION_CODE';

  let eSimPage: EsimFlowUiElement;
  let eSimManagerRemote: FakeESimManagerRemote;
  let ironPages: IronPagesElement|null;
  let profileLoadingPage: SetupLoadingPageElement|null;
  let profileDiscoveryConsentPage: ProfileDiscoveryConsentPageElement|null;
  let profileDiscoveryPage: ProfileDiscoveryListPageElement|null;
  let activationCodePage: ActivationCodePageElement|null;
  let confirmationCodePage: ConfirmationCodePageElement|null;
  let finalPage: FinalPageElement|null;
  let networkConfigRemote: FakeNetworkConfig;
  let metrics: MockMetricsPrivate;
  let focusDefaultButtonEventFired = false;
  const wifiGuidPrefix = 'wifi';

  function endFlowAndVerifyResult(esimSetupFlowResult: EsimSetupFlowResult):
      void {
    eSimPage.remove();
    flushTasks();
    assertEquals(metrics.getHistogramEnumValueCount(esimSetupFlowResult), 1);

    if (esimSetupFlowResult === EsimSetupFlowResult.SUCCESS) {
      assertEquals(
          metrics.getHistogramCount(FAILED_ESIM_SETUP_DURATION_METRIC_NAME), 0);
      assertEquals(
          metrics.getHistogramCount(SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
          1);
      return;
    }

    assertEquals(
        metrics.getHistogramCount(FAILED_ESIM_SETUP_DURATION_METRIC_NAME), 1);
    assertEquals(
        metrics.getHistogramCount(SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME),
        0);
  }

  /** Adds an actively online wifi network and esim network. */
  function addOnlineWifiNetwork(): void {
    const onlineNetwork =
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, wifiGuidPrefix);
    onlineNetwork.connectionState = ConnectionStateType.kOnline;
    networkConfigRemote.addNetworksForTest([onlineNetwork]);
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        networkConfigRemote);
  }

  /** Takes actively online network offline. */
  function takeWifiNetworkOffline(): void {
    networkConfigRemote.setNetworkConnectionStateForTest(
        wifiGuidPrefix + '_guid', ConnectionStateType.kNotConnected);
  }

  setup(async function() {
    networkConfigRemote = new FakeNetworkConfig();

    addOnlineWifiNetwork();

    metrics = new MockMetricsPrivate();
    chrome.metricsPrivate = metrics as unknown as typeof chrome.metricsPrivate;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    document.addEventListener('focus-default-button', () => {
      focusDefaultButtonEventFired = true;
    });

    eSimPage = document.createElement('esim-flow-ui');
    eSimPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(eSimPage);
    await flushTasks();

    ironPages = eSimPage.shadowRoot!.querySelector('iron-pages');
    profileLoadingPage =
        eSimPage.shadowRoot!.querySelector('#profileLoadingPage');
    profileDiscoveryConsentPage =
        eSimPage.shadowRoot!.querySelector('#profileDiscoveryConsentPage');
    profileDiscoveryPage =
        eSimPage.shadowRoot!.querySelector('#profileDiscoveryPage');
    activationCodePage =
        eSimPage.shadowRoot!.querySelector('#activationCodePage');
    confirmationCodePage =
        eSimPage.shadowRoot!.querySelector('#confirmationCodePage');
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
    assertTrue(!!activationCodePage);
    assertTrue(!!profileLoadingPage);
    assertTrue(!!profileDiscoveryConsentPage);
    assertTrue(!!profileDiscoveryPage);
    assertTrue(!!confirmationCodePage);
    assertTrue(!!finalPage);

    await activationCodePage.setFakesForTesting(
        FakeBarcodeDetector, FakeImageCapture, setIntervalFunction,
        playVideoFunction);
  });

  function assertSelectedPage(pageName: EsimPageName, page: HTMLElement): void {
    assertTrue(!!ironPages);
    assertEquals(ironPages.selected, pageName);
    assertEquals(ironPages.selected, page.id);
  }

  /**
   * Simulates navigating forward to trigger a profile install. Asserts that the
   * button_bar and page state is enabled before navigating forward. Asserts
   * that the button_bar and page state is disabled during the install.
   */
  async function navigateForwardForInstall(page: HTMLElement) {
    assertEquals(eSimPage.buttonState.forward, ButtonState.ENABLED);

    eSimPage.navigateForward();

    assertEquals(eSimPage.buttonState.forward, ButtonState.DISABLED);
    assertEquals(eSimPage.buttonState.cancel, ButtonState.DISABLED);

    if (page !== profileLoadingPage && page !== profileDiscoveryConsentPage &&
        page !== finalPage) {
      assertEquals(
          EsimPageName.PROFILE_INSTALLING,
          eSimPage.getSelectedEsimPageNameForTest());
    }

    await flushTasks();
  }

  async function enterConfirmationCode() {
    const confirmationCodeInput =
        getConfirmationCodeInput(confirmationCodePage);
    confirmationCodeInput.value = 'CONFIRMATION_CODE';
    assertFalse(confirmationCodeInput.invalid);

    // Forward button should now be enabled.
    assertConfirmationCodePage(
        /*forwardButtonShouldBeEnabled=*/ true);

    assertTrue(!!confirmationCodePage);
    await navigateForwardForInstall(confirmationCodePage);
    return confirmationCodeInput;
  }

  async function assertFinalPageAndPressDoneButton(
      shouldBeShowingError: boolean) {
    assertTrue(!!finalPage);
    assertSelectedPage(EsimPageName.FINAL, finalPage);
    assertEquals(
        !!finalPage.shadowRoot!.querySelector('.error'), shouldBeShowingError);
    assertEquals(ButtonState.ENABLED, eSimPage.buttonState.forward);
    assertEquals(ButtonState.HIDDEN, eSimPage.buttonState.cancel);
    assertEquals(eSimPage.forwardButtonLabel, 'Done');
    let exitCellularSetupEventFired = false;
    eSimPage.addEventListener('exit-cellular-setup', () => {
      exitCellularSetupEventFired = true;
    });
    eSimPage.navigateForward();

    await flushTasks();
    assertTrue(exitCellularSetupEventFired);
  }

  function assertButtonState(forwardButtonShouldBeEnabled: boolean) {
    const buttonState = eSimPage.buttonState;
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
    assertTrue(!!profileDiscoveryConsentPage);
    assertSelectedPage(
        EsimPageName.PROFILE_DISCOVERY_CONSENT, profileDiscoveryConsentPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ true);

    // When the user clicks the "scan" button, they consent to profile
    // discovery. Navigate forward to the next page.
    eSimPage.navigateForward();
    await flushTasks();
  }

  async function assertProfileDiscoveryConsentPageAndContinueManually() {
    assertTrue(!!profileDiscoveryConsentPage);
    assertSelectedPage(
        EsimPageName.PROFILE_DISCOVERY_CONSENT, profileDiscoveryConsentPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled=*/ true);

    // When the user clicks the "manually" link, they opt out of profile
    // discovery.
    const localizedLink = profileDiscoveryConsentPage.shadowRoot!.querySelector(
        '#shouldSkipDiscovery');
    assertTrue(!!localizedLink);
    localizedLink.shadowRoot!.querySelector('a')!.click();
    await flushTasks();
  }

  function assertProfileDiscoveryPage() {
    assertTrue(!!profileDiscoveryPage);
    assertSelectedPage(EsimPageName.PROFILE_DISCOVERY, profileDiscoveryPage);
    assertButtonState(
        /*forwardButtonShouldBeEnabled*/ true);
    assertEquals(eSimPage.header, eSimPage.i18n('profileDiscoveryPageTitle'));
  }

  function assertActivationCodePage(forwardButtonShouldBeEnabled: boolean) {
    assertTrue(!!activationCodePage);
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(getActivationCodeInput(activationCodePage).value, '');
    }
    assertSelectedPage(EsimPageName.ACTIVATION_CODE, activationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
  }

  function assertConfirmationCodePage(forwardButtonShouldBeEnabled: boolean) {
    assertTrue(!!confirmationCodePage);
    if (!forwardButtonShouldBeEnabled) {
      // In the initial state, input should be cleared.
      assertEquals(getConfirmationCodeInput(confirmationCodePage).value, '');
    }
    assertSelectedPage(EsimPageName.CONFIRMATION_CODE, confirmationCodePage);
    assertButtonState(forwardButtonShouldBeEnabled);
    assertEquals(eSimPage.header, eSimPage.i18n('confimationCodePageTitle'));
  }

  function getActivationCodeInput(activationCodePage: ActivationCodePageElement|
                                  null): CrInputElement {
    assertTrue(!!activationCodePage);
    const activationCode =
        activationCodePage.shadowRoot!.querySelector<CrInputElement>(
            '#activationCode');
    assertTrue(!!activationCode);
    return activationCode;
  }

  function getConfirmationCodeInput(
      confirmationCodePage: ConfirmationCodePageElement|null): CrInputElement {
    assertTrue(!!confirmationCodePage);
    const confirmationCode =
        confirmationCodePage.shadowRoot!.querySelector<CrInputElement>(
            '#confirmationCode');
    assertTrue(!!confirmationCode);
    return confirmationCode;
  }

  test('Dialog refreshes installed profiles when opened', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    assertTrue(!!availableEuiccs.euiccs[0]);
    const euicc: FakeEuicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;
    assertTrue(!!euicc);
    assertEquals(0, euicc.getRefreshInstalledProfilesCount());

    eSimPage.initSubflow();
    await flushTasks();
    assertEquals(1, euicc.getRefreshInstalledProfilesCount());
  });

  test('Error fetching profiles', async function() {
    eSimManagerRemote.addEuiccForTest(0);
    const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
    assertTrue(!!availableEuiccs.euiccs[0]);
    const euicc: FakeEuicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;

    assertTrue(!!euicc);
    euicc.setRequestPendingProfilesResult(ESimOperationResult.kFailure);
    eSimPage.initSubflow();
    await assertProfileDiscoveryConsentPageAndContinue();

    await flushTasks();
    endFlowAndVerifyResult(EsimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });

  [true, false].forEach(isManualSetup => {
    const suiteManualSetupSuffix =
        isManualSetup ? 'isManualSetup' : 'isNotManualSetup';
    suite(
        `Add eSIM flow with zero pending profiles ${suiteManualSetupSuffix}`,
        function() {
          let euicc: FakeEuicc;

          setup(async function() {
            eSimManagerRemote.addEuiccForTest(0);
            const availableEuiccs =
                await eSimManagerRemote.getAvailableEuiccs();
            assertTrue(!!availableEuiccs.euiccs[0]);
            euicc = availableEuiccs.euiccs[0] as unknown as FakeEuicc;

            await flushTasks();
            eSimPage.initSubflow();

            if (isManualSetup) {
              await assertProfileDiscoveryConsentPageAndContinueManually();
            } else {
              await assertProfileDiscoveryConsentPageAndContinue();
            }

            // Should now be at the activation code page.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ false);
            // Insert an activation code.
            const activationCode = getActivationCodeInput(activationCodePage);
            activationCode.value = ACTIVATION_CODE_VALID;
            // Forward button should now be enabled.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true);
          });

          test('Invalid activation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorInvalidActivationCode);
            assertTrue(!!activationCodePage);
            await navigateForwardForInstall(activationCodePage);

            // Install should fail and still be at activation code page.
            assertActivationCodePage(
                /*forwardButtonShouldBeEnabled*/ true);
            assertTrue(activationCodePage.showError);

            endFlowAndVerifyResult(
                EsimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE);
          });

          test('Valid activation code', async function() {
            assertTrue(!!activationCodePage);
            await navigateForwardForInstall(activationCodePage);

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);

            endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
          });

          test('Valid confirmation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);
            assertTrue(!!activationCodePage);
            await navigateForwardForInstall(activationCodePage);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false);

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kSuccess);
            await enterConfirmationCode();

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);

            endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
          });

          test('Invalid confirmation code', async function() {
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);
            assertTrue(!!activationCodePage);
            await navigateForwardForInstall(activationCodePage);

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false);

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kFailure);
            const confirmationCodeInput = await enterConfirmationCode();

            // Should still be at confirmation code page with input showing
            // error.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ true);
            assertTrue(confirmationCodeInput.invalid);

            endFlowAndVerifyResult(EsimSetupFlowResult.INSTALL_FAIL);
          });

          test('End flow before installation attempted', async function() {
            await flushTasks();
            if (isManualSetup) {
              endFlowAndVerifyResult(
                  EsimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
            } else {
              endFlowAndVerifyResult(EsimSetupFlowResult.CANCELLED_NO_PROFILES);
            }
          });

          test('No available network before installation', async function() {
            takeWifiNetworkOffline();
            await flushTasks();

            endFlowAndVerifyResult(EsimSetupFlowResult.NO_NETWORK);
          });
        });
  });

  suite('add eSIM flow with pending profiles', function() {
    async function setupWithProfiles(profileCount: number) {
      assertGT(profileCount, 0);

      eSimManagerRemote.addEuiccForTest(profileCount);
      const availableEuiccs = await eSimManagerRemote.getAvailableEuiccs();
      assertTrue(!!availableEuiccs.euiccs[0]);
      eSimPage.initSubflow();

      assertFocusDefaultButtonEventFired();
      await flushTasks();
      await assertProfileDiscoveryConsentPageAndContinue();

      // Should go to profile discovery page.
      assertProfileDiscoveryPage();
      assertFocusDefaultButtonEventFired();
      await flushTasks();
      eSimPage.maybeFocusPageElement();
      await flushTasks();
      assertEquals(eSimPage.forwardButtonLabel, 'Next');
    }

    async function deselectCellularProfile() {
      assertTrue(!!profileDiscoveryPage);
      const profileList =
          profileDiscoveryPage.shadowRoot!.querySelector<IronListElement>(
              '#profileList');
      assertTrue(!!profileList);
      assertTrue(!!profileList.items);
      profileList.selectedItem = null;
      await flushTasks();
    }

    async function skipDiscovery() {
      // Simulate pressing 'Skip'.
      assertTrue(eSimPage.buttonState.forward === ButtonState.ENABLED);
      eSimPage.navigateForward();
      await flushTasks();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      const activationCode = getActivationCodeInput(activationCodePage);
      activationCode.value = ACTIVATION_CODE_VALID;
      assertFalse(focusDefaultButtonEventFired);

      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true);
    }

    async function skipProfileList() {
      assertTrue(!!profileDiscoveryPage);
      const localizedLink =
          profileDiscoveryPage.shadowRoot!.querySelector('#profileListMessage');
      assertTrue(!!localizedLink);
      localizedLink.shadowRoot!.querySelector('a')!.click();

      await flushTasks();

      // Should now be at the activation code page.
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ false);
      assertFocusDefaultButtonEventFired();

      // Insert an activation code.
      const activationCode = getActivationCodeInput(activationCodePage);
      activationCode.value = ACTIVATION_CODE_VALID;
      assertFalse(focusDefaultButtonEventFired);

      await flushTasks();
      assertActivationCodePage(
          /*forwardButtonShouldBeEnabled*/ true);
    }

    [1, 2].forEach(profileCount => {
      test(`Skip discovery flow (${profileCount} profiles)`, async function() {
        await setupWithProfiles(profileCount);
        await deselectCellularProfile();
        await skipDiscovery();
        assertTrue(!!activationCodePage);
        await navigateForwardForInstall(activationCodePage);

        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });

    test('Skip profile list manually', async function() {
      await setupWithProfiles(1);
      await deselectCellularProfile();
      await skipProfileList();
      assertTrue(!!activationCodePage);
      await navigateForwardForInstall(activationCodePage);

      // Should now be at the final page.
      await assertFinalPageAndPressDoneButton(false);
      endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
    });

    test(
        'Skip profile list manually, after profile selection',
        async function() {
          const profileCount = 2;
          await setupWithProfiles(profileCount);
          await flushTasks();

          const discoveryListItem =
              profileDiscoveryPage!.shadowRoot!.querySelectorAll(
                  'profile-discovery-list-item');

          assertTrue(!!discoveryListItem);
          assertEquals(discoveryListItem.length, profileCount);
          assertTrue((discoveryListItem[0] as
                      ProfileDiscoveryListItemElement)!.selected);

          let activeElement = getDeepActiveElement();
          assertEquals(activeElement, discoveryListItem[0]);

          // Select a profile.
          assertTrue(!!discoveryListItem[1]);
          (discoveryListItem[1] as ProfileDiscoveryListItemElement).click();
          await flushTasks();

          activeElement = getDeepActiveElement();
          assertEquals(activeElement, discoveryListItem[0]);

          await skipProfileList();
          assertTrue(!!activationCodePage);
          await navigateForwardForInstall(activationCodePage);

          // Should now be at the final page.
          await assertFinalPageAndPressDoneButton(false);
          endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
        });

    async function selectProfile() {
      assertTrue(!!profileDiscoveryPage);
      // Select the first profile on the list.
      const profileList =
          profileDiscoveryPage.shadowRoot!.querySelector<IronListElement>(
              '#profileList');
      assertTrue(!!profileList);
      assertTrue(!!profileList.items);
      profileList.selectItem(profileList.items[0]);
      await flushTasks();

      // The 'Forward' button should now be enabled.
      assertTrue(eSimPage.buttonState.forward === ButtonState.ENABLED);

      // Simulate pressing 'Forward'.
      await navigateForwardForInstall(profileDiscoveryPage);
    }

    [1, 2].forEach(profileCount => {
      test('Select profile flow', async function() {
        await setupWithProfiles(profileCount);

        await selectProfile();

        await flushTasks();
        // Should now be at the final page.
        await assertFinalPageAndPressDoneButton(false);

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });

    [1, 2].forEach(profileCount => {
      test(
          'Select profile with valid confirmation code flow', async function() {
            await setupWithProfiles(profileCount);

            const availableEuiccs =
                await eSimManagerRemote.getAvailableEuiccs();
            assertTrue(!!availableEuiccs.euiccs[0]);
            const euicc: FakeEuicc =
                availableEuiccs.euiccs[0] as unknown as FakeEuicc;
            euicc.setProfileInstallResultForTest(
                ProfileInstallResult.kErrorNeedsConfirmationCode);

            await selectProfile();

            // Confirmation code page should be showing.
            assertConfirmationCodePage(
                /*forwardButtonShouldBeEnabled*/ false);
            assertFocusDefaultButtonEventFired();

            euicc.setProfileInstallResultForTest(ProfileInstallResult.kSuccess);
            await enterConfirmationCode();

            // Should go to final page.
            await assertFinalPageAndPressDoneButton(false);
            assertFocusDefaultButtonEventFired();

            endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
          });
    });

    [1, 2].forEach(profileCount => {
      test('End flow before installation attempted', async function() {
        await setupWithProfiles(profileCount);

        await flushTasks();
        endFlowAndVerifyResult(EsimSetupFlowResult.CANCELLED_WITHOUT_ERROR);
      });
    });

    [1, 2].forEach(profileCount => {
      test('No available network before installation', async function() {
        await setupWithProfiles(profileCount);

        takeWifiNetworkOffline();
        await flushTasks();

        endFlowAndVerifyResult(EsimSetupFlowResult.NO_NETWORK);
      });
    });

    [1, 2].forEach(profileCount => {
      test('No available network after installation', async function() {
        await setupWithProfiles(profileCount);

        await selectProfile();
        await flushTasks();
        // Right after installation, internet connection is lost and the
        // dialog closes, NO_NETWORK should not be reported.
        takeWifiNetworkOffline();
        await flushTasks();

        endFlowAndVerifyResult(EsimSetupFlowResult.SUCCESS);
      });
    });
  });

  test('Show final page with error if no EUICC', async function() {
    eSimPage.initSubflow();
    await assertProfileDiscoveryConsentPageAndContinue();
    await flushTasks();
    await assertFinalPageAndPressDoneButton(/*shouldBeShowingError=*/ true);

    endFlowAndVerifyResult(EsimSetupFlowResult.ERROR_FETCHING_PROFILES);
  });
});
