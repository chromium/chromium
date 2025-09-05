// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {V8PageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('V8Page', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = document.createElement('settings-v8-page');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(page);
    return flushTasks();
  });

  function isRadioButtonVisible(page: V8PageElement, selector: string) {
    const radioGroup =
        page.shadowRoot!.querySelector('#javascriptOptimizerRadioGroup');
    if (radioGroup === null) {
      return false;
    }
    return isChildVisible(radioGroup, selector, /*checkLightDom=*/ true);
  }

  test('CheckRadioButtons_BlockOnUnfamiliarSitesFeatureEnabled', function() {
    assertTrue(isRadioButtonVisible(page, '#blockForUnfamiliarSites'));
  });

  test('CheckRadioButtons_BlockOnUnfamiliarSitesFeatureDisabled', function() {
    assertFalse(isRadioButtonVisible(page, '#blockForUnfamiliarSites'));
  });
});
