// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {LanguageHelper, SettingsLanguagesPageElement} from 'chrome://settings/lazy_load.js';
import {getLanguageHelperInstance, LanguageHelperImpl, LanguagesBrowserProxyImpl, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="is_win">
import {LanguageSettingsActionType} from 'chrome://settings/lazy_load.js';

// </if>

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';
import {TestLanguageSettingsMetricsProxy} from './test_languages_settings_metrics_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('LanguagesPageMetricsBrowser', function() {
  let languageHelper: LanguageHelper;
  let languagesPage: SettingsLanguagesPageElement;
  let browserProxy: TestLanguagesBrowserProxy;
  let languageSettingsMetricsProxy: TestLanguageSettingsMetricsProxy;
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
    languageSettingsMetricsProxy = new TestLanguageSettingsMetricsProxy();
    LanguageSettingsMetricsProxyImpl.setInstance(languageSettingsMetricsProxy);

    LanguageHelperImpl.resetInstanceForTesting();
    languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    languagesPage = document.createElement('settings-languages-page');
    document.body.appendChild(languagesPage);
  });

  test('records when adding languages', async () => {
    languagesPage.$.addLanguages.click();
    await microtasksFinished();

    assertEquals(
        LanguageSettingsPageImpressionType.ADD_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  test('records when three-dot menu is opened', async () => {
    const menuButtons =
        languagesPage.$.languagesSection.querySelectorAll<HTMLElement>(
            '.list-item cr-icon-button.icon-more-vert');
    assertGT(menuButtons.length, 0);
    menuButtons[0]!.click();
    assertEquals(
        LanguageSettingsPageImpressionType.LANGUAGE_OVERFLOW_MENU_OPENED,
        await languageSettingsMetricsProxy.whenCalled(
            'recordPageImpressionMetric'));
  });

  // <if expr="is_win">
  test('records when chrome language is changed', async () => {
    // Adding language with supportsUI = true in
    // fake_language_settings_private.ts
    languageHelper.enableLanguage('sw');
    await microtasksFinished();
    // Testing the 'Change Chrome Language' button with 'sw'
    const languagesSection =
        languagesPage.shadowRoot.querySelector('#languagesSection');
    assertTrue(!!languagesSection);
    const menuButton = languagesSection.querySelector<HTMLElement>(
        '.list-item cr-icon-button#more-sw');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();
    const actionMenu = languagesPage.$.menu.get();
    assertTrue(actionMenu.open);
    const item = actionMenu.querySelector<HTMLElement>('#uiLanguageItem');
    assertTrue(!!item);
    item.click();
    assertEquals(
        LanguageSettingsActionType.CHANGE_CHROME_LANGUAGE,
        await languageSettingsMetricsProxy.whenCalled('recordSettingsMetric'));
  });
  // </if>

  test('records on language list reorder', async () => {
    // Add several languages.
    for (const language of ['en-CA', 'en-US', 'tk', 'no']) {
      languageHelper.enableLanguage(language);
    }

    await microtasksFinished();

    const menuButtons =
        languagesPage.$.languagesSection.querySelectorAll<HTMLElement>(
            '.list-item cr-icon-button.icon-more-vert');
    assertGT(menuButtons.length, 1);
    menuButtons[1]!.click();
    const actionMenu = languagesPage.$.menu.get();
    assertTrue(actionMenu.open);

    function getMenuItem(i18nKey: string): HTMLElement {
      const i18nString = loadTimeData.getString(i18nKey);
      assertTrue(!!i18nString);
      const menuItems =
          actionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
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
