// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, LanguageSettingsMetricsProxy, LanguageSettingsPageImpressionType, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import type {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

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
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
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

  suite('Metrics', function() {
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
  suite('MetricsOfficialBuild', function() {
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
  suite('MetricsNotMacOS', function() {
    test('records when enabling spellCheck for a language', async () => {
      assertTrue(spellCheckPage.getPref('browser.enable_spellchecking').value);

      // Enable spellcheck only for the 1st entry.
      spellCheckPage.setPrefValue('spellcheck.dictionaries', ['en-US']);

      const list = spellCheckPage.shadowRoot!.querySelector<HTMLElement>(
          '#spellCheckLanguagesList');
      assertTrue(!!list);
      const listItems = list.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(2, listItems.length);

      const toggle = listItems[1]!.querySelector('cr-toggle');
      assertTrue(!!toggle);
      toggle.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.ENABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when disabling spellCheck for a language', async () => {
      assertTrue(spellCheckPage.getPref('browser.enable_spellchecking').value);

      // Enable spellcheck for both language entries.
      spellCheckPage.setPrefValue('spellcheck.dictionaries', ['en-US', 'sw']);

      const list = spellCheckPage.shadowRoot!.querySelector<HTMLElement>(
          '#spellCheckLanguagesList');
      assertTrue(!!list);
      const listItems = list.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(2, listItems.length);

      const toggle = listItems[1]!.querySelector('cr-toggle');
      assertTrue(!!toggle);
      toggle.click();
      flush();

      assertEquals(
          LanguageSettingsActionType.DISABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });
  // </if>
});
