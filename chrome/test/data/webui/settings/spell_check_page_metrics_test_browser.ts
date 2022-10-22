// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguageHelper, LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

const spell_check_page_metrics_test_browser = {
  TestNames: {
    SpellCheckMetrics: 'spell_check_metrics_all',
    // <if expr="_google_chrome">
    SpellCheckMetricsOfficialBuild: 'spell_check_metrics_official',
    // </if>
    // <if expr="not is_macosx">
    SpellCheckMetricsNotMacOSx: 'spell_check_not_macosx',
    // </if>
  },
};

Object.assign(window, {spell_check_page_metrics_test_browser});

/**
 * A test version of LanguageSettingsMetricsProxy.
 */
class TestSpellCheckSettingsMetricsProxy extends TestBrowserProxy implements
    LanguageSettingsMetricsProxy {
  constructor() {
    super(['recordSettingsMetric', 'recordPageImpressionMetric']);
  }

  recordSettingsMetric(interaction: LanguageSettingsActionType) {
    this.methodCalled('recordSettingsMetric', interaction);
  }

  recordPageImpressionMetric(interaction: LanguageSettingsPageImpressionType) {
    this.methodCalled('recordPageImpressionMetric', interaction);
  }
}

suite('SpellCheckPageMetricsBrowser', function() {
  let languageHelper: LanguageHelper;
  let spellCheckPage: SettingsSpellCheckPageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageSettingsMetricsProxy: TestSpellCheckSettingsMetricsProxy;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs()) as
        unknown as typeof chrome.settingsPrivate;
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Sets up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.setInstance(browserProxy);

      // Sets up test browser proxy.
      languageSettingsMetricsProxy = new TestSpellCheckSettingsMetricsProxy();
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

      spellCheckPage = document.createElement('settings-spell-check-page');

      // Prefs would normally be data-bound to settings-languages-page.
      spellCheckPage.prefs = settingsLanguages.prefs;
      fakeDataBind(settingsLanguages, spellCheckPage, 'prefs');

      spellCheckPage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, spellCheckPage, 'language-helper');

      spellCheckPage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, spellCheckPage, 'languages');

      document.body.appendChild(spellCheckPage);
      languageHelper = spellCheckPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetrics, function() {
    test('records when disabling spellCheck globally', async () => {
      spellCheckPage.setPrefValue('browser.enable_spellchecking', true);
      const spellCheckToggle = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#enableSpellcheckingToggle');
      assertTrue(!!spellCheckToggle, 'no spellCheckToggle');
      spellCheckToggle.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.DISABLE_SPELL_CHECK_GLOBALLY,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when enabling spellCheck globally', async () => {
      spellCheckPage.setPrefValue('browser.enable_spellchecking', false);

      const spellCheckToggle = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#enableSpellcheckingToggle');
      assertTrue(!!spellCheckToggle);
      spellCheckToggle.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.ENABLE_SPELL_CHECK_GLOBALLY,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });

  // <if expr="_google_chrome">
  suite(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetricsOfficialBuild, function() {
    test('records when selecting basic spell check', async () => {
      spellCheckPage.setPrefValue('spellcheck.use_spelling_service', true);
      const basicServiceSelect = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellingServiceDisable');
      assertTrue(!!basicServiceSelect);
      basicServiceSelect.click();
      flush();

      assertEquals(
        LanguageSettingsActionType.SELECT_BASIC_SPELL_CHECK,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when selecting enhanced spell check', async () => {
      spellCheckPage.setPrefValue('spellcheck.use_spelling_service', false);
      const enhancedServiceSelect = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellingServiceEnable');
      assertTrue(!!enhancedServiceSelect);
      enhancedServiceSelect.click();
      flush();

      assertEquals(
        LanguageSettingsActionType.SELECT_ENHANCED_SPELL_CHECK,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });
  // </if>

  // <if expr="not is_macosx">
  suite(spell_check_page_metrics_test_browser.TestNames.SpellCheckMetricsNotMacOSx, function() {
    test('records when enabling spellCheck for a language', async () => {
      spellCheckPage.setPrefValue('browser.enable_spellchecking', true);
      // enable language with support for spell check
      spellCheckPage.setPrefValue('spellcheck.dictionaries', ['en']);
      spellCheckPage.setPrefValue('spellcheck.dictionaries', ['nb']);

      const spellCheckLanguagesList = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellCheckLanguagesList');
      assertTrue(!!spellCheckLanguagesList);
      const spellCheckLanguageItem = spellCheckLanguagesList
          .querySelectorAll<HTMLElement>('.list-item')[1];
      assertTrue(!!spellCheckLanguageItem);
      spellCheckLanguageItem.querySelector('cr-toggle')!.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.ENABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when disabling spellCheck for a language', async () => {
      spellCheckPage.setPrefValue('browser.enable_spellchecking', true);
      // enable language with support for spell check
      languageHelper.enableLanguage('en');
      languageHelper.enableLanguage('af');

      const spellCheckLanguagesList = spellCheckPage.shadowRoot!
          .querySelector<HTMLElement>('#spellCheckLanguagesList');
      assertTrue(!!spellCheckLanguagesList);
      const spellCheckLanguageItem = spellCheckLanguagesList
          .querySelectorAll<HTMLElement>('.list-item')[1];
      assertTrue(!!spellCheckLanguageItem);
      spellCheckLanguageItem.querySelector('cr-toggle')!.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.DISABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });
  // </if>
});
