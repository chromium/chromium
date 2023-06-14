// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {KerberosAccountsBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createSectionForTesting, createSubpageForTesting, Router, routes, routesMojom, SettingsKerberosPageElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.js';

suite('<settings-kerberos-page>', () => {
  let kerberosPage: SettingsKerberosPageElement;
  let browserProxy: TestKerberosAccountsBrowserProxy;

  suiteSetup(() => {
    routes.KERBEROS = createSectionForTesting(
        routes.BASIC, routesMojom.KERBEROS_SECTION_PATH,
        routesMojom.Section.kKerberos);
    routes.KERBEROS_ACCOUNTS_V2 = createSubpageForTesting(
        routes.KERBEROS, routesMojom.KERBEROS_ACCOUNTS_V2_SUBPAGE_PATH,
        routesMojom.Subpage.kKerberosAccountsV2);

    Router.resetInstanceForTesting(new Router(routes));
  });

  setup(() => {
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
        '#kerberosAccountsSubpageTrigger');
    assert(subpageTrigger);
    assertFalse(subpageTrigger.hidden);

    // Sub-page trigger navigates to Kerberos Accounts V2.
    subpageTrigger.click();
    assertEquals(
        routes.KERBEROS_ACCOUNTS_V2, Router.getInstance().currentRoute);
  });
});
