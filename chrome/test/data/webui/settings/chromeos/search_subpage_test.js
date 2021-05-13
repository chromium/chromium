// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('SearchSubpage', function() {
  /** @type {SearchSubpageElement} */
  let page = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: true,
    });
  });

  setup(function() {
    page = document.createElement('settings-search-subpage');
    document.body.appendChild(page);
    Polymer.dom.flush();
  });

  teardown(function() {
    page.remove();
  });

  test('Deep link to Preferred Search Engine', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    const params = new URLSearchParams;
    params.append('settingId', '600');
    settings.Router.getInstance().navigateTo(
        settings.routes.SEARCH_SUBPAGE, params);

    const deepLinkElement =
        page.$$('settings-search-engine').$$('#searchSelectionDialogButton');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Preferred Search Engine button should be focused for settingId=600.');
  });

  test('Deep link to Quick Answers On/Off', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    const params = new URLSearchParams;
    params.append('settingId', '608');
    settings.Router.getInstance().navigateTo(
        settings.routes.SEARCH_SUBPAGE, params);

    const deepLinkElement = page.$$('#quick-answers-enable').$$('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer On/Off toggle should be focused for settingId=608.');
  });
});
