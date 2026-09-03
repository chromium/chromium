// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CaptionsBrowserProxyImpl, getLanguageHelperInstance, LanguageHelperImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsLiveTranslateElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestCaptionsBrowserProxy} from './test_captions_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    ...getFakeLanguagePrefs(),
    {
      key: 'accessibility.captions.live_translate_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_translate_target_language',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en',
    },
  ];
}

suite('LiveTranslateSection', function() {
  let liveTranslateSection: SettingsLiveTranslateElement;
  let browserProxy: TestCaptionsBrowserProxy;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const initialPrefs = getInitialPrefs();
    const prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    const settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    LanguageHelperImpl.resetInstanceForTesting();
    const languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    // Set up test browser proxy.
    browserProxy = new TestCaptionsBrowserProxy();
    CaptionsBrowserProxyImpl.setInstance(browserProxy);

    liveTranslateSection = document.createElement('settings-live-translate');
    document.body.appendChild(liveTranslateSection);

    flush();
  });

  test('translate.enable toggle', function() {
    const settingsToggle =
        liveTranslateSection.shadowRoot.querySelector<HTMLElement>(
            '#liveTranslateToggleButton');
    assertTrue(!!settingsToggle);

    // Clicking on the toggle switches it to true.
    settingsToggle.click();
    let newToggleValue =
        prefService
            .getPref<boolean>('accessibility.captions.live_translate_enabled')
            .value;
    assertTrue(newToggleValue);

    // Clicking on the toggle switches it to false.
    settingsToggle.click();
    newToggleValue =
        prefService
            .getPref<boolean>('accessibility.captions.live_translate_enabled')
            .value;
    assertFalse(newToggleValue);
  });

  test('aria label for the target language dropdown menu', function() {
    prefService.setPrefValue(
        'accessibility.captions.live_translate_enabled', true);
    flush();

    const dropdown = liveTranslateSection.shadowRoot.querySelector(
        '#targetLanguageDropdown')!;
    const select = dropdown.shadowRoot!.querySelector('select')!;
    const expectedLabel =
        loadTimeData.getString('captionsLiveTranslateTargetLanguage');
    assertEquals(expectedLabel, select.getAttribute('aria-label'));
  });
});
