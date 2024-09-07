// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsNearbyShareSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrRadioButtonElement, CrToggleElement, NearbyAccountManagerBrowserProxyImpl, nearbyShareMojom, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, setReceiveManagerForTesting, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DeviceNameValidationResult, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeReceiveManager} from '../fake_receive_manager.js';

import {TestNearbyAccountManagerBrowserProxy} from './test_nearby_account_manager_browser_proxy.js';

const {RegisterReceiveSurfaceResult} = nearbyShareMojom;

suite('<settings-nearby-share-subpage>', () => {
  let subpage: SettingsNearbyShareSubpageElement;
  let featureToggleButton: SettingsToggleButtonElement;
  let fakeReceiveManager: FakeReceiveManager;
  let accountManagerBrowserProxy: TestNearbyAccountManagerBrowserProxy;
  let fakeContactManager: FakeContactManager;
  let fakeSettings: FakeNearbyShareSettings;

  suiteSetup(() => {
    accountManagerBrowserProxy = new TestNearbyAccountManagerBrowserProxy();
    NearbyAccountManagerBrowserProxyImpl.setInstanceForTesting(
        accountManagerBrowserProxy);
  });

  function setupFakes(): void {
    fakeReceiveManager = new FakeReceiveManager();
    setReceiveManagerForTesting(fakeReceiveManager);

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);
  }

  function syncFakeSettings(): void {
    subpage.set('settings.enabled', fakeSettings.getEnabledForTest());
    subpage.set(
        'settings.isFastInitiationHardwareSupported',
        fakeSettings.getIsFastInitiationHardwareSupportedTest());
    subpage.set(
        'settings.fastInitiationNotificationState',
        fakeSettings.getFastInitiationNotificationStateTest());
    subpage.set('settings.deviceName', fakeSettings.getDeviceNameForTest());
    subpage.set('settings.dataUsage', fakeSettings.getDataUsageForTest());
    subpage.set('settings.visibility', fakeSettings.getVisibilityForTest());
    subpage.set(
        'settings.allowedContacts', fakeSettings.getAllowedContactsForTest());
    subpage.set(
        'settings.isOnboardingComplete', fakeSettings.isOnboardingComplete());
  }

  function createSubpage(
      isEnabled: boolean, isOnboardingComplete: boolean): void {
    subpage = document.createElement('settings-nearby-share-subpage');
    subpage.prefs = {
      'nearby_sharing': {
        'enabled': {
          value: isEnabled,
        },
        'data_usage': {
          value: 3,
        },
        'device_name': {
          value: '',
        },
        'onboarding_complete': {
          value: isOnboardingComplete,
        },
      },
    };
    subpage.isSettingsRetreived = true;

    document.body.appendChild(subpage);
    flush();
  }

  async function init() {
    setupFakes();
    fakeSettings.setEnabled(true);
    fakeSettings.setIsOnboardingComplete(true);

    createSubpage(/*isEnabled=*/ true, /*isOnboardingComplete=*/ true);
    syncFakeSettings();
    const toggle =
        subpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#featureToggleButton');
    assertTrue(!!toggle);
    featureToggleButton = toggle;
    await flushTasks();
  }

  /**
   * Sets up Quick Share v2 tests which require the QuickShareV2 flag to be
   * enabled on page load.
   * - Quick Share is on by default
   * - Visibility is set to Your devices
   */
  async function setupQuickShareV2() {
    subpage.remove();
    loadTimeData.overrideValues({'isQuickShareV2Enabled': true});
    await init();
  }

  setup(async () => {
    await init();
  });

  teardown(() => {
    subpage.remove();
    // TODO(b/350547931): Permanently enable QSv2, remove flag and need to
    // override it.
    loadTimeData.overrideValues({'isQuickShareV2Enabled': false});
    accountManagerBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  // Returns true if the element exists and has not been 'removed' by the
  // Polymer template system.
  function doesElementExist(selector: string): boolean {
    const el = subpage.shadowRoot!.querySelector<HTMLElement>(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  function subpageControlsHidden(isHidden: boolean): void {
    assertEquals(isHidden, !doesElementExist('#highVisibilityToggle'));
    assertEquals(isHidden, !doesElementExist('#editDeviceNameButton'));
    assertEquals(isHidden, !doesElementExist('#editVisibilityButton'));
    assertEquals(isHidden, !doesElementExist('#editDataUsageButton'));
  }

  function subpageControlsDisabled(isDisabled: boolean): void {
    const highVisibilityToggle =
        subpage.shadowRoot!.querySelector('#highVisibilityToggle');
    const editDeviceNameButton =
        subpage.shadowRoot!.querySelector('#editDeviceNameButton');
    const editVisibilityButton =
        subpage.shadowRoot!.querySelector('#editVisibilityButton');
    const editDataUsageButton =
        subpage.shadowRoot!.querySelector('#editDataUsageButton');

    assertTrue(!!highVisibilityToggle);
    assertTrue(!!editDeviceNameButton);
    assertTrue(!!editVisibilityButton);
    assertTrue(!!editDataUsageButton);

    assertEquals(isDisabled, highVisibilityToggle.hasAttribute('disabled'));
    assertEquals(isDisabled, editDeviceNameButton.hasAttribute('disabled'));
    assertEquals(isDisabled, editVisibilityButton.hasAttribute('disabled'));
    assertEquals(isDisabled, editDataUsageButton.hasAttribute('disabled'));
  }

  function getDeviceVisibleToggle(): CrToggleElement {
    const deviceVisibleToggle =
        subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#visibilityBoxTitle cr-toggle');
    assertTrue(!!deviceVisibleToggle);
    return deviceVisibleToggle;
  }

  function getYourDevicesButton(): CrRadioButtonElement {
    const yourDevicesButton =
        subpage.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#yourDevicesVisibility');
    assertTrue(!!yourDevicesButton);
    return yourDevicesButton;
  }

  function getContactsButton(): CrRadioButtonElement {
    const contactsButton =
        subpage.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#contactsVisibility');
    assertTrue(!!contactsButton);
    return contactsButton;
  }

  function getEveryoneButton(): CrRadioButtonElement {
    const everyoneButton =
        subpage.shadowRoot!.querySelector<CrRadioButtonElement>(
            '#everyoneVisibility');
    assertTrue(!!everyoneButton);
    return everyoneButton;
  }

  test('feature toggle button controls preference', () => {
    // Ensure that these controls are enabled/disabled when the Nearby is
    // enabled/disabled.
    assertTrue(featureToggleButton.checked);
    assertTrue(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    subpageControlsHidden(false);
    subpageControlsDisabled(false);

    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    subpageControlsHidden(false);
  });

  test('toggle row controls preference', () => {
    assertTrue(featureToggleButton.checked);
    assertTrue(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());

    featureToggleButton.click();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
  });

  suite('Deeplinking', () => {
    const deepLinkTestData = [
      {
        settingId: settingMojom.Setting.kNearbyShareOnOff.toString(),
        deepLinkElement: '#featureToggleButton',
      },
      {
        settingId: settingMojom.Setting.kNearbyShareDeviceName.toString(),
        deepLinkElement: '#editDeviceNameButton',
      },
      {
        settingId: settingMojom.Setting.kNearbyShareDeviceVisibility.toString(),
        deepLinkElement: '#editVisibilityButton',
      },
      {
        settingId: settingMojom.Setting.kNearbyShareContacts.toString(),
        deepLinkElement: '#manageContactsLinkRow',
      },
      {
        settingId: settingMojom.Setting.kNearbyShareDataUsage.toString(),
        deepLinkElement: '#editDataUsageButton',
      },
      {
        settingId: settingMojom.Setting
                       .kDevicesNearbyAreSharingNotificationOnOff.toString(),
        deepLinkElement: '#fastInitiationNotificationToggle',
      },
    ];

    deepLinkTestData.forEach((testData) => {
      test(
          `Deep link to nearby setting element ${testData.deepLinkElement}`,
          async () => {
            const params = new URLSearchParams();
            params.append('settingId', testData.settingId);
            Router.getInstance().navigateTo(routes.NEARBY_SHARE, params);
            flush();

            const deepLinkElement =
                subpage.shadowRoot!.querySelector<HTMLElement>(
                    testData.deepLinkElement);
            assertTrue(!!deepLinkElement);
            await waitAfterNextRender(deepLinkElement);
            assertEquals(
                deepLinkElement, subpage.shadowRoot!.activeElement,
                `Nearby share setting element ${testData.deepLinkElement}
                     should be focused for settingId= ${testData.settingId}`);
          });
    });
  });

  test('update device name preference', () => {
    assertEquals('', subpage.prefs.nearby_sharing.device_name.value);

    const editDeviceNameButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editDeviceNameButton');
    assertTrue(!!editDeviceNameButton);
    editDeviceNameButton.click();
    flush();

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-device-name-dialog');
    assertTrue(!!dialog);
    const newName = 'NEW NAME';
    const crInput = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!crInput);
    crInput.value = newName;
    const actionButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    flush();
    syncFakeSettings();
    flush();

    assertEquals(newName, subpage.get('settings').deviceName);
  });

  test('validate device name preference', async () => {
    const button = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#editDeviceNameButton');
    assertTrue(!!button);
    button.click();
    flush();
    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-device-name-dialog');
    assertTrue(!!dialog);
    const input = dialog.shadowRoot!.querySelector<CrInputElement>('cr-input');
    assertTrue(!!input);
    const doneButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#doneButton');
    assertTrue(!!doneButton);

    fakeSettings.setNextDeviceNameResult(
        DeviceNameValidationResult.kErrorEmpty);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    // Allow the validation promise to resolve.
    await waitAfterNextRender(input);
    flush();
    assertTrue(input.invalid);
    assertTrue(doneButton.disabled);

    fakeSettings.setNextDeviceNameResult(DeviceNameValidationResult.kValid);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await waitAfterNextRender(input);
    flush();
    assertFalse(input.invalid);
    assertFalse(doneButton.disabled);
  });

  test('update data usage preference', () => {
    assertEquals(2, subpage.get('settings').dataUsage);

    const editDataUsageButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editDataUsageButton');
    assertTrue(!!editDataUsageButton);
    editDataUsageButton.click();
    flush();

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-data-usage-dialog');
    assertTrue(!!dialog);

    const dataUsageWifiOnlyButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#dataUsageWifiOnlyButton');
    assertTrue(!!dataUsageWifiOnlyButton);
    dataUsageWifiOnlyButton.click();

    const actionButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();

    flush();
    syncFakeSettings();
    flush();

    assertEquals(3, subpage.get('settings').dataUsage);
  });

  test('update visibility shows dialog', () => {
    // NOTE: all value editing is done and tested in the
    // nearby-contact-visibility component which is hosted directly on the
    // dialog. Here we just verify the dialog shows up, it has the component,
    // and it has a close/action button.
    const visibilityButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editVisibilityButton');
    assertTrue(!!visibilityButton);
    visibilityButton.click();
    flush();

    const dialog = subpage.shadowRoot!.querySelector(
        'nearby-share-contact-visibility-dialog');
    assertTrue(!!dialog);
    assertTrue(!!dialog.shadowRoot!.querySelector('nearby-contact-visibility'));
    const button =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!button);
    button.click();
  });

  test('toggle high visibility from UI', async () => {
    const toggle = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#highVisibilityToggle');
    assertTrue(!!toggle);
    toggle.click();
    flush();
    assertTrue(fakeReceiveManager.getInHighVisibilityForTest());

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);

    await waitAfterNextRender(dialog);
    const highVisibilityDialog =
        dialog.shadowRoot!.querySelector('nearby-share-high-visibility-page');
    assertTrue(isVisible(highVisibilityDialog));

    dialog['close_']();
    assertFalse(fakeReceiveManager.getInHighVisibilityForTest());
  });

  test('high visibility UI updates from high visibility changes', async () => {
    const highVisibilityToggle =
        subpage.shadowRoot!.querySelector<CrToggleElement>(
            '#highVisibilityToggle');
    assertTrue(!!highVisibilityToggle);
    assertFalse(highVisibilityToggle.checked);

    fakeReceiveManager.setInHighVisibilityForTest(true);
    assertTrue(highVisibilityToggle.checked);

    fakeReceiveManager.setInHighVisibilityForTest(false);
    assertFalse(highVisibilityToggle.checked);

    // Process stopped unchecks the toggle.
    fakeReceiveManager.setInHighVisibilityForTest(true);
    assertTrue(highVisibilityToggle.checked);
    subpage.onNearbyProcessStopped();
    flush();
    assertFalse(highVisibilityToggle.checked);

    // Failure to start advertising unchecks the toggle.
    fakeReceiveManager.setInHighVisibilityForTest(false);
    fakeReceiveManager.setInHighVisibilityForTest(true);
    assertTrue(highVisibilityToggle.checked);
    subpage.onStartAdvertisingFailure();
    flush();
    assertFalse(highVisibilityToggle.checked);

    // Toggle still gets unchecked even if advertising was not attempted.
    // E.g. if Bluetooth is off when high visibility is toggled.
    fakeReceiveManager.setInHighVisibilityForTest(false);
    subpage.set('inHighVisibility_', true);
    subpage['showHighVisibilityPage_']();
    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    await waitAfterNextRender(dialog);
    const highVisibilityDialog =
        dialog.shadowRoot!.querySelector('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);
    await waitAfterNextRender(dialog);
    assertTrue(isVisible(highVisibilityDialog));
    highVisibilityDialog.registerResult =
        RegisterReceiveSurfaceResult.kNoConnectionMedium;
    await waitAfterNextRender(highVisibilityDialog);
    const template =
        highVisibilityDialog.shadowRoot!.querySelector('nearby-page-template');
    assertTrue(!!template);
    const button =
        template.shadowRoot!.querySelector<HTMLButtonElement>('#closeButton');
    assertTrue(!!button);
    button.click();
    flush();
    assertFalse(highVisibilityToggle.checked);
  });

  test('GAIA email, account manager enabled', async () => {
    await accountManagerBrowserProxy.whenCalled('getAccounts');
    flush();

    const profileName = subpage.shadowRoot!.querySelector('#profileName');
    assertTrue(!!profileName);
    assertEquals('Primary Account', profileName.textContent!.trim());
    const profileLabel = subpage.shadowRoot!.querySelector('#profileLabel');
    assertTrue(!!profileLabel);
    assertEquals('primary@gmail.com', profileLabel.textContent!.trim());
  });

  test('show receive dialog', () => {
    subpage.set('showReceiveDialog_', true);
    flush();

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
  });

  test('show high visibility dialog', async () => {
    // Mock performance.now to return a constant 0 for testing.
    const originalNow = performance.now;
    performance.now = () => {
      return 0;
    };

    const params = new URLSearchParams();
    params.append('receive', '1');
    params.append('timeout', '600');  // 10 minutes
    Router.getInstance().navigateTo(routes.NEARBY_SHARE, params);

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog =
        dialog.shadowRoot!.querySelector('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);
    assertFalse(highVisibilityDialog['highVisibilityTimedOut_']());

    flush();
    await waitAfterNextRender(dialog);

    assertTrue(isVisible(highVisibilityDialog));
    assertEquals(600 * 1000, highVisibilityDialog.shutoffTimestamp);

    // Restore mock
    performance.now = originalNow;
  });

  test('high visibility dialog times out', async () => {
    // Mock performance.now to return a constant 0 for testing.
    const originalNow = performance.now;
    performance.now = () => {
      return 0;
    };

    const params = new URLSearchParams();
    params.append('receive', '1');
    params.append('timeout', '600');  // 10 minutes
    Router.getInstance().navigateTo(routes.NEARBY_SHARE, params);

    const dialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog =
        dialog.shadowRoot!.querySelector('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);

    highVisibilityDialog['calculateRemainingTime_']();
    assertFalse(highVisibilityDialog['highVisibilityTimedOut_']());

    // Set time past the shutoffTime.
    performance.now = () => {
      return 6000001;
    };

    highVisibilityDialog['calculateRemainingTime_']();
    await waitAfterNextRender(dialog);
    assertTrue(isVisible(highVisibilityDialog));
    assertTrue(highVisibilityDialog['highVisibilityTimedOut_']());

    // Restore mock
    performance.now = originalNow;
  });

  test('download contacts on attach', () => {
    // Ensure contacts download occurs when the subpage is attached.
    assertTrue(fakeContactManager.downloadContactsCalled);
  });

  test('Do not download contacts on attach pre-onboarding', async () => {
    subpage.remove();
    Router.getInstance().resetRouteForTesting();

    setupFakes();
    fakeSettings.setEnabled(false);
    fakeSettings.setIsOnboardingComplete(false);
    syncFakeSettings();
    createSubpage(/*isEnabled=*/ false, /*isOnboardingComplete=*/ false);

    await flushTasks();
    // Ensure contacts download occurs when the subpage is attached.
    assertFalse(fakeContactManager.downloadContactsCalled);
  });

  test('Show setup button pre-onboarding', async () => {
    subpage.remove();
    Router.getInstance().resetRouteForTesting();

    setupFakes();
    createSubpage(/*isEnabled=*/ false, /*isOnboardingComplete=*/ false);

    await flushTasks();
    assertFalse(doesElementExist('#featureToggleButton'));
    assertTrue(doesElementExist('#setupRow'));

    // Clicking starts onboarding flow
    const setupRow = subpage.shadowRoot!.querySelector('#setupRow');
    assertTrue(!!setupRow);
    const button = setupRow.querySelector('cr-button');
    assertTrue(!!button);
    button.click();
    await flushTasks();
    assertTrue(doesElementExist('#receiveDialog'));

    const receiveDialog = subpage.shadowRoot!.querySelector('#receiveDialog');
    assertTrue(!!receiveDialog);

    const element = receiveDialog.shadowRoot!.querySelector('#onboarding-one');
    assertTrue(!!element);

    assertEquals('active', element.className);
  });

  test('feature toggle UI changes', () => {
    // Ensure toggle off UI occurs when toggle off.
    assertTrue(featureToggleButton.checked);
    assertEquals('On', featureToggleButton.label.trim());

    featureToggleButton.click();

    assertFalse(featureToggleButton.checked);
    assertEquals('Off', featureToggleButton.label.trim());
  });

  test('subpage hidden when feature toggled off', () => {
    // Ensure that the subpage content is hidden when the Nearby is off.
    const subpageContent =
        subpage.shadowRoot!.querySelector<HTMLElement>('#subpageContent');
    const highVizToggle = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#highVisibilityToggle');
    const editDeviceNameButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editDeviceNameButton');
    const editVisibilityButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editVisibilityButton');
    const editDataUsageButton =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editDataUsageButton');

    assertTrue(!!subpageContent);
    assertTrue(!!highVizToggle);
    assertTrue(!!editDeviceNameButton);
    assertTrue(!!editVisibilityButton);
    assertTrue(!!editDataUsageButton);

    assertTrue(featureToggleButton.checked);
    assertTrue(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    assertTrue(doesElementExist('#help'));

    editVisibilityButton.click();
    flush();
    const visibilityDialog = subpage.shadowRoot!.querySelector(
        'nearby-share-contact-visibility-dialog');
    assertTrue(!!visibilityDialog);
    assertTrue(!!visibilityDialog.shadowRoot!.querySelector(
        'nearby-contact-visibility'));

    editDeviceNameButton.click();
    flush();
    const deviceNameDialog =
        subpage.shadowRoot!.querySelector('nearby-share-device-name-dialog');
    assertTrue(!!deviceNameDialog);

    editDataUsageButton.click();
    flush();
    const dataUsageDialog =
        subpage.shadowRoot!.querySelector('nearby-share-data-usage-dialog');
    assertTrue(!!dataUsageDialog);

    highVizToggle.click();
    flush();
    const receiveDialog =
        subpage.shadowRoot!.querySelector('nearby-share-receive-dialog');
    assertTrue(!!receiveDialog);

    const helpContent =
        subpage.shadowRoot!.querySelector<HTMLElement>('#helpContent');
    assertTrue(!!helpContent);

    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    assertEquals('none', subpageContent.style.display);
    assertEquals('none', helpContent.style.display);
    subpageControlsHidden(false);
    assertFalse(doesElementExist('#help'));
  });

  test('Fast init toggle exists', () => {
    assertTrue(!!subpage.shadowRoot!.querySelector(
        '#fastInitiationNotificationToggle'));
  });

  test('UX changes disabled when no hardware support', async () => {
    subpage.set('settings.isFastInitiationHardwareSupported', false);
    await flushTasks();

    // Toggle doesnt exist
    const fastInitToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#fastInitiationNotificationToggle');
    assertNull(fastInitToggle);

    // Subpage contents do not show when feature off
    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());

    subpageControlsHidden(true);
  });

  test('Fast initiation notification toggle', async () => {
    const fastInitToggle = subpage.shadowRoot!.querySelector<CrToggleElement>(
        '#fastInitiationNotificationToggle');
    assertTrue(!!fastInitToggle);
    await flushTasks();
    assertTrue(fastInitToggle.checked);
    assertEquals(
        FastInitiationNotificationState.kEnabled,
        subpage.get('settings').fastInitiationNotificationState);

    fastInitToggle.click();
    await flushTasks();
    assertFalse(fastInitToggle.checked);
    assertEquals(
        FastInitiationNotificationState.kDisabledByUser,
        subpage.get('settings').fastInitiationNotificationState);
  });

  test('Subpage content visible but disabled when feature off', () => {
    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());

    subpageControlsHidden(false);
    subpageControlsDisabled(true);
  });

  test('Subpage content not visible pre-onboarding', async () => {
    featureToggleButton.click();
    subpage.set('prefs.nearby_sharing.onboarding_complete.value', false);
    await flushTasks();

    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    subpageControlsHidden(true);
  });

  test('Subpage content visible but disabled when feature off', () => {
    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());

    subpageControlsHidden(false);
    subpageControlsDisabled(true);
  });

  test('Subpage content not visible pre-onboarding', async () => {
    featureToggleButton.click();
    subpage.set('prefs.nearby_sharing.onboarding_complete.value', false);
    await flushTasks();

    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    subpageControlsHidden(true);
  });

  test(
      'Subpage shows no Quick Share on/off toggle on QuickShareV2 enabled',
      async () => {
        setupQuickShareV2();
        const enableQuickShareToggle =
            subpage.shadowRoot!.querySelector('#featureToggleButton');
        assertFalse(!!enableQuickShareToggle);
      });

  test('QuickShareV2: Visibility sublabel hidden when QS enabled', async () => {
    setupQuickShareV2();
    subpage.set('settings.visibility', Visibility.kAllContacts);
    await waitAfterNextRender(subpage);
    assertFalse(
        isChildVisible(subpage, '#visibilityBoxTitle .secondary', false));
  });

  test(
      'QuickShareV2: Visibility sublabel visible when QS disabled',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kNoOne);
        await waitAfterNextRender(subpage);
        assertTrue(
            isChildVisible(subpage, '#visibilityBoxTitle .secondary', false));
      });

  test(
      'QuickShareV2: Visibility buttons disabled when QS disabled',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kNoOne);
        await waitAfterNextRender(subpage);
        const visibilityButtonGroup =
            subpage.shadowRoot!.querySelector('cr-radio-group');
        assertTrue(!!visibilityButtonGroup);
        assertTrue(visibilityButtonGroup.disabled);
      });

  test(
      'QuickShareV2: Visibility change to Your devices on user click Your devices label',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kAllContacts);
        await waitAfterNextRender(subpage);
        assertNotEquals(subpage.settings.visibility, Visibility.kYourDevices);

        const yourDevicesButton = getYourDevicesButton();
        yourDevicesButton.click();

        await waitAfterNextRender(subpage);
        assertEquals(subpage.settings.visibility, Visibility.kYourDevices);
      });

  test(
      'QuickShareV2: Visibility change to Contacts on user click Contacts label',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kYourDevices);
        await waitAfterNextRender(subpage);
        assertNotEquals(subpage.settings.visibility, Visibility.kAllContacts);

        const contactsButton = getContactsButton();
        contactsButton.click();

        await waitAfterNextRender(subpage);
        assertEquals(subpage.settings.visibility, Visibility.kAllContacts);
      });

  test(
      'QuickShareV2: Visibility defaulted to Your devices on Selected contacts visibility setting detected',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kSelectedContacts);
        await waitAfterNextRender(subpage);
        assertEquals(subpage.settings.visibility, Visibility.kYourDevices);
      });

  test(
      'QuickShareV2: Previously set Visibility reset on device visibility toggled off then on',
      async () => {
        setupQuickShareV2();
        subpage.set('settings.visibility', Visibility.kAllContacts);
        await waitAfterNextRender(subpage);

        let deviceVisibleToggle = getDeviceVisibleToggle();
        assertTrue(!!deviceVisibleToggle);
        deviceVisibleToggle.click();
        await waitAfterNextRender(subpage);

        assertEquals(subpage.settings.visibility, Visibility.kNoOne);

        deviceVisibleToggle = getDeviceVisibleToggle();
        assertTrue(!!deviceVisibleToggle);
        deviceVisibleToggle.click();
        await waitAfterNextRender(subpage);

        const contactsButton = getContactsButton();
        assertTrue(contactsButton.checked);
        assertEquals(subpage.settings.visibility, Visibility.kAllContacts);
      });

  test(
      'QuickShareV2: Everyone visibility set when high visibility enabled',
      async () => {
        setupQuickShareV2();
        fakeReceiveManager.setInHighVisibilityForTest(true);

        const everyoneButton = getEveryoneButton();
        assertTrue(everyoneButton.checked);
      });

  test(
      'QuickShareV2: High visibility enabled on select Everyone button',
      async () => {
        setupQuickShareV2();
        fakeReceiveManager.setInHighVisibilityForTest(false);

        const everyoneButton = getEveryoneButton();
        everyoneButton.click();

        assertTrue(
            await fakeReceiveManager.isInHighVisibility().then((result) => {
              return result.inHighVisibility;
            }));
      });

  test(
      'QuickShareV2: High visibility disabled on select visibility button from Everyone visibility',
      async () => {
        setupQuickShareV2();
        fakeReceiveManager.setInHighVisibilityForTest(true);

        const everyoneButton = getEveryoneButton();
        assertTrue(everyoneButton.checked);

        const yourDevicesButton = getYourDevicesButton();
        yourDevicesButton.click();

        assertFalse(
            await fakeReceiveManager.isInHighVisibility().then((result) => {
              return result.inHighVisibility;
            }));

        test(
            'QuickShareV2: Former visibility restored on Everyone button selected and failure to register receive surface',
            async () => {
              setupQuickShareV2();
              fakeReceiveManager.setNextResultForTest(false);

              const everyoneButton = getEveryoneButton();
              everyoneButton.click();

              assertFalse(everyoneButton.checked);
              const yourDevicesButton = getYourDevicesButton();
              assertTrue(yourDevicesButton.checked);
            });

        test(
            'QuickShareV2: Everyone visibility restored on Your devices button selected and failure to de-register receive surface',
            async () => {
              setupQuickShareV2();
              const everyoneButton = getEveryoneButton();
              everyoneButton.click();

              fakeReceiveManager.setNextResultForTest(false);
              const yourDevicesButton = getYourDevicesButton();
              yourDevicesButton.click();

              assertFalse(yourDevicesButton.checked);
              assertTrue(everyoneButton.checked);
            });

        test(
            'QuickShareV2: Your devices visibility enabled on Quick Share enabled, Everyone button previously selected and failure to register receive surface',
            async () => {
              setupQuickShareV2();
              fakeReceiveManager.setInHighVisibilityForTest(true);
              const deviceVisibleToggle = getDeviceVisibleToggle();
              deviceVisibleToggle.click();
              assertFalse(await fakeReceiveManager.isInHighVisibility().then(
                  (result) => {
                    return result.inHighVisibility;
                  }));

              fakeReceiveManager.setNextResultForTest(false);
              deviceVisibleToggle.click();
              const everyoneButton = getEveryoneButton();
              assertFalse(everyoneButton.checked);
              const yourDevicesButton = getYourDevicesButton();
              assertTrue(yourDevicesButton.checked);
            });

        test(
            'QuickShareV2: Everyone visibility restored on Quick Share disabled and failure to de-register receive surface',
            async () => {
              setupQuickShareV2();
              fakeReceiveManager.setInHighVisibilityForTest(true);
              fakeReceiveManager.setNextResultForTest(false);
              const deviceVisibileToggle = getDeviceVisibleToggle();
              deviceVisibileToggle.click();
              const everyoneButton = getEveryoneButton();
              assertTrue(everyoneButton.checked);
              assertTrue(await fakeReceiveManager.isInHighVisibility().then(
                  (result) => {
                    return result.inHighVisibility;
                  }));
            });
      });
});
