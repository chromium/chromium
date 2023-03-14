// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NearbyAccountManagerBrowserProxyImpl, nearbyShareMojom, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting, setReceiveManagerForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {DeviceNameValidationResult, FastInitiationNotificationState} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeReceiveManager} from './fake_receive_manager.js';

const {RegisterReceiveSurfaceResult} = nearbyShareMojom;

/** @implements {AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
    ]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');

    return Promise.resolve([
      {
        id: '123',
        accountType: 1,
        isDeviceAccount: true,
        isSignedIn: true,
        unmigrated: false,
        fullName: 'Primary Account',
        pic: 'data:image/png;base64,primaryAccountPicData',
        email: 'primary@gmail.com',
      },
    ]);
  }
}

suite('NearbyShare', function() {
  /** @type {?SettingsNearbyShareSubpage} */
  let subpage = null;
  /** @type {?SettingsToggleButtonElement} */
  let featureToggleButton = null;
  /** @type {?FakeReceiveManager} */
  let fakeReceiveManager = null;
  /** @type {AccountManagerBrowserProxy} */
  let accountManagerBrowserProxy = null;
  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings = null;

  setup(function() {
    setupFakes();
    fakeSettings.setEnabled(true);
    fakeSettings.setIsOnboardingComplete(true);


    createSubpage(/*is_enabled=*/ true, /*is_onboarding_complete=*/ true);
    syncFakeSettings();
    featureToggleButton =
        subpage.shadowRoot.querySelector('#featureToggleButton');

    return flushAsync();
  });

  teardown(function() {
    subpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function setupFakes() {
    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    NearbyAccountManagerBrowserProxyImpl.setInstanceForTesting(
        accountManagerBrowserProxy);

    fakeReceiveManager = new FakeReceiveManager();
    setReceiveManagerForTesting(fakeReceiveManager);

    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);
  }

  function syncFakeSettings() {
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

  function createSubpage(is_enabled, is_onboarding_complete) {
    PolymerTest.clearBody();

    subpage = document.createElement('settings-nearby-share-subpage');
    subpage.prefs = {
      'nearby_sharing': {
        'enabled': {
          value: is_enabled,
        },
        'data_usage': {
          value: 3,
        },
        'device_name': {
          value: '',
        },
        'onboarding_complete': {
          value: is_onboarding_complete,
        },
      },
    };
    subpage.isSettingsRetreived = true;

    document.body.appendChild(subpage);
    flush();
  }

  // Returns true if the element exists and has not been 'removed' by the
  // Polymer template system.
  function doesElementExist(selector) {
    const el = subpage.shadowRoot.querySelector(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  function subpageControlsHidden(is_hidden) {
    assertEquals(is_hidden, !doesElementExist('#highVisibilityToggle'));
    assertEquals(is_hidden, !doesElementExist('#editDeviceNameButton'));
    assertEquals(is_hidden, !doesElementExist('#editVisibilityButton'));
    assertEquals(is_hidden, !doesElementExist('#editDataUsageButton'));
  }

  function subpageControlsDisabled(is_disabled) {
    assertEquals(
        is_disabled,
        subpage.shadowRoot.querySelector('#highVisibilityToggle')
            .hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.shadowRoot.querySelector('#editDeviceNameButton')
            .hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.shadowRoot.querySelector('#editVisibilityButton')
            .hasAttribute('disabled'));
    assertEquals(
        is_disabled,
        subpage.shadowRoot.querySelector('#editDataUsageButton')
            .hasAttribute('disabled'));
  }

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('feature toggle button controls preference', function() {
    // Ensure that these controls are enabled/disabled when the Nearby is
    // enabled/disabled.
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    subpageControlsHidden(false);
    subpageControlsDisabled(false);

    featureToggleButton.click();
    flush();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    subpageControlsHidden(false);
  });

  test('toggle row controls preference', function() {
    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
  });

  suite('Deeplinking', () => {
    const deepLinkTestData = [
      {settingId: '208', deepLinkElement: '#featureToggleButton'},
      {settingId: '214', deepLinkElement: '#editDeviceNameButton'},
      {settingId: '215', deepLinkElement: '#editVisibilityButton'},
      {settingId: '216', deepLinkElement: '#manageContactsLinkRow'},
      {settingId: '217', deepLinkElement: '#editDataUsageButton'},
      {settingId: '220', deepLinkElement: '#fastInitiationNotificationToggle'},
    ];

    deepLinkTestData.forEach((testData) => {
      test(
          'Deep link to nearby setting element ' + testData.deepLinkElement,
          async () => {
            const params = new URLSearchParams();
            params.append('settingId', testData.settingId);
            Router.getInstance().navigateTo(routes.NEARBY_SHARE, params);

            flush();

            const deepLinkElement =
                subpage.shadowRoot.querySelector(testData.deepLinkElement);
            await waitAfterNextRender(deepLinkElement);
            assertEquals(
                deepLinkElement, subpage.shadowRoot.activeElement,
                'Nearby share setting element ' + testData.deepLinkElement +
                    ' should be focused for settingId=' + testData.settingId);
          });
    });
  });

  test('update device name preference', function() {
    assertEquals('', subpage.prefs.nearby_sharing.device_name.value);

    subpage.shadowRoot.querySelector('#editDeviceNameButton').click();
    flush();

    const dialog =
        subpage.shadowRoot.querySelector('nearby-share-device-name-dialog');
    const oldName = subpage.settings.deviceName;
    const newName = 'NEW NAME';
    dialog.shadowRoot.querySelector('cr-input').value = newName;
    dialog.shadowRoot.querySelector('.action-button').click();
    flush();
    syncFakeSettings();
    flush();

    assertEquals(newName, subpage.settings.deviceName);
    subpage.set('settings.deviceName', oldName);
  });

  test('validate device name preference', async () => {
    subpage.shadowRoot.querySelector('#editDeviceNameButton').click();
    flush();
    const dialog =
        subpage.shadowRoot.querySelector('nearby-share-device-name-dialog');
    const input = dialog.shadowRoot.querySelector('cr-input');
    const doneButton = dialog.shadowRoot.querySelector('#doneButton');

    fakeSettings.setNextDeviceNameResult(
        DeviceNameValidationResult.kErrorEmpty);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    // Allow the validation promise to resolve.
    await waitAfterNextRender();
    flush();
    assertTrue(input.invalid);
    assertTrue(doneButton.disabled);

    fakeSettings.setNextDeviceNameResult(DeviceNameValidationResult.kValid);
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    await waitAfterNextRender();
    flush();
    assertFalse(input.invalid);
    assertFalse(doneButton.disabled);
  });

  test('update data usage preference', function() {
    assertEquals(2, subpage.settings.dataUsage);

    subpage.shadowRoot.querySelector('#editDataUsageButton').click();
    flush();

    const dialog =
        subpage.shadowRoot.querySelector('nearby-share-data-usage-dialog');
    dialog.shadowRoot.querySelector('#dataUsageWifiOnlyButton').click();
    dialog.shadowRoot.querySelector('.action-button').click();
    flush();
    syncFakeSettings();
    flush();

    assertEquals(3, subpage.settings.dataUsage);
  });

  test('update visibility shows dialog', function() {
    // NOTE: all value editing is done and tested in the
    // nearby-contact-visibility component which is hosted directly on the
    // dialog. Here we just verify the dialog shows up, it has the component,
    // and it has a close/action button.
    subpage.shadowRoot.querySelector('#editVisibilityButton').click();
    flush();

    const dialog = subpage.shadowRoot.querySelector(
        'nearby-share-contact-visibility-dialog');
    assertTrue(
        dialog.shadowRoot.querySelector('nearby-contact-visibility') !== null);
    dialog.shadowRoot.querySelector('.action-button').click();
  });

  test('toggle high visibility from UI', async function() {
    subpage.shadowRoot.querySelector('#highVisibilityToggle').click();
    flush();
    assertTrue(fakeReceiveManager.getInHighVisibilityForTest());

    const dialog =
        subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);

    await waitAfterNextRender(dialog);
    const highVisibilityDialog =
        dialog.shadowRoot.querySelector('nearby-share-high-visibility-page');
    assertTrue(isVisible(highVisibilityDialog));

    dialog.close_();
    assertFalse(fakeReceiveManager.getInHighVisibilityForTest());
  });

  test(
      'high visibility UI updates from high visibility changes',
      async function() {
        const highVisibilityToggle =
            subpage.shadowRoot.querySelector('#highVisibilityToggle');
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
        subpage.inHighVisibility_ = true;
        subpage.showHighVisibilityPage_();
        const dialog =
            subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
        assertTrue(!!dialog);
        await waitAfterNextRender(dialog);
        const highVisibilityDialog = dialog.shadowRoot.querySelector(
            'nearby-share-high-visibility-page');
        await waitAfterNextRender(dialog);
        assertTrue(isVisible(highVisibilityDialog));
        highVisibilityDialog.registerResult =
            RegisterReceiveSurfaceResult.kNoConnectionMedium;
        await waitAfterNextRender(highVisibilityDialog);
        highVisibilityDialog.shadowRoot.querySelector('nearby-page-template')
            .shadowRoot.querySelector('#closeButton')
            .click();
        flush();
        assertFalse(highVisibilityToggle.checked);
      });

  test('GAIA email, account manager enabled', async () => {
    await accountManagerBrowserProxy.whenCalled('getAccounts');
    flush();

    const profileName = subpage.shadowRoot.querySelector('#profileName');
    assertEquals('Primary Account', profileName.textContent.trim());
    const profileLabel = subpage.shadowRoot.querySelector('#profileLabel');
    assertEquals('primary@gmail.com', profileLabel.textContent.trim());
  });

  test('show receive dialog', function() {
    subpage.showReceiveDialog_ = true;
    flush();

    const dialog =
        subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
  });

  test('show high visibility dialog', async function() {
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
        subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog =
        dialog.shadowRoot.querySelector('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);
    assertFalse(highVisibilityDialog.highVisibilityTimedOut_());

    flush();
    await waitAfterNextRender(dialog);

    assertTrue(isVisible(highVisibilityDialog));
    assertEquals(highVisibilityDialog.shutoffTimestamp, 600 * 1000);

    // Restore mock
    performance.now = originalNow;
  });

  test('high visibility dialog times out', async function() {
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
        subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
    assertTrue(!!dialog);
    const highVisibilityDialog =
        dialog.shadowRoot.querySelector('nearby-share-high-visibility-page');
    assertTrue(!!highVisibilityDialog);

    highVisibilityDialog.calculateRemainingTime_();
    assertFalse(highVisibilityDialog.highVisibilityTimedOut_());

    // Set time past the shutoffTime.
    performance.now = () => {
      return 6000001;
    };

    highVisibilityDialog.calculateRemainingTime_();
    await waitAfterNextRender(dialog);
    assertTrue(isVisible(highVisibilityDialog));
    assertTrue(highVisibilityDialog.highVisibilityTimedOut_());

    // Restore mock
    performance.now = originalNow;
  });

  test('download contacts on attach', async () => {
    await flushAsync();
    // Ensure contacts download occurs when the subpage is attached.
    assertTrue(fakeContactManager.downloadContactsCalled);
  });

  test('Do not download contacts on attach pre-onboarding', async () => {
    await flushAsync();

    subpage.remove();
    Router.getInstance().resetRouteForTesting();

    setupFakes();
    fakeSettings.setEnabled(false);
    fakeSettings.setIsOnboardingComplete(false);
    syncFakeSettings();
    createSubpage(/*is_enabled=*/ false, /*is_onboarding_complete=*/ false);

    await flushAsync();
    // Ensure contacts download occurs when the subpage is attached.
    assertFalse(fakeContactManager.downloadContactsCalled);
  });

  test('Show setup button pre-onboarding', async () => {
    await flushAsync();

    subpage.remove();
    Router.getInstance().resetRouteForTesting();

    setupFakes();
    createSubpage(/*is_enabled=*/ false, /*is_onboarding_complete=*/ false);

    await flushAsync();
    assertFalse(doesElementExist('#featureToggleButton'));
    assertTrue(doesElementExist('#setupRow'));

    // Clicking starts onboarding flow
    subpage.shadowRoot.querySelector('#setupRow')
        .querySelector('cr-button')
        .click();
    await flushAsync();
    assertTrue(doesElementExist('#receiveDialog'));
    assertEquals(
        'active',
        subpage.shadowRoot.querySelector('#receiveDialog')
            .shadowRoot.querySelector('#onboarding-one')
            .className);
  });

  test('feature toggle UI changes', function() {
    // Ensure toggle off UI occurs when toggle off.
    assertEquals(true, featureToggleButton.checked);
    assertEquals('On', featureToggleButton.label.trim());
    assertTrue(featureToggleButton.classList.contains('enabled-toggle-on'));
    assertFalse(featureToggleButton.classList.contains('enabled-toggle-off'));

    featureToggleButton.click();

    assertEquals(false, featureToggleButton.checked);
    assertEquals('Off', featureToggleButton.label.trim());
    assertFalse(featureToggleButton.classList.contains('enabled-toggle-on'));
    assertTrue(featureToggleButton.classList.contains('enabled-toggle-off'));
  });

  test('subpage hidden when feature toggled off', function() {
    // Ensure that the subpage content is hidden when the Nearby is off.
    const subpageContent = subpage.shadowRoot.querySelector('#subpageContent');
    const highVizToggle =
        subpage.shadowRoot.querySelector('#highVisibilityToggle');
    const editDeviceNameButton =
        subpage.shadowRoot.querySelector('#editDeviceNameButton');
    const editVisibilityButton =
        subpage.shadowRoot.querySelector('#editVisibilityButton');
    const editDataUsageButton =
        subpage.shadowRoot.querySelector('#editDataUsageButton');

    assertEquals(true, featureToggleButton.checked);
    assertEquals(true, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('On', featureToggleButton.label.trim());
    assertTrue(doesElementExist('#help'));

    editVisibilityButton.click();
    flush();
    const visibilityDialog = subpage.shadowRoot.querySelector(
        'nearby-share-contact-visibility-dialog');
    assertTrue(!!visibilityDialog);
    assertTrue(
        visibilityDialog.shadowRoot.querySelector(
            'nearby-contact-visibility') !== null);

    editDeviceNameButton.click();
    flush();
    const deviceNameDialog =
        subpage.shadowRoot.querySelector('nearby-share-device-name-dialog');
    assertTrue(!!deviceNameDialog);

    editDataUsageButton.click();
    flush();
    const dataUsageDialog =
        subpage.shadowRoot.querySelector('nearby-share-data-usage-dialog');
    assertTrue(!!dataUsageDialog);

    highVizToggle.click();
    flush();
    const receiveDialog =
        subpage.shadowRoot.querySelector('nearby-share-receive-dialog');
    assertTrue(!!receiveDialog);

    featureToggleButton.click();
    flush();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());
    assertEquals('none', subpageContent.style.display);
    assertEquals(
        'none', subpage.shadowRoot.querySelector('#helpContent').style.display);
    subpageControlsHidden(false);
    assertFalse(doesElementExist('#help'));
  });

  test('Fast init toggle exists', function() {
    assertTrue(!!subpage.shadowRoot.querySelector(
        '#fastInitiationNotificationToggle'));
  });

  test('UX changes disabled when no hardware support', async () => {
    subpage.set('settings.isFastInitiationHardwareSupported', false);
    await flushAsync();

    // Toggle doesnt exist
    const fastInitToggle =
        subpage.shadowRoot.querySelector('#fastInitiationNotificationToggle');
    assertFalse(!!fastInitToggle);

    // Subpage contents do not show when feature off
    featureToggleButton.click();
    flush();

    assertFalse(featureToggleButton.checked);
    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());

    subpageControlsHidden(true);
  });

  test('Fast initiation notification toggle', async () => {
    const fastInitToggle =
        subpage.shadowRoot.querySelector('#fastInitiationNotificationToggle');
    assertTrue(!!fastInitToggle);
    await flushAsync();
    assertTrue(fastInitToggle.checked);
    assertEquals(
        FastInitiationNotificationState.kEnabled,
        subpage.settings.fastInitiationNotificationState);

    fastInitToggle.click();
    await flushAsync();
    assertFalse(fastInitToggle.checked);
    assertEquals(
        FastInitiationNotificationState.kDisabledByUser,
        subpage.settings.fastInitiationNotificationState);
  });

  test('Subpage content visible but disabled when feature off', async () => {
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
    await flushAsync();

    assertFalse(subpage.prefs.nearby_sharing.enabled.value);
    subpageControlsHidden(true);
  });

  test('Subpage content visible but disabled when feature off', async () => {
    featureToggleButton.click();
    flush();

    assertEquals(false, featureToggleButton.checked);
    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    assertEquals('Off', featureToggleButton.label.trim());

    subpageControlsHidden(false);
    subpageControlsDisabled(true);
  });

  test('Subpage content not visible pre-onboarding', async () => {
    featureToggleButton.click();
    subpage.set('prefs.nearby_sharing.onboarding_complete.value', false);
    await flushAsync();

    assertEquals(false, subpage.prefs.nearby_sharing.enabled.value);
    subpageControlsHidden(true);
  });
});
