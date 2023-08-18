// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsResetBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {LifetimeBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from './test_os_lifetime_browser_proxy.js';
import {TestOsResetBrowserProxy} from './test_os_reset_browser_proxy.js';

suite('<os-settings-reset-page>', () => {
  let resetPage = null;

  /** @type {!settings.ResetPageBrowserProxy} */
  let resetPageBrowserProxy = null;

  /** @type {!LifetimeBrowserProxy} */
  let lifetimeBrowserProxy = null;

  /** @type {!ESimManagerRemote|undefined} */
  let eSimManagerRemote;

  suiteSetup(() => {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    resetPageBrowserProxy = new TestOsResetBrowserProxy();
    OsResetBrowserProxyImpl.setInstanceForTesting(resetPageBrowserProxy);
  });

  setup(async () => {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    Router.getInstance().navigateTo(routes.OS_RESET);
    PolymerTest.clearBody();
    resetPage = document.createElement('os-settings-reset-page');
    document.body.appendChild(resetPage);
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    resetPage.remove();
    lifetimeBrowserProxy.reset();
    resetPageBrowserProxy.reset();
  });

  test('Reset card should be visible', () => {
    const resetCard = resetPage.shadowRoot.querySelector('settings-reset-card');
    assertTrue(isVisible(resetCard));
  });
});
