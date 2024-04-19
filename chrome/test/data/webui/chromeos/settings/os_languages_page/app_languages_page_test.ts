// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsAppLanguagesPageElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore} from 'chrome://os-settings/os_settings.js';
import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../app_management/fake_page_handler.js';
import {replaceBody, replaceStore, setupFakeHandler} from '../app_management/test_util.js';

type AppConfig = Partial<App>;

suite('<app-languages-page>', () => {
  const crIconButtonTag = 'cr-icon-button';
  const deviceLanguageLocaleTag = '';
  const deviceLanguageLabel = 'Device language';
  let appLanguagesPage: OsSettingsAppLanguagesPageElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    appLanguagesPage = document.createElement('os-settings-app-languages-page');
    appLanguagesPage.prefs = {
      arc: {
        last_set_app_locale: {
          key: 'arc.last_set_app_locale',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: '',
        },
      },
    };

    replaceBody(appLanguagesPage);
    flushTasks();
  });

  teardown(() => {
    appLanguagesPage.remove();
  });

  async function addApp(appId: string, arcOptions: AppConfig): Promise<void> {
    await fakeHandler.addApp(appId, arcOptions);
    await fakeHandler.flushPipesForTesting();
    flushTasks();
  }

  function getAppItems(): NodeListOf<HTMLDivElement> {
    return appLanguagesPage.shadowRoot!.querySelectorAll<HTMLDivElement>(
        '#appItem');
  }

  function getAppLanguagesDescriptionText(): string {
    const descriptionText =
        appLanguagesPage.shadowRoot!.querySelector<HTMLDivElement>(
            '#appLanguagesDescription');
    assertTrue(!!descriptionText, 'descriptionText not found');
    assertTrue(
        !!descriptionText.textContent, 'description textContent not found');
    return descriptionText.textContent;
  }

  function assertAppItem(
      list: NodeListOf<HTMLDivElement>, idx: number, title: string,
      selectedLanguage: string): void {
    const tag = `[appItem#${idx}]`;
    assertTrue(
        idx < list.length,
        `${tag} Invalid idx ${idx} is larger than list size ${list.length}`);
    const appItem = list[idx]!.querySelector('.app-info');
    assertTrue(!!appItem, `${tag} App item not found`);
    assertTrue(!!appItem.textContent, `${tag} Item textContent not found`);
    assertTrue(
        appItem.textContent.includes(title),
        `${tag} Invalid text ${appItem.textContent}`);

    const appLanguage = appItem.querySelector('.secondary');
    assertTrue(!!appLanguage, `${tag} App language not found`);
    assertTrue(
        !!appLanguage.textContent, `${tag} App language textContent not found`);
    assertTrue(
        appLanguage.textContent.includes(selectedLanguage),
        `${tag} Invalid text ${appLanguage.textContent}`);
  }

  test(
      'No apps with supported locales, show no-apps description text',
      async () => {
        const arcOptions: AppConfig = {
          type: AppType.kArc,
        };
        await addApp('no-supported-locales', arcOptions);

        assertStringContains(
            getAppLanguagesDescriptionText(),
            'No apps support app language selection');
      });

  test(
      'Multiple apps with supported locales, show supported apps, and ' +
          'description text',
      async () => {
        // Add app with selected locale.
        const testApp1Title = 'app1';
        const testDisplayName = 'testDisplayName';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          title: testApp1Title,
          supportedLocales: [{
            localeTag: 'test',
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: 'test',
            displayName: testDisplayName,
            nativeDisplayName: '',
          },
        };
        await addApp('supported-locales-with-selected-locale', arcOptions);

        // Add app with no selected locale (device language selected).
        const testApp2Title = 'app2';
        const arcOptions2: AppConfig = {
          type: AppType.kArc,
          title: testApp2Title,
          supportedLocales: [{
            localeTag: 'test',
            displayName: testDisplayName,
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: '',
            displayName: '',
            nativeDisplayName: '',
          },
        };
        await addApp('supported-locales-without-selected-locale', arcOptions2);

        const appItems = getAppItems();
        assertEquals(2, appItems.length);
        assertAppItem(appItems, /* idx= */ 0, testApp1Title, testDisplayName);
        assertAppItem(
            appItems, /* idx= */ 1, testApp2Title, deviceLanguageLabel);

        assertStringContains(
            getAppLanguagesDescriptionText(),
            'Only apps that support language selection are shown here');
      });

  test(
      'Click three-dots button and edit language, show edit language dialog',
      async () => {
        const testAppTitle = 'testAppTitle';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          title: testAppTitle,
          supportedLocales: [{
            localeTag: 'test',
            displayName: '',
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: '',
            displayName: '',
            nativeDisplayName: '',
          },
        };
        await addApp('click-three-dots-button-and-edit-language', arcOptions);

        // Clicks three-dots button to open dropdown menu.
        const appItems = getAppItems();
        assertEquals(1, appItems.length);
        const threeDotsButton = appItems[0]!.querySelector(crIconButtonTag);
        assertTrue(!!threeDotsButton, 'threeDotsButton not found');
        threeDotsButton.click();

        // Clicks edit language button to open edit dialog.
        const editLanguageButton =
            appLanguagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#editLanguage');
        assertTrue(!!editLanguageButton, 'editLanguageButton not found');
        editLanguageButton.click();
        // Wait until dialog is opened.
        flushTasks();

        // Verify edit dialog is opened and contains the correct app info.
        const appLanguageEditDialog =
            appLanguagesPage.shadowRoot!.querySelector(
                'app-language-selection-dialog');
        assertTrue(
            !!appLanguageEditDialog,
            'app-language-selection-dialog not found.');
        assertEquals(testAppTitle, appLanguageEditDialog.app.title);
      });

  test(
      'Click three-dots button and reset language, selected language is ' +
          'set to device language',
      async () => {
        const testAppId = 'click-three-dots-button-and-reset-language';
        const testLocaleTag = 'testLocaleTag';
        const arcOptions: AppConfig = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: testLocaleTag,
            displayName: '',
            nativeDisplayName: '',
          }],
          selectedLocale: {
            localeTag: testLocaleTag,
            displayName: '',
            nativeDisplayName: '',
          },
        };
        await addApp(testAppId, arcOptions);

        // Clicks three-dots button to open dropdown menu.
        const appItems = getAppItems();
        assertEquals(1, appItems.length);
        const threeDotsButton = appItems[0]!.querySelector(crIconButtonTag);
        assertTrue(!!threeDotsButton, 'threeDotsButton not found');
        threeDotsButton.click();

        // Clicks reset language button.
        const resetLanguageButton =
            appLanguagesPage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#resetLanguage');
        assertTrue(!!resetLanguageButton, 'resetLanguageButton not found');
        resetLanguageButton.click();
        // Wait for AppManagementStore to be updated.
        await fakeHandler.flushPipesForTesting();

        // Verify app language is reset back to device language.
        const app = AppManagementStore.getInstance().data.apps[testAppId];
        assertEquals(deviceLanguageLocaleTag, app!.selectedLocale!.localeTag);
      });
});
