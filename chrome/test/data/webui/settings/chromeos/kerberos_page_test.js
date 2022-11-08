// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KerberosAccountsBrowserProxyImpl, Route, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.js';

suite('KerberosPageTests', function() {
  let browserProxy = null;

  /** @type {SettingsKerberosPageElement} */
  let kerberosPage = null;

  setup(function() {
    routes.BASIC = new Route('/'),
    routes.KERBEROS = routes.BASIC.createSection('/kerberos', 'kerberos');
    routes.KERBEROS_ACCOUNTS_V2 =
        routes.KERBEROS.createChild('/kerberos/kerberosAccounts');

    Router.resetInstanceForTesting(new Router(routes));

    browserProxy = new TestKerberosAccountsBrowserProxy();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
  });

  teardown(function() {
    kerberosPage.remove();
    Router.getInstance().resetRouteForTesting();
    KerberosAccountsBrowserProxyImpl.setInstanceForTesting(undefined);
  });

  test('Kerberos Section contains a link to Kerberos Accounts', () => {
    kerberosPage = document.createElement('settings-kerberos-page');
    document.body.appendChild(kerberosPage);
    flush();

    // Sub-page trigger is shown.
    const subpageTrigger = kerberosPage.shadowRoot.querySelector(
        '#kerberos-accounts-subpage-trigger');
    assertFalse(subpageTrigger.hidden);

    // Sub-page trigger navigates to Kerberos Accounts V2.
    subpageTrigger.click();
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.KERBEROS_ACCOUNTS_V2);
  });
});
