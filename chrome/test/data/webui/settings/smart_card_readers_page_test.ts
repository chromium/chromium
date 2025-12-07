// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSmartCardReadersPageElement} from 'chrome://settings/lazy_load.js';
import {ChooserType, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

// clang-format on
suite('SmartCardReadersPageSettings', function() {
  let testElement: SettingsSmartCardReadersPageElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      enableSmartCardReadersContentSetting: true,
    });
    resetRouterForTesting();
  });

  // Initialize the settings-smart-card-readers-page element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('settings-smart-card-readers-page');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  test('CorrectTypesArePassed', function() {
    const radioGroup = testElement.shadowRoot!.querySelector(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertEquals(ContentSettingsTypes.SMART_CARD_READERS, radioGroup.category);

    const chooserExceptionList =
        testElement.shadowRoot!.querySelector('chooser-exception-list');
    assertTrue(!!chooserExceptionList);
    assertEquals(
        ChooserType.SMART_CARD_READERS_DEVICES,
        chooserExceptionList.chooserType);
    assertEquals(
        ContentSettingsTypes.SMART_CARD_READERS, chooserExceptionList.category);
  });
});
