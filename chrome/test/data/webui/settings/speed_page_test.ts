// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkPredictionOptions} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, SettingsPrefsElement, SpeedPageElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

suite('SpeedPage', function() {
  function getFakePrefs() {
    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        // By default the pref is initialized to WIFI_ONLY_DEPRECATED, but then
        // treated as STANDARD. See chrome/browser/preloading/preloading_prefs.h
        // for more details.
        value: NetworkPredictionOptions.WIFI_ONLY_DEPRECATED,
      },
    ];
    return fakePrefs;
  }

  let speedPage: SpeedPageElement;
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
      speedPage = document.createElement('settings-speed-page');
      speedPage.prefs = settingsPrefs.prefs!;
      document.body.appendChild(speedPage);
    });
  });

  test('testPreloadPagesDefault', function() {
    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingToggle.checked);
  });

  test('testPreloadPagesDisabled', function() {
    speedPage.$.preloadingToggle.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.DISABLED,
        speedPage.prefs.net.network_prediction_options.value);
    assertFalse(speedPage.$.preloadingToggle.checked);
  });

  test('testPreloadPagesStandard', function() {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    speedPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.DISABLED);

    speedPage.$.preloadingToggle.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesStandardFromExtended', function() {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    speedPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.EXTENDED);

    speedPage.$.preloadingStandard.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtended', function() {
    speedPage.$.preloadingExtended.click();
    flush();

    assertEquals(
        NetworkPredictionOptions.EXTENDED,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingExtended.checked);
    assertTrue(speedPage.$.preloadingExtended.expanded);
  });

  test('testPreloadPagesStandardExpand', function() {
    // By default, the preloadingStandard option will be selected and expanded.
    assertTrue(speedPage.$.preloadingStandard.expanded);

    speedPage.$.preloadingStandard.$.expandButton.click();
    flush();

    assertFalse(speedPage.$.preloadingStandard.expanded);

    speedPage.$.preloadingStandard.$.expandButton.click();
    flush();

    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtendedExpand', function() {
    assertFalse(speedPage.$.preloadingExtended.expanded);

    speedPage.$.preloadingExtended.$.expandButton.click();
    flush();

    assertTrue(speedPage.$.preloadingExtended.expanded);

    speedPage.$.preloadingExtended.$.expandButton.click();
    flush();

    assertFalse(speedPage.$.preloadingExtended.expanded);
  });
});
