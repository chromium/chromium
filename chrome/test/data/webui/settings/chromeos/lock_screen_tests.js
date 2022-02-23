// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';
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
    const settingId = '1109';

    const params = new URLSearchParams;
    params.append('settingId', settingId);
    settings.Router.getInstance().navigateTo(
        settings.routes.LOCK_SCREEN, params);

    Polymer.dom.flush();

    const deepLinkElement = lockScreenPage.$$('#enableLockScreen')
                                .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Lock screen toggle should be focused for settingId=' + settingId);
  });

  test('Lock screen options disabled by policy', async () => {
    const unlockTypeRadioGroup = lockScreenPage.$$('#unlockType');
    assertTrue(!!unlockTypeRadioGroup, 'Unlock type radio group not found');

    await test_util.flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled');

    // This block is not really validating the change because it checks the same
    // state as initial. However it improves the reliability of the next assert
    // block as it adds some wait time.
    cr.webUIListenerCallback('quick-unlock-disabled-by-policy-changed', false);
    await test_util.flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled after policy change');

    cr.webUIListenerCallback('quick-unlock-disabled-by-policy-changed', true);
    await test_util.flushTasks();
    assertTrue(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly enabled after policy change');

    cr.webUIListenerCallback('quick-unlock-disabled-by-policy-changed', false);
    await test_util.flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled after policy change');
  });
});
