// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('PrivacyPageTests', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  setup(function() {
    PolymerTest.clearBody();
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    privacyPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Suggested content, visibility disabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    Polymer.dom.flush();

    assertEquals(null, privacyPage.$$('#suggested-content'));
  });

  test('Suggested content, visibility enabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    Polymer.dom.flush();

    // The default state of the pref is disabled.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
    loadTimeData.overrideValues({
      suggestedContentToggleEnabled: true,
    });

    // Update the backing pref to enabled.
    privacyPage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        }
      }
    };

    Polymer.dom.flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent = assert(privacyPage.$$('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });

  test('Deep link to verified access', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1101');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRIVACY, params);

    Polymer.dom.flush();

    const deepLinkElement =
        privacyPage.$$('#enable-verified-access').$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Verified access toggle should be focused for settingId=1101.');
  });

  test('Fingerprint dialog closes when token expires', async () => {
    loadTimeData.overrideValues({
      fingerprintUnlockEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    await test_util.waitAfterNextRender(privacyPage);

    if (!privacyPage.isAccountManagementFlowsV2Enabled_) {
      return;
    }

    const quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
    privacyPage.authToken_ = quickUnlockPrivateApi.getFakeToken();

    settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
    Polymer.dom.flush();

    const subpageTrigger = privacyPage.$$('#lockScreenSubpageTrigger');
    // Sub-page trigger navigates to the lock screen page.
    subpageTrigger.click();
    Polymer.dom.flush();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.LOCK_SCREEN);
    const lockScreenPage = assert(privacyPage.$$('#lockScreen'));

    // Password dialog should not open because the authToken_ is set.
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const editFingerprintsTrigger = lockScreenPage.$$('#editFingerprints');
    editFingerprintsTrigger.click();
    Polymer.dom.flush();

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.FINGERPRINT);
    assertFalse(privacyPage.showPasswordPromptDialog_);

    const fingerprintTrigger =
        privacyPage.$$('#fingerprint-list').$$('#addFingerprint');
    fingerprintTrigger.click();

    // Invalidate the auth token by firing an event.
    assertFalse(privacyPage.authToken_ === undefined);
    const event = new CustomEvent('invalidate-auth-token-requested');
    lockScreenPage.dispatchEvent(event);
    assertTrue(privacyPage.authToken_ === undefined);

    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.FINGERPRINT);
    assertTrue(privacyPage.showPasswordPromptDialog_);
  });
});

suite('PrivacePageTest_OfficialBuild', function() {
  /** @type {SettingsPrivacyPageElement} */
  let privacyPage = null;

  setup(function() {
    PolymerTest.clearBody();
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    privacyPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to send usage stats', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1103');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_PRIVACY, params);

    Polymer.dom.flush();

    const deepLinkElement = privacyPage.$$('#enable-logging').$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Send usage stats toggle should be focused for settingId=1103.');
  });
});
