// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {SettingsLockScreenElement} from 'chrome://os-settings/chromeos/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-lock-screen-subpage>', function() {
  let lockScreenPage: SettingsLockScreenElement|null = null;

  setup(function() {
    lockScreenPage = document.createElement('settings-lock-screen-subpage');
    document.body.appendChild(lockScreenPage);
    flush();
  });

  teardown(function() {
    lockScreenPage!.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to Lock screen', async () => {
    const settingId = '1109';

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.LOCK_SCREEN, params);

    flush();

    const deepLinkElement =
        lockScreenPage!.shadowRoot!.querySelector('#enableLockScreen')!
            .shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Lock screen toggle should be focused for settingId=' + settingId);
  });

  test('Lock screen options disabled by policy', async () => {
    const unlockTypeRadioGroup =
        lockScreenPage!.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#unlockType');
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
