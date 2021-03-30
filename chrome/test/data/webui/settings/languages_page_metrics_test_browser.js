// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl, LanguageSettingsActionType, LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';
import {fakeDataBind} from '../test_util.m.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

/**
 * A test version of LanguageSettingsMetricsProxy.
 *
 * @implements {LanguageSettingsMetricsProxy}
 *
 */
class TestLanguageSettingsMetricsProxy extends TestBrowserProxy {
  constructor() {
    super(['recordSettingsMetric', 'recordPageImpressionMetric']);
  }

  /** @override */
  recordSettingsMetric(interaction) {
    this.methodCalled('recordSettingsMetric', interaction);
  }

  /** @override */
  recordPageImpressionMetric(interaction) {
    this.methodCalled('recordPageImpressionMetric', interaction);
  }
}

suite('LanguagesPageMetricsBrowser', function() {
  /** @type {!LanguageHelper} */
  let languageHelper;
  /** @type {!SettingsLanguagesPageElement} */
  let languagesSubpage;
  /** @type {!TestLanguagesBrowserProxy} */
  let browserProxy;
  /** @type {!TestLanguageSettingsMetricsProxy} */
  let languageSettingsMetricsProxy;

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
      languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
      LanguageSettingsMetricsProxyImpl.instance_ = languageSettingsMetricsProxy;

      // Sets up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      languagesSubpage = /** @type {!SettingsLanguagesPageElement} */ (
          document.createElement('settings-languages-subpage'));

      // Prefs would normally be data-bound to settings-languages-page.
      languagesSubpage.prefs = settingsLanguages.prefs;
      fakeDataBind(settingsLanguages, languagesSubpage, 'prefs');

      languagesSubpage.languageHelper = settingsLanguages.languageHelper;
      fakeDataBind(settingsLanguages, languagesSubpage, 'language-helper');

      languagesSubpage.languages = settingsLanguages.languages;
      fakeDataBind(settingsLanguages, languagesSubpage, 'languages');

      document.body.appendChild(languagesSubpage);
      languageHelper = languagesSubpage.languageHelper;
      return languageHelper.whenReady();
    });
  });

  test('records when adding languages', async () => {
    languagesSubpage.$$('#addLanguages').click();
    flush();

    assertEquals(
        LanguageSettingsPageImpressionType.ADD_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  test('records when disabling translate.enable toggle', async () => {
    languagesSubpage.setPrefValue('translate.enabled', true);
    languagesSubpage.$$('#offerTranslateOtherLanguages').click();
    flush();

    assertEquals(
        LanguageSettingsActionType.DISABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when enabling translate.enable toggle', async () => {
    languagesSubpage.setPrefValue('translate.enabled', false);
    languagesSubpage.$$('#offerTranslateOtherLanguages').click();
    flush();

    assertEquals(
        LanguageSettingsActionType.ENABLE_TRANSLATE_GLOBALLY,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });

  test('records when three-dot menu is opened', async () => {
    const menuButtons =
        languagesSubpage.$$('#languagesSection')
            .querySelectorAll('.list-item cr-icon-button.icon-more-vert');

    menuButtons[0].click();
    assertEquals(
        LanguageSettingsPageImpressionType.LANGUAGE_OVERFLOW_MENU_OPENED,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  test('records when ticking translate checkbox', async () => {
    const menuButtons =
        languagesSubpage.$$('#languagesSection')
            .querySelectorAll('.list-item cr-icon-button.icon-more-vert');

    // Chooses the second language to change translate checkbox
    // as first language is the language used for translation.
    menuButtons[1].click();
    const actionMenu = languagesSubpage.$$('#menu').get();
    assertTrue(actionMenu.open);
    const menuItems = actionMenu.querySelectorAll('.dropdown-item');
    for (const item of menuItems) {
      if (item.id === 'offerTranslations') {
        const checkedValue = item.checked;
        item.click();
        assertEquals(
            LanguageSettingsActionType.DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE +
                item.checked,
            await languageSettingsMetricsProxy.whenCalled(
                'recordSettingsMetric'));
        return;
      }
    }
  });

  test('records on language list reorder', async () => {
    // Add several languages.
    for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
      languageHelper.enableLanguage(language);
    }

    flush();

    const menuButtons =
        languagesSubpage.$$('#languagesSection')
            .querySelectorAll('.list-item cr-icon-button.icon-more-vert');

    menuButtons[1].click();
    const actionMenu = languagesSubpage.$$('#menu').get();
    assertTrue(actionMenu.open);

    function getMenuItem(i18nKey) {
      const i18nString = loadTimeData.getString(i18nKey);
      assertTrue(!!i18nString);
      const menuItems = actionMenu.querySelectorAll('.dropdown-item');
      const menuItem = Array.from(menuItems).find(
          item => item.textContent.trim() === i18nString);
      assertTrue(!!menuItem, 'Menu item "' + i18nKey + '" not found');
      return menuItem;
    }
    let moveButton = getMenuItem('moveUp');
    moveButton.click();
    moveButton = getMenuItem('moveDown');
    moveButton.click();
    moveButton = getMenuItem('moveToTop');
    moveButton.click();
    assertEquals(
        3, languageSettingsMetricsProxy.getCallCount('recordSettingsMetric'));
  });
});