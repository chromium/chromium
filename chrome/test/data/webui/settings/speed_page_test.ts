// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {NetworkPredictionOptions} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SpeedPageElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

  setup(async () => {
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    settingsPrefs.initialize(settingsPrivate);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Wait until settings are initialized to start tests.
    await CrSettingsPrefs.initialized;

    speedPage = document.createElement('settings-speed-page');
    speedPage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(speedPage);
    await microtasksFinished();
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

  test('testPreloadPagesStandardFromExtended', async () => {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    speedPage.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.EXTENDED);

    speedPage.$.preloadingStandard.click();
    await eventToPromise('selected-changed', speedPage.$.preloadingRadioGroup);

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtended', async () => {
    speedPage.$.preloadingExtended.click();
    await eventToPromise('selected-changed', speedPage.$.preloadingRadioGroup);

    assertEquals(
        NetworkPredictionOptions.EXTENDED,
        speedPage.prefs.net.network_prediction_options.value);
    assertTrue(speedPage.$.preloadingExtended.checked);
    assertTrue(speedPage.$.preloadingExtended.expanded);
  });

  test('testPreloadPagesStandardExpand', async function() {
    // By default, the preloadingStandard option will be selected and collapsed.
    assertFalse(speedPage.$.preloadingStandard.expanded);

    const expandButton = speedPage.$.preloadingStandard.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingStandard.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingStandard.expanded);
  });

  test('testPreloadPagesExtendedExpand', async function() {
    assertFalse(speedPage.$.preloadingExtended.expanded);

    const expandButton = speedPage.$.preloadingExtended.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingExtended.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingExtended.expanded);
  });
});
