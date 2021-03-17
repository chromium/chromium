// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {fakeDataBind} from '../test_util.m.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguagesMetricsProxy} from './test_languages_metrics_proxy.js';

// TODO(crbug/1109431): Remove this test once migration is complete.
suite('LanguagesPageMetricsChromeOS', function() {
  /** @type {!LanguageHelper} */
  let languageHelper;
  /** @type {!SettingsLanguagesPageElement} */
  let languagesPage;
  /** @type {!TestLanguagesBrowserProxy} */
  let browserProxy;
  /** @type {!TestLanguagesMetricsProxy} */
  let languagesMetricsProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isChromeOSLanguagesSettingsUpdate: false,
    });
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(function() {
    document.body.innerHTML = '';
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);
    return CrSettingsPrefs.initialized.then(function() {
      // Sets up test browser proxy.
      browserProxy = new TestLanguagesBrowserProxy();
      LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Sets up test browser proxy.
      languagesMetricsProxy = new TestLanguagesMetricsProxy();
      LanguagesMetricsProxyImpl.instance_ = languagesMetricsProxy;

      // Sets up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      languagesPage = /** @type {!SettingsLanguagesPageElement} */ (
          document.createElement('settings-languages-page'));

      // Prefs would normally be data-bound to settings-languages-page.
      languagesPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, languagesPage, 'prefs');

      document.body.appendChild(languagesPage);
      languageHelper = languagesPage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  test('records when adding languages', async () => {
    languagesPage.$$('settings-languages-subpage').$$('#addLanguages').click();
    flush();

    await languagesMetricsProxy.whenCalled('recordAddLanguages');
  });

  test('records when clicking edit dictionary', async () => {
    languagesPage.$$('#spellCheckSubpageTrigger').click();
    flush();

    assertEquals(
        LanguagesPageInteraction.OPEN_CUSTOM_SPELL_CHECK,
        await languagesMetricsProxy.whenCalled('recordInteraction'));
  });

  test('records when disabling translate.enable toggle', async () => {
    languagesPage.$$('settings-languages-subpage')
        .setPrefValue('translate.enabled', true);
    languagesPage.$$('settings-languages-subpage')
        .$$('#offerTranslateOtherLanguages')
        .click();
    flush();

    assertFalse(
        await languagesMetricsProxy.whenCalled('recordToggleTranslate'));
  });

  test('records when enabling translate.enable toggle', async () => {
    languagesPage.$$('settings-languages-subpage')
        .setPrefValue('translate.enabled', false);
    languagesPage.$$('settings-languages-subpage')
        .$$('#offerTranslateOtherLanguages')
        .click();
    flush();

    assertTrue(await languagesMetricsProxy.whenCalled('recordToggleTranslate'));
  });

  test('records when disabling spell check toggle', async () => {
    languagesPage.setPrefValue('browser.enable_spellchecking', true);
    languagesPage.$$('#enableSpellcheckingToggle').click();
    flush();

    assertFalse(
        await languagesMetricsProxy.whenCalled('recordToggleSpellCheck'));
  });

  test('records when enabling spell check toggle', async () => {
    languagesPage.setPrefValue('browser.enable_spellchecking', false);
    languagesPage.$$('#enableSpellcheckingToggle').click();
    flush();

    assertTrue(
        await languagesMetricsProxy.whenCalled('recordToggleSpellCheck'));
  });

  test('records when switching system language and restarting', async () => {
    // Adds several languages.
    for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
      languageHelper.enableLanguage(language);
    }

    flush();

    const languagesCollapse = languagesPage.$$('#languagesCollapse');
    languagesCollapse.opened = true;

    const menuButtons =
        languagesPage.$$('settings-languages-subpage')
            .$$('#languagesSection')
            .querySelectorAll('.list-item cr-icon-button.icon-more-vert');

    // Chooses the second language to switch system language,
    // as first language is the default language.
    menuButtons[1].click();
    const actionMenu =
        languagesPage.$$('settings-languages-subpage').$$('#menu').get();
    assertTrue(actionMenu.open);
    const menuItems = actionMenu.querySelectorAll('.dropdown-item');
    for (const item of menuItems) {
      if (item.id === 'uiLanguageItem') {
        assertFalse(item.checked);
        item.click();

        assertEquals(
            LanguagesPageInteraction.SWITCH_SYSTEM_LANGUAGE,
            await languagesMetricsProxy.whenCalled('recordInteraction'));
        return;
      }
    }
    actionMenu.close();

    // Chooses restart button after switching system language.
    const restartButton =
        languagesPage.$$('settings-languages-subpage').$$('#restartButton');
    assertTrue(!!restartButton);
    restartButton.click();

    assertEquals(
        LanguagesPageInteraction.RESTART,
        await languagesMetricsProxy.whenCalled('recordInteraction'));
  });

  test('records when ticking translate checkbox', async () => {
    const languagesCollapse = languagesPage.$$('#languagesCollapse');
    languagesCollapse.opened = true;

    const menuButtons =
        languagesPage.$$('settings-languages-subpage')
            .$$('#languagesSection')
            .querySelectorAll('.list-item cr-icon-button.icon-more-vert');

    // Chooses the second language to change translate checkbox
    // as first language is the language used for translation.
    menuButtons[1].click();
    const actionMenu =
        languagesPage.$$('settings-languages-subpage').$$('#menu').get();
    assertTrue(actionMenu.open);
    const menuItems = actionMenu.querySelectorAll('.dropdown-item');
    for (const item of menuItems) {
      if (item.id === 'offerTranslations') {
        const checkedValue = item.checked;
        item.click();
        assertEquals(
            await languagesMetricsProxy.whenCalled(
                'recordTranslateCheckboxChanged'),
            !checkedValue);
        return;
      }
    }
  });
});
