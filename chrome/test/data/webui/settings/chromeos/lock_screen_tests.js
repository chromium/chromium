// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

suite('LockScreenPage', function() {
  let lockScreenPage = null;

  setup(function() {
    PolymerTest.clearBody();
    lockScreenPage = document.createElement('settings-lock-screen');
    document.body.appendChild(lockScreenPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    lockScreenPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to Lock screen', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    const settingId =
        loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled') ? '1109' :
                                                                       '303';

    const params = new URLSearchParams;
    params.append('settingId', settingId);
    settings.Router.getInstance().navigateTo(
        settings.routes.LOCK_SCREEN, params);

    Polymer.dom.flush();

    const deepLinkElement =
        lockScreenPage.$$('#enableLockScreen').$$('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Lock screen toggle should be focused for settingId=' + settingId);
  });
});
