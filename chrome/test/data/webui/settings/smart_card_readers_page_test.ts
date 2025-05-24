// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsSmartCardReadersPageElement} from 'chrome://settings/lazy_load.js';
import {ChooserType, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('SmartCardReadersPageSettings', function() {
  let testElement: SettingsSmartCardReadersPageElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
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
