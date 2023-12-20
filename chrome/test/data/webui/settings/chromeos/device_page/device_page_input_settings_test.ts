// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Browser tests for Input settings on the Device page, specific to when the
 * OsSettingsRevampWayfinding feature is enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {LanguagesModel} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, DevicePageBrowserProxyImpl, ensureLazyLoaded, OsSettingsRoutes, OsSettingsSubpageElement, resetGlobalScrollTargetForTesting, Route, Router, routes, setGlobalScrollTargetForTesting, SettingsDevicePageElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeLanguageHelper} from '../os_languages_page/fake_language_helper.js';

import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

interface SubpageData {
  routeName: keyof OsSettingsRoutes;
  elementTagName: string;
}

suite('<settings-device-page> Input settings', () => {
  let settingsPrefs: SettingsPrefsElement;
  let devicePage: SettingsDevicePageElement;

  const fakeLanguagesModel: LanguagesModel = {
    supported: [],
    enabled: [],
    translateTarget: 'test',
    prospectiveUILanguage: undefined,
    inputMethods: {
      supported: [],
      enabled: [],
      currentId: 'fakeID',
      imeLanguagePackStatus: {},
    },
    alwaysTranslate: [],
    neverTranslate: [],
    spellCheckOnLanguages: [],
    spellCheckOffLanguages: [],
  };

  async function createPage(): Promise<void> {
    devicePage = document.createElement('settings-device-page');
    devicePage.languages = fakeLanguagesModel;
    devicePage.languageHelper = new FakeLanguageHelper();
    devicePage.prefs = settingsPrefs.prefs;
    document.body.appendChild(devicePage);
    await flushTasks();
  }

  async function navigateToSubpage(route: Route): Promise<void> {
    await ensureLazyLoaded();
    Router.getInstance().navigateTo(route);
    await flushTasks();
  }

  /**
   * Expects the os-settings-subpage parent element containing the subpage
   * element with the given tag name to be visible on the page.
   */
  function assertSubpageIsVisible(elementTagName: string): void {
    const subpageElement = devicePage.shadowRoot!.querySelector(elementTagName);
    assertTrue(!!subpageElement);
    const subpageParentElement = subpageElement.parentNode as HTMLElement;
    assertTrue(subpageParentElement instanceof OsSettingsSubpageElement);
    assertTrue(
        isVisible(subpageParentElement),
        `${elementTagName} should be visible.`);
  }

  suiteSetup(async () => {
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
  });

  suiteTeardown(() => {
    settingsPrefs.remove();
  });

  teardown(() => {
    devicePage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  setup(() => {
    loadTimeData.overrideValues({enableInputDeviceSettingsSplit: true});

    Router.getInstance().navigateTo(routes.DEVICE);

    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
  });

  suite('Input subpages', () => {
    setup(() => {
      // Necessary for os-settings-edit-dictionary-page which uses
      // GlobalScrollTargetMixin
      setGlobalScrollTargetForTesting(document.body);
    });

    teardown(() => {
      resetGlobalScrollTargetForTesting();
    });

    const inputSubpages: SubpageData[] = [
      {
        routeName: 'OS_LANGUAGES_INPUT',
        elementTagName: 'os-settings-input-page',
      },
      {
        routeName: 'OS_LANGUAGES_INPUT_METHOD_OPTIONS',
        elementTagName: 'settings-input-method-options-page',
      },
      {
        routeName: 'OS_LANGUAGES_EDIT_DICTIONARY',
        elementTagName: 'os-settings-edit-dictionary-page',
      },
      {
        routeName: 'OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY',
        elementTagName: 'os-settings-japanese-manage-user-dictionary-page',
      },
    ];
    inputSubpages.forEach(({routeName, elementTagName}) => {
      test(
          `${elementTagName} subpage element is visible for route ${routeName}`,
          async () => {
            await createPage();

            await navigateToSubpage(routes[routeName]);
            assertSubpageIsVisible(elementTagName);
          });
    });
  });

  test('Current input method is visible in Keyboard row sublabel', async () => {
    await createPage();

    const perDeviceKeyboardRow =
        devicePage.shadowRoot!.querySelector<CrLinkRowElement>(
            '#perDeviceKeyboardRow');
    assertTrue(!!perDeviceKeyboardRow);
    assertTrue(isVisible(perDeviceKeyboardRow));
    assertEquals('fake display name', perDeviceKeyboardRow.subLabel);
  });
});
