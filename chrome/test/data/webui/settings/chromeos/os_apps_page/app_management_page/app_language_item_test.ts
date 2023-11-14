// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementAppLanguageItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {isHidden, replaceBody, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-app-language-item>', () => {
  let appLanguageItem: AppManagementAppLanguageItemElement;
  let fakeHandler: FakePageHandler;

  setup(() => {
    fakeHandler = setupFakeHandler();
    appLanguageItem =
        document.createElement('app-management-app-language-item');

    replaceBody(appLanguageItem);
    flushTasks();
  });

  teardown(() => {
    appLanguageItem.remove();
  });

  test('No supported locales, hide app language settings label', async () => {
    const arcOptions = {
      type: AppType.kArc,
    };

    const arcApp = await fakeHandler.addApp('no-supported-locales', arcOptions);
    await fakeHandler.flushPipesForTesting();
    appLanguageItem.app = arcApp;

    assertTrue(isHidden(appLanguageItem));
  });

  test(
      'Supported locales exists without selected locale, ' +
          'show app language settings with default selected locale',
      async () => {
        const arcOptions = {
          type: AppType.kArc,
          supportedLocales: [{
            localeTag: 'test123',
            displayName: '',
            nativeDisplayName: '',
          }],
        };

        const arcApp = await fakeHandler.addApp(
            'supported-locales-default-selected-locale', arcOptions);
        await fakeHandler.flushPipesForTesting();
        appLanguageItem.app = arcApp;

        assertTrue(isVisible(appLanguageItem));
        const selectedLocaleLabel =
            appLanguageItem.shadowRoot!.querySelector('cr-link-row')!
                .shadowRoot!.querySelector('#labelWrapper > #subLabel');
        assertTrue(!!selectedLocaleLabel);
        assertTrue(
            selectedLocaleLabel.textContent!.includes('Device language'));
      });

  test('Selected locale exists, show display name', async () => {
    const displayName = 'English (United States)';
    const arcOptions = {
      type: AppType.kArc,
      supportedLocales: [{
        localeTag: 'en-US',
        displayName: displayName,
        nativeDisplayName: '',
      }],
      selectedLocale: {
        localeTag: 'en-US',
        displayName: displayName,
        nativeDisplayName: '',
      },
    };

    const arcApp = await fakeHandler.addApp(
        'supported-locales-with-selected-locale', arcOptions);
    await fakeHandler.flushPipesForTesting();
    appLanguageItem.app = arcApp;

    const selectedLocaleLabel =
        appLanguageItem.shadowRoot!.querySelector('cr-link-row')!.shadowRoot!
            .querySelector('#labelWrapper > #subLabel');
    assertTrue(!!selectedLocaleLabel);
    assertTrue(selectedLocaleLabel.textContent!.includes(displayName));
  });
});
