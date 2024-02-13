// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import type {AccountSelectionLacrosElement} from 'chrome://profile-picker/lazy_load.js';
import type {AvailableAccount} from 'chrome://profile-picker/profile_picker.js';
import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

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

    async function verifySelectNewAccountCalled() {
      await browserProxy.whenCalled('selectNewAccount');
      browserProxy.resetResolver('selectNewAccount');
    }

    async function verifySelectExistingAccountLacrosCalled(gaiaId: string) {
      const args = await browserProxy.whenCalled('selectExistingAccountLacros');
      assertEquals(args[1], gaiaId);
      browserProxy.resetResolver('selectExistingAccountLacros');
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

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    test('GuestLink', async function() {
      testElement.shadowRoot!.querySelector<HTMLElement>(
                                 '#guestModeLink')!.click();
      return browserProxy.whenCalled('openDeviceGuestLinkLacros');
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
      // Click "Use another account".
      buttons[0]!.click();
      await verifySelectNewAccountCalled();
      // Click account buttons.
      buttons[1]!.click();
      await verifySelectExistingAccountLacrosCalled('gaia-id-0');
    });

    test('accountButtonsDisabledAfterClick', async function() {
      flushTasks();
      // Add some accounts.
      webUIListenerCallback(
          'available-accounts-changed', generateAccountsList(3));
      flushTasks();
      const accountsButtons =
          testElement.shadowRoot!.querySelectorAll<HTMLButtonElement>(
              '#buttonsContainer > button');
      assertTrue(!!accountsButtons);
      assertEquals(accountsButtons.length, 3);
      accountsButtons[0]!.click();
      accountsButtons.forEach(button => {
        assertTrue(button.disabled);
      });
      const otherAccountButton =
          testElement.shadowRoot!.querySelector<HTMLButtonElement>(
              '#other-account-button');
      assertTrue(!!otherAccountButton);
      assertTrue(otherAccountButton.disabled);
    });
  });
});
