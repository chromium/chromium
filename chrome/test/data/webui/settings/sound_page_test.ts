// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SoundPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefsBrowserProxy, PrefService, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

suite('SoundPage', function() {
  let testSiteSettingsPrefsBrowserProxy: TestSiteSettingsBrowserProxy;
  let page: SoundPageElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});
    resetRouterForTesting();

    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: 'generated.notification',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    testSiteSettingsPrefsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(testSiteSettingsPrefsBrowserProxy);

    page = document.createElement('settings-sound-page');
    document.body.appendChild(page);
  });

  test('UpdateStatus', async () => {
    assertTrue(page.$.blockAutoplaySetting.disabled);
    assertFalse(page.$.blockAutoplaySetting.checked);

    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: true},
      enabled: true,
    });

    await microtasksFinished();
    // Check that we are on and enabled.
    assertFalse(page.$.blockAutoplaySetting.disabled);
    assertTrue(page.$.blockAutoplaySetting.checked);

    // Toggle the pref off.
    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: false},
      enabled: true,
    });

    await microtasksFinished();
    // Check that we are off and enabled.
    assertFalse(page.$.blockAutoplaySetting.disabled);
    assertFalse(page.$.blockAutoplaySetting.checked);

    // Disable the autoplay status toggle.
    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: false},
      enabled: false,
    });

    await microtasksFinished();
    // Check that we are off and disabled.
    assertTrue(page.$.blockAutoplaySetting.disabled);
    assertFalse(page.$.blockAutoplaySetting.checked);
  });

  test('Hidden', () => {
    assertTrue(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertTrue(isVisible(page.$.blockAutoplaySetting));

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});
    resetRouterForTesting();

    page.remove();
    page = document.createElement('settings-sound-page');
    document.body.appendChild(page);

    assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertFalse(isVisible(page.$.blockAutoplaySetting));
  });

  test('Click', async () => {
    assertTrue(page.$.blockAutoplaySetting.disabled);
    assertFalse(page.$.blockAutoplaySetting.checked);

    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: true},
      enabled: true,
    });

    await microtasksFinished();
    // Check that we are on and enabled.
    assertFalse(page.$.blockAutoplaySetting.disabled);
    assertTrue(page.$.blockAutoplaySetting.checked);

    // Click on the toggle and wait for the proxy to be called.
    page.$.blockAutoplaySetting.click();
    const enabled = await testSiteSettingsPrefsBrowserProxy.whenCalled(
        'setBlockAutoplayEnabled');
    assertFalse(enabled);
  });
});
