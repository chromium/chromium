// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LanguageHelper, LanguageSettingsMetricsProxy, LanguageSettingsPageImpressionType, SettingsSpellCheckPageElement} from 'chrome://settings/lazy_load.js';
import {getLanguageHelperInstance, LanguageHelperImpl, LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

// <if expr="_google_chrome or not is_macosx">
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
// </if>

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

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
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getFakeLanguagePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

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
    document.body.appendChild(spellCheckPage);
    await microtasksFinished();
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  suite('Metrics', function() {
    test('records when disabling spellCheck globally', async () => {
      prefService.setPrefValue('browser.enable_spellchecking', true);
      await microtasksFinished();
      const spellCheckToggle = spellCheckPage.$.enableSpellcheckingToggle;
      spellCheckToggle.click();
      await microtasksFinished();

      assertEquals(
          LanguageSettingsActionType.DISABLE_SPELL_CHECK_GLOBALLY,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when enabling spellCheck globally', async () => {
      prefService.setPrefValue('browser.enable_spellchecking', false);
      await microtasksFinished();

      const spellCheckToggle = spellCheckPage.$.enableSpellcheckingToggle;
      spellCheckToggle.click();
      await microtasksFinished();

      assertEquals(
          LanguageSettingsActionType.ENABLE_SPELL_CHECK_GLOBALLY,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });

  // <if expr="_google_chrome">
  suite('MetricsOfficialBuild', function() {
    test('records when selecting basic spell check', async () => {
      prefService.setPrefValue('spellcheck.use_spelling_service', true);
      await microtasksFinished();
      const basicServiceSelect =
          spellCheckPage.shadowRoot.querySelector<HTMLElement>(
              '#spellingServiceDisable');
      assertTrue(!!basicServiceSelect);
      basicServiceSelect.click();
      await microtasksFinished();

      assertEquals(
        LanguageSettingsActionType.SELECT_BASIC_SPELL_CHECK,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when selecting enhanced spell check', async () => {
      prefService.setPrefValue('spellcheck.use_spelling_service', false);
      await microtasksFinished();
      const enhancedServiceSelect = spellCheckPage.$.spellingServiceEnable;
      enhancedServiceSelect.click();
      await microtasksFinished();

      assertEquals(
        LanguageSettingsActionType.SELECT_ENHANCED_SPELL_CHECK,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });
  // </if>

  // <if expr="not is_macosx">
  suite('MetricsNotMacOS', function() {
    test('records when enabling spellCheck for a language', async () => {
      assertTrue(
          prefService.getPref<boolean>('browser.enable_spellchecking').value);

      // Enable spellcheck only for the 1st entry.
      prefService.setPrefValue('spellcheck.dictionaries', ['en-US']);

      const list = spellCheckPage.$.spellCheckLanguagesList;
      const listItems = list.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(2, listItems.length);

      const toggle = listItems[1]!.querySelector('cr-toggle');
      assertTrue(!!toggle);
      toggle.click();
      await microtasksFinished();

      assertEquals(
          LanguageSettingsActionType.ENABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });

    test('records when disabling spellCheck for a language', async () => {
      assertTrue(
          prefService.getPref<boolean>('browser.enable_spellchecking').value);

      // Enable spellcheck for both language entries.
      prefService.setPrefValue('spellcheck.dictionaries', ['en-US', 'sw']);

      const list = spellCheckPage.$.spellCheckLanguagesList;
      const listItems = list.querySelectorAll<HTMLElement>('.list-item');
      assertEquals(2, listItems.length);

      const toggle = listItems[1]!.querySelector('cr-toggle');
      assertTrue(!!toggle);
      toggle.click();
      await microtasksFinished();

      assertEquals(
          LanguageSettingsActionType.DISABLE_SPELL_CHECK_FOR_LANGUAGE,
          await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
    });
  });
  // </if>
});
