// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrivacyHubBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {PeripheralDataAccessBrowserProxyImpl, Router, routes, SecureDnsMode} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeMetricsPrivate} from './fake_metrics_private.js';
import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

const crosSettingPrefName = 'cros.device.peripheral_data_access_enabled';
const localStatePrefName =
    'settings.local_state_device_pci_data_access_enabled';

/**
 * @implements {PeripheralDataAccessBrowserProxy}
 */
class TestPeripheralDataAccessBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isThunderboltSupported',
      'getPolicyState',
    ]);

    /** @type {DataAccessPolicyState} */
    this.policy_state_ = {
      prefName: crosSettingPrefName,
      isUserConfigurable: false,
    };
  }

  /** @override */
  isThunderboltSupported() {
    this.methodCalled('isThunderboltSupported');
    return Promise.resolve(/*supported=*/ true);
  }

  /** @override */
  getPolicyState() {
    this.methodCalled('getPolicyState');
    return Promise.resolve(this.policy_state_);
  }

  /**
   * @param {String} pref_name
   * @param {Boolean} is_user_configurable
   */
  setPolicyState(pref_name, is_user_configurable) {
    this.policy_state_.prefName = pref_name;
    this.policy_state_.isUserConfigurable = is_user_configurable;
  }
}

suite('PrivacyPageTests', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        },
      },
    },
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    flush();

    await browserProxy.whenCalled('isThunderboltSupported');
  });

  teardown(function() {
    privacyPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Returns true if the element exists and has not been 'removed' by the
   * Polymer template system.
   * @param {string} selector The ID of the element about which to query.
   * @return {boolean} Whether or not the element has been masked by the
   *                   template system.
   * @private
   */
  function elementExists(selector) {
    const el = privacyPage.shadowRoot.querySelector(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  test(
      'Suggested content, hidden when privacy hub feature flag is enabled',
      async () => {
        loadTimeData.overrideValues({
          showPrivacyHubPage: true,
        });

        privacyPage = document.createElement('os-settings-privacy-page');
        document.body.appendChild(privacyPage);
        flush();

        assertFalse(elementExists('#suggested-content'));
      });

  test('Suggested content, pref disabled', async () => {
    loadTimeData.overrideValues({
      showPrivacyHubPage: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    flush();

    // The default state of the pref is disabled.
    const suggestedContent =
        assert(privacyPage.shadowRoot.querySelector('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    loadTimeData.overrideValues({
      showPrivacyHubPage: false,
    });

    // Update the backing pref to enabled.
    privacyPage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        },
      },
      'cros': {
        'device': {
          'peripheral_data_access_enabled': {
            value: true,
          },
        },
      },
      'dns_over_https': {
        'mode': {
          value: SecureDnsMode.AUTOMATIC,
        },
        'templates': {
          value: '',
        },
      },
    };

    flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent =
        assert(privacyPage.shadowRoot.querySelector('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });

  test('Deep link to verified access', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1101');
    Router.getInstance().navigateTo(routes.OS_PRIVACY, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('#verifiedAccessToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Verified access toggle should be focused for settingId=1101.');
  });

  test('Deep link to guest browsing on users page', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1104');
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('settings-manage-users-subpage')
            .shadowRoot.querySelector('#allowGuestBrowsing')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Allow guest browsing should be focused for settingId=1104.');
  });

  test('Deep link to show usernames on sign in on users page', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1105');
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('settings-manage-users-subpage')
            .shadowRoot.querySelector('#showUserNamesOnSignIn')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Allow guest browsing should be focused for settingId=1105.');
  });

  test('Deep link to snooping protection on smart privacy page', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: true,
    });

    const params = new URLSearchParams();
    params.append('settingId', '1114');
    Router.getInstance().navigateTo(routes.SMART_PRIVACY, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('settings-smart-privacy-subpage')
            .shadowRoot.querySelector('#snoopingProtectionToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Snooping protection should be focused for settingId=1114.');
  });

  test('Deep link to quick dim on smart privacy page', async () => {
    loadTimeData.overrideValues({
      isQuickDimEnabled: true,
    });

    const params = new URLSearchParams();
    params.append('settingId', '1115');
    Router.getInstance().navigateTo(routes.SMART_PRIVACY, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('settings-smart-privacy-subpage')
            .shadowRoot.querySelector('#quickDimToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick dim should be focused for settingId=1115.');
  });

  test('Fingerprint dialog closes when token expires', async () => {
    loadTimeData.overrideValues({
      fingerprintUnlockEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    privacyPage.authToken_ = quickUnlockPrivateApi.getFakeToken();

    Router.getInstance().navigateTo(routes.LOCK_SCREEN);
    flush();

    const subpageTrigger =
        privacyPage.shadowRoot.querySelector('#lockScreenSubpageTrigger');
    // Sub-page trigger navigates to the lock screen page.
    subpageTrigger.click();
    flush();

    assertEquals(Router.getInstance().currentRoute, routes.LOCK_SCREEN);
    const lockScreenPage =
        assert(privacyPage.shadowRoot.querySelector('#lockScreen'));

    // Password dialog should not open because the authToken_ is set.
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const editFingerprintsTrigger =
        lockScreenPage.shadowRoot.querySelector('#editFingerprints');
    editFingerprintsTrigger.click();
    flush();

    assertEquals(Router.getInstance().currentRoute, routes.FINGERPRINT);
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const fingerprintTrigger =
        privacyPage.shadowRoot.querySelector('#fingerprint-list')
            .shadowRoot.querySelector('#addFingerprint');
    fingerprintTrigger.click();

    // Invalidate the auth token by firing an event.
    assertFalse(privacyPage.authToken_ === undefined);
    const event = new CustomEvent('invalidate-auth-token-requested');
    lockScreenPage.dispatchEvent(event);
    assertTrue(privacyPage.authToken_ === undefined);

    assertEquals(Router.getInstance().currentRoute, routes.FINGERPRINT);
    assertTrue(privacyPage.showPasswordPromptDialog_);
  });

  test('Refresh token when authentication token invalid', async () => {
    privacyPage.setProperties({setModes_: () => {}});
    flush();

    privacyPage.dispatchEvent(new CustomEvent('auth-token-invalid'));

    assertEquals(privacyPage.setModes_, undefined);
  });

  test('Smart privacy hidden when both features disabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: false,
      isQuickDimEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertFalse(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Smart privacy shown when only quick dim enabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: false,
      isQuickDimEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertTrue(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Smart privacy shown if only snooping protection enabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: true,
      isQuickDimEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertTrue(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Open PrivacyHub', async () => {
    loadTimeData.overrideValues({
      showPrivacyHubPage: true,
    });

    const privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    const privacyHubPageRow = assert(
        privacyPage.shadowRoot.querySelector('#privacyHubSubpageTrigger'));

    assertEquals(
        fakeMetricsPrivate.countMetricValue('ChromeOS.PrivacyHub.Opened', 0),
        0);

    privacyHubPageRow.click();
    flush();
    waitAfterNextRender();

    assertEquals(
        fakeMetricsPrivate.countMetricValue('ChromeOS.PrivacyHub.Opened', 0),
        1);
  });

  test('Send HaTS messages', async () => {
    loadTimeData.overrideValues({
      isPrivacyHubHatsEnabled: true,
    });

    const privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertEquals(
        0, privacyHubBrowserProxy.getCallCount('sendOpenedOsPrivacyPage'));
    assertEquals(
        1, privacyHubBrowserProxy.getCallCount('sendLeftOsPrivacyPage'));

    const params = new URLSearchParams();
    params.append('settingId', '1101');
    Router.getInstance().navigateTo(routes.OS_PRIVACY, params);

    flush();

    assertEquals(
        1, privacyHubBrowserProxy.getCallCount('sendOpenedOsPrivacyPage'));
    assertEquals(
        1, privacyHubBrowserProxy.getCallCount('sendLeftOsPrivacyPage'));

    params.set('settingId', '1105');
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    flush();

    assertEquals(
        1, privacyHubBrowserProxy.getCallCount('sendOpenedOsPrivacyPage'));
    assertEquals(
        2, privacyHubBrowserProxy.getCallCount('sendLeftOsPrivacyPage'));
  });

  // TODO(crbug.com/1262869): add a test for deep linking to snopping setting
  //                          once it has been added.
});

suite('PeripheralDataAccessTest', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  /** @type {Object} */
  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: false,
        },
      },
    },
    'settings': {'local_state_device_pci_data_access_enabled': {value: false}},
    'dns_over_https':
        {'mode': {value: SecureDnsMode.AUTOMATIC}, 'templates': {value: ''}},
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
  });

  teardown(function() {
    privacyPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  async function setUpPage(pref_name, is_user_configurable) {
    browserProxy.setPolicyState(pref_name, is_user_configurable);
    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(privacyPage);
    flush();

    await browserProxy.whenCalled('getPolicyState');
    await waitAfterNextRender();
    flush();
  }

  test('DialogOpensOnToggle', async () => {
    await setUpPage(crosSettingPrefName, /**is_user_configurable=*/ true);
    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot.querySelector('#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const dialog = privacyPage.shadowRoot.querySelector('#protectionDialog')
                       .$.warningDialog;
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton = dialog.querySelector('#cancelButton');
    cancelButton.click();
    flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('DisableClicked', async () => {
    await setUpPage(crosSettingPrefName, /**is_user_configurable=*/ true);
    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot.querySelector('#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const dialog = privacyPage.shadowRoot.querySelector('#protectionDialog')
                       .$.warningDialog;
    assertTrue(dialog.open);

    // Advance the dialog and move onto the next dialog.
    const disableButton = dialog.querySelector('#disableConfirmation');
    disableButton.click();
    flush();

    // The toggle should now be flipped to unset.
    assertFalse(toggle.checked);
  });

  test('managedAndConfigurablePrefIsToggleable', async () => {
    await setUpPage(localStatePrefName, /**is_user_configurable=*/ true);
    flush();

    // Ensure only the local state toggle appears.
    assertTrue(
        privacyPage.shadowRoot.querySelector('#crosSettingDataAccessToggle')
            .hidden);

    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot.querySelector('#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const dialog = privacyPage.shadowRoot.querySelector('#protectionDialog')
                       .$.warningDialog;
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton = dialog.querySelector('#cancelButton');
    cancelButton.click();
    flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('managedAndNonConfigurablePrefIsNotToggleable', async () => {
    await setUpPage(localStatePrefName, /**is_user_configurable=*/ false);
    flush();

    // Ensure only the local state toggle appears.
    assertTrue(
        privacyPage.shadowRoot.querySelector('#crosSettingDataAccessToggle')
            .hidden);

    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot.querySelector('#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    // Dialog should not appear since the toggle is disabled.
    const dialog = privacyPage.shadowRoot.querySelector('#protectionDialog');
    assertFalse(!!dialog);
  });
});
