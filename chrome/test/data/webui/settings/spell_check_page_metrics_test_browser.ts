// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, LanguageSettingsMetricsProxy, LanguageSettingsPageImpressionType, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {getLanguageHelperInstance, LanguageHelperImpl, LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

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

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    // Sets up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    // Sets up test browser proxy.
    languageSettingsMetricsProxy = new TestSpellCheckSettingsMetricsProxy();
    LanguageSettingsMetricsProxyImpl.setInstance(languageSettingsMetricsProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    spellCheckPage = document.createElement('settings-spell-check-page');
    spellCheckPage.languages = languageHelper.languages;
    languageHelper.addEventListener('languages-changed', (e: Event) => {
      spellCheckPage.languages = (e as CustomEvent).detail;
    });

    document.body.appendChild(spellCheckPage);
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite('Metrics', function() {
    test('records when disabling spellCheck globally', async () => {
      PrefService.getInstance().setPrefValue(
          'browser.enable_spellchecking', true);
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
      PrefService.getInstance().setPrefValue(
          'browser.enable_spellchecking', false);

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
      PrefService.getInstance().setPrefValue(
          'spellcheck.use_spelling_service', true);
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
      PrefService.getInstance().setPrefValue(
          'spellcheck.use_spelling_service', false);
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
      assertTrue(PrefService.getInstance()
                     .getPref<boolean>('browser.enable_spellchecking')
                     .value);

      // Enable spellcheck only for the 1st entry.
      PrefService.getInstance().setPrefValue(
          'spellcheck.dictionaries', ['en-US']);

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
      assertTrue(PrefService.getInstance()
                     .getPref<boolean>('browser.enable_spellchecking')
                     .value);

      // Enable spellcheck for both language entries.
      PrefService.getInstance().setPrefValue(
          'spellcheck.dictionaries', ['en-US', 'sw']);

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
