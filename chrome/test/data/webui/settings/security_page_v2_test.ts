// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSecurityPageV2Element} from 'chrome://settings/lazy_load.js';
import {SecuritySettingsBundleSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('Main', function() {
  let settingsPrefs: SettingsPrefsElement;
  let page: SettingsSecurityPageV2Element;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    page = document.createElement('settings-security-page-v2');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    flush();
  });

  test('StandardBundleIsInitiallySelected', function() {
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.prefs.generated.security_settings_bundle.value);
  });

  test('EnhanceBundleSelected', async function() {
    // Standard bundle is initially selected.
    assertEquals(
        SecuritySettingsBundleSetting.STANDARD,
        page.prefs.generated.security_settings_bundle.value);

    // Click on Enhanced bundle.
    page.$.securitySettingsBundleEnhanced.click();
    await microtasksFinished();
    assertEquals(
        SecuritySettingsBundleSetting.ENHANCED,
        page.prefs.generated.security_settings_bundle.value);
  });
});
