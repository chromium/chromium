// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCheckboxElement, SettingsResetPageElement, SettingsResetProfileDialogElement} from 'chrome://settings/lazy_load.js';
import {ResetBrowserProxyImpl, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestResetBrowserProxy} from './test_reset_browser_proxy.js';

// clang-format on

const TestNames = {
  ResetProfileDialogAction: 'ResetProfileDialogAction',
  ResetProfileDialogOpenClose: 'ResetProfileDialogOpenClose',
  ResetProfileDialogOriginUnknown: 'ResetProfileDialogOriginUnknown',
  ResetProfileDialogOriginUserClick: 'ResetProfileDialogOriginUserClick',
  ResetProfileDialogOriginTriggeredReset:
      'ResetProfileDialogOriginTriggeredReset',
};

suite('DialogTests', function() {
  let resetPage: SettingsResetPageElement;
  let resetPageBrowserProxy: TestResetBrowserProxy;

  setup(function() {
    resetPageBrowserProxy = new TestResetBrowserProxy();
    ResetBrowserProxyImpl.setInstance(resetPageBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetPage = document.createElement('settings-reset-page');
    document.body.appendChild(resetPage);
  });

  teardown(function() {
    resetPage.remove();
  });

  /**
   * @param closeDialogFn A function to call for closing the dialog.
   */
  async function testOpenCloseResetProfileDialog(
      closeDialogFn: (dialog: SettingsResetProfileDialogElement) => void) {
    resetPageBrowserProxy.resetResolver('onShowResetProfileDialog');
    resetPageBrowserProxy.resetResolver('onHideResetProfileDialog');

    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    flush();
    const dialog =
        resetPage.shadowRoot!.querySelector('settings-reset-profile-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog!.$.dialog.open);

    const whenDialogClosed = eventToPromise('close', dialog!);

    await resetPageBrowserProxy.whenCalled('onShowResetProfileDialog');
    closeDialogFn(dialog!);
    await Promise.all([
      whenDialogClosed,
      resetPageBrowserProxy.whenCalled('onHideResetProfileDialog'),
    ]);
  }

  // Tests that the reset profile dialog opens and closes correctly and that
  // resetPageBrowserProxy calls are occurring as expected.
  test(TestNames.ResetProfileDialogOpenClose, async function() {
    // Test case where the 'cancel' button is clicked.
    await testOpenCloseResetProfileDialog(dialog => {
      dialog.$.cancel.click();
    });
    // Test case where the browser's 'back' button is clicked.
    await testOpenCloseResetProfileDialog(_dialog => {
      resetPage.currentRouteChanged(routes.BASIC);
    });
  });

  // Tests that when user request to reset the profile the appropriate
  // message is sent to the browser.
  test(TestNames.ResetProfileDialogAction, async function() {
    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    flush();
    const dialog =
        resetPage.shadowRoot!.querySelector('settings-reset-profile-dialog');
    assertTrue(!!dialog);

    const checkbox = dialog!.shadowRoot!.querySelector<CrCheckboxElement>(
        '[slot=footer] cr-checkbox')!;
    assertTrue(checkbox.checked);
    const showReportedSettingsLink =
        dialog!.shadowRoot!.querySelector<HTMLElement>('[slot=footer] a');
    assertTrue(!!showReportedSettingsLink);
    showReportedSettingsLink!.click();

    await resetPageBrowserProxy.whenCalled('showReportedSettings');
    // Ensure that the checkbox was not toggled as a result of
    // clicking the link.
    assertTrue(checkbox.checked);
    assertFalse(dialog!.$.reset.disabled);
    assertFalse(dialog!.$.resetSpinner.active);
    dialog!.$.reset.click();
    assertTrue(dialog!.$.reset.disabled);
    assertTrue(dialog!.$.cancel.disabled);
    assertTrue(dialog!.$.resetSpinner.active);
    await resetPageBrowserProxy.whenCalled('performResetProfileSettings');
  });

  async function testResetRequestOrigin(expectedOrigin: string) {
    const dialog =
        resetPage.shadowRoot!.querySelector('settings-reset-profile-dialog');
    assertTrue(!!dialog);
    dialog!.$.reset.click();
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
