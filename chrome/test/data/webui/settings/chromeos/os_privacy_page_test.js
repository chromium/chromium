// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {DataAccessPolicyState, MetricsConsentBrowserProxyImpl, MetricsConsentState, PeripheralDataAccessBrowserProxyImpl, Router, routes, SecureDnsMode} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.js';

const crosSettingPrefName = 'cros.device.peripheral_data_access_enabled';
const localStatePrefName =
    'settings.local_state_device_pci_data_access_enabled';
const deviceMetricsConsentPrefName = 'cros.metrics.reportingEnabled';
const userMetricsConsentPrefName = 'metrics.user_consent';

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
      isUserConfigurable: false
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

/**
 * @implements {MetricsConsentBrowserProxy}
 */
class TestMetricsConsentBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getMetricsConsentState',
      'updateMetricsConsent',
    ]);

    /** @type {MetricsConsentState} */
    this.state_ = {
      prefName: deviceMetricsConsentPrefName,
      isConfigurable: false
    };
  }

  /** @override */
  getMetricsConsentState() {
    this.methodCalled('getMetricsConsentState');
    return Promise.resolve(this.state_);
  }

  /** @override */
  updateMetricsConsent(consent) {
    this.methodCalled('updateMetricsConsent');
    return Promise.resolve(consent);
  }

  /**
   * @param {String} prefName
   * @param {Boolean} isConfigurable
   */
  setMetricsConsentState(prefName, isConfigurable) {
    this.state_.prefName = prefName;
    this.state_.isConfigurable = isConfigurable;
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
        }
      }
    }
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstance(browserProxy);
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

  test('Suggested content, pref disabled', async () => {
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    flush();

    // The default state of the pref is disabled.
    const suggestedContent =
        assert(privacyPage.shadowRoot.querySelector('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    // Update the backing pref to enabled.
    privacyPage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        }
      },
      'cros': {
        'device': {
          'peripheral_data_access_enabled': {
            value: true,
          }
        }
      },
      'dns_over_https': {
        'mode': {
          value: SecureDnsMode.AUTOMATIC
        },
        'templates': {
          value: ''
        }
      }
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
        privacyPage.shadowRoot.querySelector('#enableVerifiedAccess')
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
        privacyPage.shadowRoot.querySelector('settings-users-page')
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
        privacyPage.shadowRoot.querySelector('settings-users-page')
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
        privacyPage.shadowRoot.querySelector('settings-smart-privacy-page')
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
        privacyPage.shadowRoot.querySelector('settings-smart-privacy-page')
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

    assertEquals(Router.getInstance().getCurrentRoute(), routes.LOCK_SCREEN);
    const lockScreenPage =
        assert(privacyPage.shadowRoot.querySelector('#lockScreen'));

    // Password dialog should not open because the authToken_ is set.
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const editFingerprintsTrigger =
        lockScreenPage.shadowRoot.querySelector('#editFingerprints');
    editFingerprintsTrigger.click();
    flush();

    assertEquals(Router.getInstance().getCurrentRoute(), routes.FINGERPRINT);
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

    assertEquals(Router.getInstance().getCurrentRoute(), routes.FINGERPRINT);
    assertTrue(privacyPage.showPasswordPromptDialog_);
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

  // TODO(crbug.com/1262869): add a test for deep linking to snopping setting
  //                          once it has been added.
});

suite('PrivacePageTest_OfficialBuild', async () => {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        }
      },
      'metrics': {
        'reportingEnabled': {
          value: true,
        }
      }
    },
    'metrics': {
      'user_consent': {
        value: false,
      }
    },
    'dns_over_https':
        {'mode': {value: SecureDnsMode.AUTOMATIC}, 'templates': {value: ''}},
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  /** @type {?TestMetricsConsentBrowserProxy} */
  let metricsConsentBrowserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstance(browserProxy);

    metricsConsentBrowserProxy = new TestMetricsConsentBrowserProxy();
    MetricsConsentBrowserProxyImpl.setInstance(metricsConsentBrowserProxy);

    privacyPage = document.createElement('os-settings-privacy-page');
    PolymerTest.clearBody();
  });

  async function setUpPage(prefName, isConfigurable) {
    metricsConsentBrowserProxy.setMetricsConsentState(prefName, isConfigurable);

    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(privacyPage);
    flush();

    await metricsConsentBrowserProxy.whenCalled('getMetricsConsentState');
    await waitAfterNextRender();
    flush();
  }

  teardown(function() {
    privacyPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to send usage stats', async () => {
    await setUpPage(deviceMetricsConsentPrefName, /*isConfigurable=*/ true);

    const params = new URLSearchParams();
    params.append('settingId', '1103');
    Router.getInstance().navigateTo(routes.OS_PRIVACY, params);

    flush();

    const deepLinkElement =
        privacyPage.shadowRoot.querySelector('#enable-logging')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Send usage stats toggle should be focused for settingId=1103.');
  });

  test('Toggle disabled if metrics consent is not configurable', async () => {
    await setUpPage(deviceMetricsConsentPrefName, /*isConfigurable=*/ false);

    const toggle = privacyPage.shadowRoot.querySelector('#enable-logging')
                       .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(toggle);

    // The pref is true, so the toggle should be on.
    assertTrue(toggle.checked);

    // Not configurable, so toggle should be disabled.
    assertTrue(toggle.disabled);
  });

  test('Toggle enabled if metrics consent is configurable', async () => {
    await setUpPage(deviceMetricsConsentPrefName, /*is_configurable=*/ true);

    const toggle = privacyPage.shadowRoot.querySelector('#enable-logging')
                       .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(toggle);

    // The pref is true, so the toggle should be on.
    assertTrue(toggle.checked);

    // Configurable, so toggle should be enabled.
    assertFalse(toggle.disabled);

    // Toggle.
    toggle.click();
    await metricsConsentBrowserProxy.whenCalled('updateMetricsConsent');

    // Pref should be off now.
    assertFalse(toggle.checked);
  });

  test('Correct pref displayed', async () => {
    await setUpPage(userMetricsConsentPrefName, /*is_configurable=*/ true);

    const toggle = privacyPage.shadowRoot.querySelector('#enable-logging')
                       .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(toggle);

    // The user consent pref is false, so the toggle should not be checked.
    assertFalse(toggle.checked);

    // Configurable, so toggle should be enabled.
    assertFalse(toggle.disabled);

    // Toggle.
    toggle.click();
    await metricsConsentBrowserProxy.whenCalled('updateMetricsConsent');

    // Pref should be on now.
    assertTrue(toggle.checked);
  });
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
        }
      }
    },
    'settings': {'local_state_device_pci_data_access_enabled': {value: false}},
    'dns_over_https':
        {'mode': {value: SecureDnsMode.AUTOMATIC}, 'templates': {value: ''}},
  };

  /** @type {?TestPeripheralDataAccessBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstance(browserProxy);
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
