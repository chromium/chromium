// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SmartCardReaderOriginEntryElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: SmartCardReaderOriginEntryElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('smart-card-reader-origin-entry');
    assertTrue(!!testElement);
    testElement.smartCardReaderName = 'reader';
    testElement.origin = 'foo.com';
    document.body.appendChild(testElement);
  });

  test('SmartCardReadersPage_IsPopulated', async function() {
    const buttons = testElement.shadowRoot!.querySelectorAll('#removeOrigin');
    assertEquals(buttons.length, 1);
    (buttons[0] as CrIconButtonElement).click();
    await browserProxy.whenCalled('revokeSmartCardReaderGrant');
    flush();
    assertEquals(browserProxy.getCallCount('revokeSmartCardReaderGrant'), 1);
    const lastArgs = browserProxy.getArgs('revokeSmartCardReaderGrant')[0];
    assertEquals(lastArgs[0], 'reader');
    assertEquals(lastArgs[1], 'foo.com');
  });
});
