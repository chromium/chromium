// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkPredictionOptions, PreloadingPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeSettingsPrivate} from './fake_settings_private.js';

suite('PreloadingPage', function() {
  function getFakePrefs() {
    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        // By default the pref is initialized to WIFI_ONLY_DEPRECATED, but then
        // treated as STANDARD. See chrome/browser/prefetch/prefetch_prefs.h for
        // more details.
        value: NetworkPredictionOptions.WIFI_ONLY_DEPRECATED,
      },
    ];
    return fakePrefs;
  }

  let preloadingPage: PreloadingPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs()) as
        unknown as typeof chrome.settingsPrivate;
    settingsPrefs.initialize(settingsPrivate);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Wait until settings are initialized to start tests.
    return CrSettingsPrefs.initialized.then(() => {
      preloadingPage = document.createElement('settings-preloading-page');
      preloadingPage.prefs = settingsPrefs.prefs!;
      document.body.appendChild(preloadingPage);
    });
  });

  test('testPreloadPagesDefault', function() {
    assertEquals(
        NetworkPredictionOptions.STANDARD,
        preloadingPage.prefs.net.network_prediction_options.value);
  });

  test('testPreloadPagesDisabled', function() {
    preloadingPage.$.preloadingDisabled.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.DISABLED,
        preloadingPage.prefs.net.network_prediction_options.value);
    assertTrue(preloadingPage.$.preloadingDisabled.checked);
  });

  test('testPreloadPagesStandard', function() {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingStandard actually updates the underlying pref.
    preloadingPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.DISABLED);

    preloadingPage.$.preloadingStandard.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        preloadingPage.prefs.net.network_prediction_options.value);
    assertTrue(preloadingPage.$.preloadingStandard.checked);
    assertTrue(preloadingPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtended', function() {
    preloadingPage.$.preloadingExtended.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.EXTENDED,
        preloadingPage.prefs.net.network_prediction_options.value);
    assertTrue(preloadingPage.$.preloadingExtended.checked);
    assertTrue(preloadingPage.$.preloadingExtended.expanded);
  });

  test('testPreloadPagesStandardExpand', function() {
    // By default, the preloadingStandard option will be selected and expanded.
    assertTrue(preloadingPage.$.preloadingStandard.expanded);

    preloadingPage.$.preloadingStandard.$.expandButton.click();
    flush();

    assertFalse(preloadingPage.$.preloadingStandard.expanded);

    preloadingPage.$.preloadingStandard.$.expandButton.click();
    flush();

    assertTrue(preloadingPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtendedExpand', function() {
    assertFalse(preloadingPage.$.preloadingExtended.expanded);

    preloadingPage.$.preloadingExtended.$.expandButton.click();
    flush();

    assertTrue(preloadingPage.$.preloadingExtended.expanded);

    preloadingPage.$.preloadingExtended.$.expandButton.click();
    flush();

    assertFalse(preloadingPage.$.preloadingExtended.expanded);
  });
});
