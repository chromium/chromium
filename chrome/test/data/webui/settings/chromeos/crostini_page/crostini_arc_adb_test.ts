// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniPortSetting, SettingsCrostiniArcAdbElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

suite('<settings-crostini-arc-adb>', () => {
  let subpage: SettingsCrostiniArcAdbElement;

  function setCrostiniPrefs(enabled: boolean, {
    sharedPaths = {},
    forwardedPorts = [],
    micAllowed = false,
    arcEnabled = false,
    bruschettaInstalled = false,
  }: PrefParams = {}): void {
    subpage.prefs = {
      arc: {
        enabled: {value: arcEnabled},
      },
      bruschetta: {
        installed: {
          value: bruschettaInstalled,
        },
      },
      crostini: {
        enabled: {value: enabled},
        mic_allowed: {value: micAllowed},
        port_forwarding: {ports: {value: forwardedPorts}},
      },
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      },
    };
    flush();
  }

  setup(async () => {
    loadTimeData.overrideValues({
      arcAdbSideloadingSupported: true,
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });

    Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB);

    clearBody();
    subpage = document.createElement('settings-crostini-arc-adb');
    document.body.appendChild(subpage);
    setCrostiniPrefs(true, {arcEnabled: true});
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to enable adb debugging', async () => {
    const CROSTINI_ADB_DEBUGGING_SETTING =
        settingMojom.Setting.kCrostiniAdbDebugging.toString();
    const params = new URLSearchParams();
    params.append('settingId', CROSTINI_ADB_DEBUGGING_SETTING);
    Router.getInstance().navigateTo(routes.CROSTINI_ANDROID_ADB, params);

    flush();

    const deepLinkElement =
        subpage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#arcAdbEnabledButton');
    assertTrue(!!deepLinkElement);
    assertTrue(isVisible(deepLinkElement));

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Enable adb debugging button should be focused for settingId=${
            CROSTINI_ADB_DEBUGGING_SETTING}.`);
  });
});
