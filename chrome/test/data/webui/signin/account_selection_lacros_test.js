// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountSelectionLacrosElement} from 'chrome://profile-picker/lazy_load.js';
import {ensureLazyLoaded, ManageProfilesBrowserProxyImpl} from 'chrome://profile-picker/profile_picker.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';

import {assertTrue} from '../chai_assert.js';
import {flushTasks, isChildVisible, waitBeforeNextRender} from '../test_util.js';
import {TestManageProfilesBrowserProxy} from './test_manage_profiles_browser_proxy.js';

suite('ProfileTypeChoiceTest', function() {
  /** @type {!AccountSelectionLacrosElement} */
  let testElement;

  /** @type {!TestManageProfilesBrowserProxy} */
  let browserProxy;

  /**
   * @param {number} n Indicates the desired number of accounts.
   * @return {!Array<!UnassignedAccounts>} Array of accounts.
   */
  function generateAccountsList(n) {
    return Array(n).fill(null).map((x, i) => ({
                                     gaiaId: `gaia-id-${i}`,
                                     name: `name-${i}`,
                                     email: `email-${i}`,
                                   }));
  }

  /**
   * @param {string} gaiaId
   */
  async function verifyLoadSignInProfileCreationFlowCalled(gaiaId) {
    const args = await browserProxy.whenCalled('loadSignInProfileCreationFlow');
    assertEquals(args[1], gaiaId);
    browserProxy.resetResolver('loadSignInProfileCreationFlow');
  }

  setup(async function() {
    browserProxy = new TestManageProfilesBrowserProxy();
    ManageProfilesBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = '';
    testElement = /** @type {!AccountSelectionLacrosElement} */ (
        document.createElement('account-selection-lacros'));
    testElement.profileThemeInfo = browserProxy.profileThemeInfo;
    document.body.append(testElement);

    await Promise.all([
      browserProxy.whenCalled('getUnassignedAccounts'),
      ensureLazyLoaded(),
    ]);
    browserProxy.reset();

    await waitBeforeNextRender(testElement);
  });

  test('BackButton', function() {
    assertTrue(isChildVisible(testElement, '#backButton'));
  });

  test('accountButtons', async function() {
    // There are no accounts initially, only "Use another account".
    flushTasks();
    let buttons = testElement.shadowRoot.querySelectorAll('.account-button');
    assertTrue(!!buttons);
    assertEquals(buttons.length, 1);
    // Add some accounts.
    webUIListenerCallback(
        'unassigned-accounts-changed', generateAccountsList(3));
    flushTasks();
    buttons = testElement.shadowRoot.querySelectorAll('.account-button');
    assertTrue(!!buttons);
    assertEquals(buttons.length, 4);
    // Update the accounts again.
    webUIListenerCallback(
        'unassigned-accounts-changed', generateAccountsList(2));
    flushTasks();
    buttons = testElement.shadowRoot.querySelectorAll('.account-button');
    assertTrue(!!buttons);
    assertEquals(buttons.length, 3);
    // Click account buttons.
    buttons[0].click();
    await verifyLoadSignInProfileCreationFlowCalled('gaia-id-0');
    buttons[1].click();
    await verifyLoadSignInProfileCreationFlowCalled('gaia-id-1');
    // Click "Use another account".
    buttons[2].click();
    await verifyLoadSignInProfileCreationFlowCalled('');
  });
});
