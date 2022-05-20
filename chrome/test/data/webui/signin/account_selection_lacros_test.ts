// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import {AccountSelectionLacrosElement} from 'chrome://profile-picker/lazy_load.js';
import {AvailableAccount, ensureLazyLoaded, ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible, waitBeforeNextRender} from 'chrome://webui-test/test_util.js';

import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

[true, false].forEach(withinFlow => {
  const suiteSuffix = withinFlow ? 'WithinFlow' : 'OpenedDirectly';

  suite(`AccountSelectionLacrosTest${suiteSuffix}`, function() {
    let testElement: AccountSelectionLacrosElement;
    let browserProxy: TestManageProfilesBrowserProxy;

    /**
     * @param n Indicates the desired number of accounts.
     */
    function generateAccountsList(n: number): AvailableAccount[] {
      return Array(n).fill(null).map((_x, i) => ({
                                       gaiaId: `gaia-id-${i}`,
                                       name: `name-${i}`,
                                       email: `email-${i}`,
                                       accountImageUrl: `account-image-${i}`,
                                     }));
    }

    async function verifySelectAccountLacrosCalled(gaiaId: string) {
      const args = await browserProxy.whenCalled('selectAccountLacros');
      assertEquals(args[1], gaiaId);
      browserProxy.resetResolver('selectAccountLacros');
    }

    setup(async function() {
      browserProxy = new TestManageProfilesBrowserProxy();
      ManageProfilesBrowserProxyImpl.setInstance(browserProxy);

      // Simulate the history state (using navigation_mixin.ts breaks the test).
      history.pushState(
          {
            route: 'account-selection-lacros',
            step: 'accountSelectionLacros',
            isFirst: !withinFlow,
          },
          '', '/account-selection-lacros');

      document.body.innerHTML = '';
      testElement = document.createElement('account-selection-lacros');
      testElement.profileThemeInfo = browserProxy.profileThemeInfo;
      document.body.append(testElement);

      await Promise.all([
        browserProxy.whenCalled('getAvailableAccounts'),
        ensureLazyLoaded(),
      ]);
      browserProxy.reset();

      await waitBeforeNextRender(testElement);
    });

    test('BackButton', function() {
      assertEquals(isChildVisible(testElement, '#backButton'), withinFlow);
    });

    test('accountButtons', async function() {
      // There are no accounts initially, only "Use another account".
      flushTasks();
      let buttons = testElement.shadowRoot!.querySelectorAll<HTMLElement>(
          '.account-button');
      assertTrue(!!buttons);
      assertEquals(buttons.length, 1);
      // Add some accounts.
      webUIListenerCallback(
          'available-accounts-changed', generateAccountsList(3));
      flushTasks();
      buttons = testElement.shadowRoot!.querySelectorAll<HTMLElement>(
          '.account-button');
      assertTrue(!!buttons);
      assertEquals(buttons.length, 4);
      // Update the accounts again.
      webUIListenerCallback(
          'available-accounts-changed', generateAccountsList(2));
      flushTasks();
      buttons = testElement.shadowRoot!.querySelectorAll<HTMLElement>(
          '.account-button');
      assertTrue(!!buttons);
      assertEquals(buttons.length, 3);
      // Click account buttons.
      buttons[1]!.click();
      await verifySelectAccountLacrosCalled('gaia-id-0');
      buttons[2]!.click();
      await verifySelectAccountLacrosCalled('gaia-id-1');
      // Click "Use another account".
      buttons[0]!.click();
      await verifySelectAccountLacrosCalled('');
    });
  });
});
