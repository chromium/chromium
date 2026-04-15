// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, SettingsAddLanguagesDialogElement, SettingsTranslatePageElement} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import type {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguageSettingsMetricsProxy} from './test_languages_settings_metrics_proxy.js';

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
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);

    await CrSettingsPrefs.initialized;
    // Sets up test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    // Sets up test metrics proxy.
    languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
    LanguageSettingsMetricsProxyImpl.setInstance(languageSettingsMetricsProxy);

    // Sets up fake languageSettingsPrivate API.
    const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
    (languageSettingsPrivate as unknown as FakeLanguageSettingsPrivate)
        .setSettingsPrefs(settingsPrefs);

    const settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);
    languageHelper = settingsLanguages;

    translatePage = document.createElement('settings-translate-page');

    // Prefs would normally be data-bound to settings-languages-page.
    translatePage.prefs = settingsLanguages.prefs;
    fakeDataBind(settingsLanguages, translatePage, 'prefs');

    translatePage.languages = settingsLanguages.languages;
    fakeDataBind(settingsLanguages, translatePage, 'languages');

    document.body.appendChild(translatePage);
    await settingsLanguages.whenReady();
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
