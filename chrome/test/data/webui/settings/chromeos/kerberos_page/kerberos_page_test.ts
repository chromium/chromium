// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KerberosAccountsBrowserProxyImpl, Route, Router, routes, SettingsKerberosPageElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.js';

suite('<settings-kerberos-page>', () => {
  let kerberosPage: SettingsKerberosPageElement;
  let browserProxy: TestKerberosAccountsBrowserProxy;

  setup(() => {
    routes.BASIC = new Route('/'),
    routes.KERBEROS = routes.BASIC.createSection('/kerberos', 'kerberos');
    routes.KERBEROS_ACCOUNTS_V2 =
        routes.KERBEROS.createChild('/kerberos/kerberosAccounts');

    Router.resetInstanceForTesting(new Router(routes));

    browserProxy = new TestKerberosAccountsBrowserProxy();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  teardown(() => {
    kerberosPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Kerberos Section contains a link to Kerberos Accounts', () => {
    kerberosPage = document.createElement('settings-kerberos-page');
    document.body.appendChild(kerberosPage);
    flush();

    // Sub-page trigger is shown.
    const subpageTrigger = kerberosPage.shadowRoot!.querySelector<HTMLElement>(
        '#kerberos-accounts-subpage-trigger');
    assert(subpageTrigger);
    assertFalse(subpageTrigger.hidden);

    // Sub-page trigger navigates to Kerberos Accounts V2.
    subpageTrigger.click();
    assertEquals(
        routes.KERBEROS_ACCOUNTS_V2, Router.getInstance().currentRoute);
  });
});
