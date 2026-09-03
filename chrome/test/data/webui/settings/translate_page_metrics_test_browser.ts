// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, SettingsAddLanguagesDialogElement, SettingsTranslatePageElement} from 'chrome://settings/lazy_load.js';
import {getLanguageHelperInstance, LanguageHelperImpl, LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguageSettingsMetricsProxy} from './test_languages_settings_metrics_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('TranslatePageMetricsBrowser', function() {
  let languageHelper: LanguageHelper;
  let translatePage: SettingsTranslatePageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageSettingsMetricsProxy: TestLanguageSettingsMetricsProxy;

  async function openAddLanguagesDialog(
      addButtonId: '#addAlwaysTranslate'|'#addNeverTranslate',
      dialogId: '#alwaysTranslateDialog'|
      '#neverTranslateDialog'): Promise<SettingsAddLanguagesDialogElement> {
    const whenDialogOpen = eventToPromise('cr-dialog-open', translatePage);
    translatePage.shadowRoot!.querySelector<HTMLElement>(addButtonId)!.click();
    await whenDialogOpen;

    const dialog =
        translatePage.shadowRoot!
            .querySelector<SettingsAddLanguagesDialogElement>(dialogId);
    assertTrue(!!dialog);
    return dialog;
  }

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

    // Sets up test metrics proxy.
    languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
    LanguageSettingsMetricsProxyImpl.setInstance(languageSettingsMetricsProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    translatePage = document.createElement('settings-translate-page');
    translatePage.languages = languageHelper.languages;
    languageHelper.addEventListener('languages-changed', (e: Event) => {
      translatePage.languages = (e as CustomEvent).detail;
    });

    document.body.appendChild(translatePage);
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
    PrefService.getInstance().setPrefValue('translate.enabled', true);
    translatePage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when enabling translate.enable toggle', async () => {
    PrefService.getInstance().setPrefValue('translate.enabled', false);
    translatePage.shadowRoot!
        .querySelector<HTMLElement>('#offerTranslateOtherLanguages')!.click();
    flush();

    assertEquals(
        LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when adding always translate language', async () => {
    const dialog = await openAddLanguagesDialog(
        '#addAlwaysTranslate', '#alwaysTranslateDialog');

    dialog.dispatchEvent(new CustomEvent('languages-added', {detail: ['sw']}));

    assertEquals(
        LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test(
      'records for each language when adding always translate languages',
      async () => {
        const dialog = await openAddLanguagesDialog(
            '#addAlwaysTranslate', '#alwaysTranslateDialog');

        dialog.dispatchEvent(
            new CustomEvent('languages-added', {detail: ['sw', 'no']}));

        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        assertEquals(
            2,
            languageSettingsMetricsProxy.getCallCount('recordSettingsMetric'));
        assertEquals(
            LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE,
            languageSettingsMetricsProxy.getArgs('recordSettingsMetric')[0]);
        assertEquals(
            LanguageSettingsActionType.ADD_TO_ALWAYS_TRANSLATE,
            languageSettingsMetricsProxy.getArgs('recordSettingsMetric')[1]);
      });

  test('records when removing always translate language', async () => {
    languageHelper.setLanguageAlwaysTranslateState('sw', true);
    flush();

    const removeButton = translatePage.shadowRoot!.querySelector<HTMLElement>(
        '#alwaysTranslateList .icon-delete-gray');
    assertTrue(!!removeButton);
    removeButton.click();

    assertEquals(
        LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test(
      'records for each language when removing always translate languages',
      async () => {
        languageHelper.setLanguageAlwaysTranslateState('sw', true);
        languageHelper.setLanguageAlwaysTranslateState('no', true);
        flush();

        const alwaysTranslateList =
            translatePage.shadowRoot!.querySelector<HTMLElement>(
                '#alwaysTranslateList');
        assertTrue(!!alwaysTranslateList);

        let removeButton =
            alwaysTranslateList.querySelector<HTMLElement>('.icon-delete-gray');
        assertTrue(!!removeButton);
        const firstCall =
            languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        removeButton.click();
        assertEquals(
            LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE,
            await firstCall);
        languageSettingsMetricsProxy.resetResolver('recordSettingsMetric');
        flush();

        removeButton =
            alwaysTranslateList.querySelector<HTMLElement>('.icon-delete-gray');
        assertTrue(!!removeButton);
        const secondCall =
            languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        removeButton.click();
        assertEquals(
            LanguageSettingsActionType.REMOVE_FROM_ALWAYS_TRANSLATE,
            await secondCall);
      });

  test('records when adding never translate language', async () => {
    const dialog = await openAddLanguagesDialog(
        '#addNeverTranslate', '#neverTranslateDialog');

    dialog.dispatchEvent(new CustomEvent('languages-added', {detail: ['sw']}));

    assertEquals(
        LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test(
      'records for each language when adding never translate languages',
      async () => {
        const dialog = await openAddLanguagesDialog(
            '#addNeverTranslate', '#neverTranslateDialog');

        dialog.dispatchEvent(
            new CustomEvent('languages-added', {detail: ['sw', 'no']}));

        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        assertEquals(
            2,
            languageSettingsMetricsProxy.getCallCount('recordSettingsMetric'));
        assertEquals(
            LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE,
            languageSettingsMetricsProxy.getArgs('recordSettingsMetric')[0]);
        assertEquals(
            LanguageSettingsActionType.ADD_TO_NEVER_TRANSLATE,
            languageSettingsMetricsProxy.getArgs('recordSettingsMetric')[1]);
      });

  test('records when removing never translate language', async () => {
    languageHelper.disableTranslateLanguage('sw');
    flush();

    const neverTranslateList =
        translatePage.shadowRoot!.querySelector<HTMLElement>(
            '#neverTranslateList');
    assertTrue(!!neverTranslateList);
    const removeButton = neverTranslateList.querySelector<HTMLElement>(
        '.icon-delete-gray:not([disabled])');
    assertTrue(!!removeButton);
    removeButton.click();

    assertEquals(
        LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test(
      'records for each language when removing never translate languages',
      async () => {
        // en-US is already on never-translate list by default in fake prefs.
        languageHelper.disableTranslateLanguage('sw');
        languageHelper.disableTranslateLanguage('no');
        flush();

        const neverTranslateList =
            translatePage.shadowRoot!.querySelector<HTMLElement>(
                '#neverTranslateList');
        assertTrue(!!neverTranslateList);

        let removeButton = neverTranslateList.querySelector<HTMLElement>(
            '.icon-delete-gray:not([disabled])');
        assertTrue(!!removeButton);
        const firstCall =
            languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        removeButton.click();
        assertEquals(
            LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE,
            await firstCall);
        languageSettingsMetricsProxy.resetResolver('recordSettingsMetric');
        flush();

        removeButton = neverTranslateList.querySelector<HTMLElement>(
            '.icon-delete-gray:not([disabled])');
        assertTrue(!!removeButton);
        const secondCall =
            languageSettingsMetricsProxy.whenCalled('recordSettingsMetric');
        removeButton.click();
        assertEquals(
            LanguageSettingsActionType.REMOVE_FROM_NEVER_TRANSLATE,
            await secondCall);
      });
});
