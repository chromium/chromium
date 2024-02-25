// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, SettingsTranslatePageElement} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import type {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguageSettingsMetricsProxy} from './test_languages_settings_metrics_proxy.js';

suite('TranslatePageMetricsBrowser', function() {
  let languageHelper: LanguageHelper;
  let translatePage: SettingsTranslatePageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageSettingsMetricsProxy: TestLanguageSettingsMetricsProxy;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Sets up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Sets up test browser proxy.
      languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
      LanguageSettingsMetricsProxyImpl.setInstance(
          languageSettingsMetricsProxy);

      // Sets up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      (languageSettingsPrivate as unknown as FakeLanguageSettingsPrivate)
          .setSettingsPrefs(settingsPrefs);

      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      translatePage = document.createElement('settings-translate-page');

      // Prefs would normally be data-bound to settings-languages-page.
      translatePage.prefs = settingsLanguages.prefs;
      fakeDataBind(settingsLanguages, translatePage, 'prefs');

      translatePage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, translatePage, 'language-helper');

      translatePage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, translatePage, 'languages');

      document.body.appendChild(translatePage);
      languageHelper = translatePage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  test('records when translate target is changed', async () => {
    const targetLanguageSelector = translatePage.shadowRoot!.querySelector
        <HTMLSelectElement>('#targetLanguage');
    assertTrue(!!targetLanguageSelector);

    targetLanguageSelector.value = 'sw';
    targetLanguageSelector.dispatchEvent(new CustomEvent('change'));

    assertEquals(
      LanguageSettingsActionType.CHANGE_TRANSLATE_TARGET,
      await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when disabling translate.enable toggle', async () => {
    translatePage.setPrefValue('translate.enabled', true);
    translatePage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when enabling translate.enable toggle', async () => {
    translatePage.setPrefValue('translate.enabled', false);
    translatePage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });
});
