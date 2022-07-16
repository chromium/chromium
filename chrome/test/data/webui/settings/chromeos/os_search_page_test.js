// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

suite('OSSearchPageTests', function() {
  /** @type {?SettingsSearchPageElement} */
  let page = null;

  setup(function() {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: false,
    });
    page = document.createElement('os-settings-search-page');
    document.body.appendChild(page);
    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to preferred search engine', async () => {
    const params = new URLSearchParams;
    params.append('settingId', '600');
    settings.Router.getInstance().navigateTo(
        settings.routes.OS_SEARCH, params);

    const deepLinkElement =
        page.$$('settings-search-engine').$$('#searchSelectionDialogButton');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Preferred search dropdown should be focused for settingId=600.');
  });
});
