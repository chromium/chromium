// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ResetBrowserProxyImpl, Router, routes} from 'chrome://settings/settings.js';
import {TestResetBrowserProxy} from 'chrome://test/settings/test_reset_browser_proxy.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';
// clang-format on

/** @enum {string} */
const TestNames = {
  ResetProfileDialogAction: 'ResetProfileDialogAction',
  ResetProfileDialogOpenClose: 'ResetProfileDialogOpenClose',
  ResetProfileDialogOriginUnknown: 'ResetProfileDialogOriginUnknown',
  ResetProfileDialogOriginUserClick: 'ResetProfileDialogOriginUserClick',
  ResetProfileDialogOriginTriggeredReset:
      'ResetProfileDialogOriginTriggeredReset',
};

suite('DialogTests', function() {
  let resetPage = null;

  /** @type {!settings.ResetPageBrowserProxy} */
  let resetPageBrowserProxy = null;

  setup(function() {
    resetPageBrowserProxy = new TestResetBrowserProxy();
    ResetBrowserProxyImpl.instance_ = resetPageBrowserProxy;

    PolymerTest.clearBody();
    resetPage = document.createElement('settings-reset-page');
    document.body.appendChild(resetPage);
  });

  teardown(function() {
    resetPage.remove();
  });

  /**
   * @param {function(SettingsResetProfileDialogElement)}
   *     closeDialogFn A function to call for closing the dialog.
   */
  async function testOpenCloseResetProfileDialog(closeDialogFn) {
    resetPageBrowserProxy.resetResolver('onShowResetProfileDialog');
    resetPageBrowserProxy.resetResolver('onHideResetProfileDialog');

    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    flush();
    const dialog = resetPage.$$('settings-reset-profile-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    const whenDialogClosed = eventToPromise('close', dialog);

    await resetPageBrowserProxy.whenCalled('onShowResetProfileDialog');
    closeDialogFn(dialog);
    await Promise.all([
      whenDialogClosed,
      resetPageBrowserProxy.whenCalled('onHideResetProfileDialog'),
    ]);
  }

  // Tests that the reset profile dialog opens and closes correctly and that
  // resetPageBrowserProxy calls are occurring as expected.
  test(TestNames.ResetProfileDialogOpenClose, async function() {
    // Test case where the 'cancel' button is clicked.
    await testOpenCloseResetProfileDialog((dialog) => {
      dialog.$.cancel.click();
    });
    // Test case where the browser's 'back' button is clicked.
    await testOpenCloseResetProfileDialog((dialog) => {
      resetPage.currentRouteChanged(routes.BASIC);
    });
  });

  // Tests that when user request to reset the profile the appropriate
  // message is sent to the browser.
  test(TestNames.ResetProfileDialogAction, async function() {
    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    flush();
    const dialog = resetPage.$$('settings-reset-profile-dialog');
    assertTrue(!!dialog);

    const checkbox = dialog.$$('[slot=footer] cr-checkbox');
    assertTrue(checkbox.checked);
    const showReportedSettingsLink = dialog.$$('[slot=footer] a');
    assertTrue(!!showReportedSettingsLink);
    showReportedSettingsLink.click();

    await resetPageBrowserProxy.whenCalled('showReportedSettings');
    // Ensure that the checkbox was not toggled as a result of
    // clicking the link.
    assertTrue(checkbox.checked);
    assertFalse(dialog.$.reset.disabled);
    assertFalse(dialog.$.resetSpinner.active);
    dialog.$.reset.click();
    assertTrue(dialog.$.reset.disabled);
    assertTrue(dialog.$.cancel.disabled);
    assertTrue(dialog.$.resetSpinner.active);
    await resetPageBrowserProxy.whenCalled('performResetProfileSettings');
  });

  async function testResetRequestOrigin(expectedOrigin) {
    const dialog = resetPage.$$('settings-reset-profile-dialog');
    assertTrue(!!dialog);
    dialog.$.reset.click();
    const resetRequest =
        await resetPageBrowserProxy.whenCalled('performResetProfileSettings');
    assertEquals(expectedOrigin, resetRequest);
  }

  test(TestNames.ResetProfileDialogOriginUnknown, async function() {
    Router.getInstance().navigateTo(routes.RESET_DIALOG);
    await resetPageBrowserProxy.whenCalled('onShowResetProfileDialog');
    await testResetRequestOrigin('');
  });

  test(TestNames.ResetProfileDialogOriginUserClick, async function() {
    resetPage.$.resetProfile.click();
    await resetPageBrowserProxy.whenCalled('onShowResetProfileDialog');
    await testResetRequestOrigin('userclick');
  });

  test(TestNames.ResetProfileDialogOriginTriggeredReset, async function() {
    Router.getInstance().navigateTo(routes.TRIGGERED_RESET_DIALOG);
    await resetPageBrowserProxy.whenCalled('onShowResetProfileDialog');
    await testResetRequestOrigin('triggeredreset');
  });
});
