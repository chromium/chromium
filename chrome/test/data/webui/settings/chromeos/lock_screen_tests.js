// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('LockScreenPage', function() {
  let lockScreenPage = null;

  setup(function() {
    PolymerTest.clearBody();
    lockScreenPage = document.createElement('settings-lock-screen');
    document.body.appendChild(lockScreenPage);
    flush();
  });

  teardown(function() {
    lockScreenPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to Lock screen', async () => {
    const settingId = '1109';

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.LOCK_SCREEN, params);

    flush();

    const deepLinkElement =
        lockScreenPage.shadowRoot.querySelector('#enableLockScreen')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Lock screen toggle should be focused for settingId=' + settingId);
  });

  test('Lock screen options disabled by policy', async () => {
    const unlockTypeRadioGroup =
        lockScreenPage.shadowRoot.querySelector('#unlockType');
    assertTrue(!!unlockTypeRadioGroup, 'Unlock type radio group not found');

    await flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled');

    // This block is not really validating the change because it checks the same
    // state as initial. However it improves the reliability of the next assert
    // block as it adds some wait time.
    webUIListenerCallback('quick-unlock-disabled-by-policy-changed', false);
    await flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled after policy change');

    webUIListenerCallback('quick-unlock-disabled-by-policy-changed', true);
    await flushTasks();
    assertTrue(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly enabled after policy change');

    webUIListenerCallback('quick-unlock-disabled-by-policy-changed', false);
    await flushTasks();
    assertFalse(
        unlockTypeRadioGroup.disabled,
        'Unlock type radio group unexpectedly disabled after policy change');
  });
});
