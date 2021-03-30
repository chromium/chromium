// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestKerberosAccountsBrowserProxy} from './test_kerberos_accounts_browser_proxy.m.js';
// #import {Router, Route, routes, KerberosAccountsBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// clang-format on

cr.define('settings_kerberos_page', function() {
  suite('KerberosPageTests', function() {
    let browserProxy = null;

    /** @type {SettingsKerberosPageElement} */
    let kerberosPage = null;

    setup(function() {
      settings.routes.BASIC = new settings.Route('/'),
      settings.routes.KERBEROS =
          settings.routes.BASIC.createSection('/kerberos', 'kerberos');
      settings.routes.KERBEROS_ACCOUNTS_V2 =
          settings.routes.KERBEROS.createChild('/kerberos/kerberosAccounts');

      settings.Router.resetInstanceForTesting(
          new settings.Router(settings.routes));

      browserProxy = new TestKerberosAccountsBrowserProxy();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
    });

    teardown(function() {
      kerberosPage.remove();
      settings.Router.getInstance().resetRouteForTesting();
      settings.KerberosAccountsBrowserProxyImpl.instance_ = undefined;
    });

    test('Kerberos Section contains a link to Kerberos Accounts', () => {
      kerberosPage = document.createElement('settings-kerberos-page');
      document.body.appendChild(kerberosPage);
      Polymer.dom.flush();

      // Sub-page trigger is shown.
      const subpageTrigger = kerberosPage.shadowRoot.querySelector(
          '#kerberos-accounts-subpage-trigger');
      assertFalse(subpageTrigger.hidden);

      // Sub-page trigger navigates to Kerberos Accounts V2.
      subpageTrigger.click();
      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.KERBEROS_ACCOUNTS_V2);
    });
  });

  // #cr_define_end
  return {};
});
