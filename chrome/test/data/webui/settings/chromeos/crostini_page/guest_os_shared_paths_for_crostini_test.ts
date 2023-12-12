// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniBrowserProxyImpl, CrostiniPortSetting, GuestOsBrowserProxyImpl, SettingsCrostiniPageElement, SettingsGuestOsSharedPathsElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let subpage: SettingsGuestOsSharedPathsElement;
let guestOsBrowserProxy: TestGuestOsBrowserProxy;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

interface PrefParams {
  sharedPaths?: {[key: string]: string[]};
  forwardedPorts?: CrostiniPortSetting[];
  micAllowed?: boolean;
  arcEnabled?: boolean;
  bruschettaInstalled?: boolean;
}

function setCrostiniPrefs(enabled: boolean, {
  sharedPaths = {},
  forwardedPorts = [],
  micAllowed = false,
  arcEnabled = false,
  bruschettaInstalled = false,
}: PrefParams = {}): void {
  crostiniPage.prefs = {
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

// Functionality is already tested in OSSettingsGuestOsSharedPathsTest,
// so just check that we correctly set up the page for our 'termina' VM.
suite('Subpage shared paths', () => {
  setup(async () => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();

    disableAnimationsAndTransitions();

    setCrostiniPrefs(
        true, {sharedPaths: {path1: ['termina'], path2: ['some-other-vm']}});

    await flushTasks();
    Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);

    await flushTasks();
    const subpageElement = crostiniPage.shadowRoot!.querySelector(
        'settings-guest-os-shared-paths');
    assertTrue(!!subpageElement);
    subpage = subpageElement;
    await flushTasks();
  });

  test('Basic', () => {
    assertEquals(1, subpage.shadowRoot!.querySelectorAll('.list-item').length);
  });
});
