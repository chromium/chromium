// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsResetBrowserProxyImpl, OsSettingsPowerwashDialogElement, ResetSettingsCardElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, LifetimeBrowserProxyImpl, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from '../test_os_lifetime_browser_proxy.js';

import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.js';

suite('<reset-settings-card>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const route =
      isRevampWayfindingEnabled ? routes.SYSTEM_PREFERENCES : routes.OS_RESET;

  let resetSettingsCard: ResetSettingsCardElement;
  let resetPageBrowserProxy: TestOsResetBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let eSimManagerRemote: FakeESimManagerRemote;

  suiteSetup(() => {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    resetPageBrowserProxy = new TestOsResetBrowserProxy();
    OsResetBrowserProxyImpl.setInstanceForTesting(resetPageBrowserProxy);
  });

  setup(() => {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);
    Router.getInstance().navigateTo(route);
    resetSettingsCard = document.createElement('reset-settings-card');
    document.body.appendChild(resetSettingsCard);
    flush();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    resetSettingsCard.remove();
    lifetimeBrowserProxy.reset();
    resetPageBrowserProxy.reset();
  });

  function getPowerwashButton(): HTMLElement {
    const powerwashButton =
        resetSettingsCard.shadowRoot!.querySelector<CrButtonElement>(
            '#powerwashButton');
    assertTrue(!!powerwashButton);
    return powerwashButton;
  }

  function getSanitizeButton(): HTMLElement|null {
    const sanitizeButton =
        resetSettingsCard.shadowRoot!.querySelector<CrButtonElement>(
            '#sanitizeButton');
    return sanitizeButton;
  }

  function getPowerwashDialog(): OsSettingsPowerwashDialogElement {
    const powerwashDialog = resetSettingsCard.shadowRoot!.querySelector(
        'os-settings-powerwash-dialog');
    assertTrue(!!powerwashDialog);
    return powerwashDialog;
  }

  async function testOpenClosePowerwashDialog(
      closeButtonFn: (dialog: OsSettingsPowerwashDialogElement) =>
          HTMLElement): Promise<void> {
    // Open powerwash dialog.
    getPowerwashButton().click();
    await flushTasks();

    const dialog = getPowerwashDialog();
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);

    const onDialogClosedPromise = new Promise<void>((resolve) => {
      dialog.addEventListener('close', () => {
        assertFalse(dialog.$.dialog.open);
        resolve();
      });
    });

    closeButtonFn(dialog).click();
    await onDialogClosedPromise;
  }

  async function openDialogWithESimWarning(): Promise<void> {
    eSimManagerRemote.addEuiccForTest(2);

    // Set the first profile's state to kActive.
    const euiccResponse = await eSimManagerRemote.getAvailableEuiccs();
    const euicc = euiccResponse.euiccs[0];
    assertTrue(!!euicc);

    const profileListResponse = await euicc.getProfileList();
    const profile = profileListResponse.profiles[0];
    assertTrue(!!profile);
    await profile.installProfile('dummyCode');

    // Click the powerwash button.
    getPowerwashButton().click();
    await flushTasks();

    // The eSIM warning should be showing.
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ true);
    const dialog = getPowerwashDialog();
    const itemLength =
        dialog.shadowRoot!.querySelector('iron-list')!.items!.length;
    assertEquals(1, itemLength);

    // The 'Continue' button should initially be disabled.
    assertTrue(dialog.shadowRoot!.querySelector<CrButtonElement>(
                                     '#continue')!.disabled);
  }

  function assertOpenDialogUIState(shouldBeShowingESimWarning: boolean): void {
    const dialog = getPowerwashDialog();
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    const hasPowerwashContainer =
        !!dialog.shadowRoot!.querySelector('#powerwashContainer');
    assertEquals(!shouldBeShowingESimWarning, hasPowerwashContainer);
    assertEquals(!shouldBeShowingESimWarning, hasPowerwashContainer);
    assertEquals(
        !shouldBeShowingESimWarning,
        !!dialog.shadowRoot!.querySelector('#powerwash'));

    assertEquals(
        shouldBeShowingESimWarning,
        !!dialog.shadowRoot!.querySelector('#profilesListContainer'));
    assertEquals(
        shouldBeShowingESimWarning,
        !!dialog.shadowRoot!.querySelector('#continue'));
  }

  /**
   * Navigates to the deep link provided by |settingId| and returns true if
   * the focused element is |deepLinkElement|.
   */
  async function isDeepLinkFocusedForSettingId(
      deepLinkElement: HTMLElement, settingId: string): Promise<boolean> {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(route, params);

    await waitAfterNextRender(deepLinkElement);
    return deepLinkElement === getDeepActiveElement();
  }

  // Tests that the powerwash dialog with no EUICC opens and closes correctly,
  // and that chrome.send calls are propagated as expected.
  test('Powerwash dialog opens and closes correctly', async () => {
    // Test case where the 'cancel' button is clicked.
    await testOpenClosePowerwashDialog((dialog) => {
      return dialog.$.cancel;
    });
  });

  // Tests that when powerwash is requested chrome.send calls are
  // propagated as expected.
  test('Powerwash should trigger factory reset', async () => {
    // Open powerwash dialog.
    getPowerwashButton().click();
    await flushTasks();
    const dialog = getPowerwashDialog();
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
    dialog.shadowRoot!.querySelector<CrButtonElement>('#powerwash')!.click();
    const requestTpmFirmwareUpdate =
        await lifetimeBrowserProxy.whenCalled('factoryReset');
    assertFalse(requestTpmFirmwareUpdate);
  });

  // Tests that when the route changes to one containing a deep link to
  // powerwash, powerwash is focused.
  test('Powerwash button is focused via deep link', async () => {
    const settingId = settingMojom.Setting.kPowerwash.toString();
    const isFocused =
        await isDeepLinkFocusedForSettingId(getPowerwashButton(), settingId);
    assertTrue(
        isFocused, `Powerwash should be focused for settingId=${settingId}.`);
  });

  // Tests that when the route changes to one containing a deep link not equal
  // to powerwash, no focusing of powerwash occurs.
  test(
      'Powerwash button is not focused for different deep link setting ID',
      async () => {
        const invalidSettingId = '1234';
        const isFocused = await isDeepLinkFocusedForSettingId(
            getPowerwashButton(), invalidSettingId);
        assertFalse(
            isFocused,
            `Powerwash should not be focused for settingId=${
                invalidSettingId}.`);
      });

  test(
      'EUICC with no non-pending profiles shows powerwash dialog', async () => {
        eSimManagerRemote.addEuiccForTest(2);

        await testOpenClosePowerwashDialog((dialog) => {
          return dialog.$.cancel;
        });
      });

  test('Non-pending profile shows eSIM warning dialog', async () => {
    await openDialogWithESimWarning();

    // Clicking the checkbox should enable the 'Continue' button.
    const dialog = getPowerwashDialog();
    const continueButton =
        dialog.shadowRoot!.querySelector<CrButtonElement>('#continue');
    assertTrue(!!continueButton);
    dialog.shadowRoot!.querySelector('cr-checkbox')!.click();
    assertFalse(continueButton.disabled);

    // Click the 'Continue' button.
    continueButton.click();
    await flushTasks();
    // The powerwash UI should now be showing.
    assertOpenDialogUIState(/*shouldBeShowingESimWarning=*/ false);
  });

  test(
      'Clicking the eSIM warning dialog link goes to the mobile data subpage',
      async () => {
        await openDialogWithESimWarning();

        const dialog = getPowerwashDialog();
        const mobileSettingsLink =
            dialog.shadowRoot!.querySelector('localized-link')!.shadowRoot!
                .querySelector('a');
        assertTrue(!!mobileSettingsLink);

        mobileSettingsLink.click();
        await flushTasks();

        assertEquals(
            routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
        assertEquals(
            'type=Cellular',
            Router.getInstance().getQueryParameters().toString());
      });

  // Tests that the sanitize button is shown when sanitization is allowed and
  // is not visible otherwise. When it is enabled, the dialog should pop up
  // once the button is clicked.
  const sanitizeFeatureEnabled = loadTimeData.getBoolean('allowSanitize');
  if (sanitizeFeatureEnabled) {
    test('Clicking the sanitizeButton shows its dialog.', async () => {
      const sanitizeButton = getSanitizeButton();
      assertTrue(!!sanitizeButton);
      sanitizeButton.click();
      await resetPageBrowserProxy.whenCalled('onShowSanitizeDialog');
    });
  } else {
    test('The sanitizeButton does not show.', () => {
      const sanitizeButton = getSanitizeButton();
      assertFalse(isVisible(sanitizeButton));
    });
  }
});
