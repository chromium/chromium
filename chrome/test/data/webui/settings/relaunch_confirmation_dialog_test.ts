// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {LifetimeBrowserProxyImpl, RelaunchMixinLit, RestartType} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// clang-format on

class TestRelaunchMixinElement extends RelaunchMixinLit
(CrLitElement) {
  static get is() {
    return 'test-relaunch-mixin';
  }

  override render() {
    // clang-format off
    return html`
      ${this.shouldShowRelaunchDialog ? html`
        <relaunch-confirmation-dialog @close="${this.onRelaunchDialogClose}">
        </relaunch-confirmation-dialog>
      ` : ''}`;
    // clang-format on
  }
}

customElements.define(TestRelaunchMixinElement.is, TestRelaunchMixinElement);

declare global {
  interface HTMLElementTagNameMap {
    'test-relaunch-mixin': TestRelaunchMixinElement;
  }
}

suite('RelaunchConfirmationDialogTestSuite', function() {
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let testRelaunchMixin: TestRelaunchMixinElement;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testRelaunchMixin = document.createElement('test-relaunch-mixin');
    document.body.appendChild(testRelaunchMixin);

    assertEquals(
        null,
        testRelaunchMixin.shadowRoot.querySelector(
            'relaunch-confirmation-dialog'));
  });

  test('restart_WithoutDialog', function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(false);
    testRelaunchMixin.performRestart(RestartType.RESTART);
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('relaunch_WithoutDialog', function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(false);
    testRelaunchMixin.performRestart(RestartType.RELAUNCH);
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('relaunch_withDialog', async function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    lifetimeBrowserProxy.setRelaunchConfirmationDialogDescription(
        'Relaunch dialog body.');
    testRelaunchMixin.performRestart(RestartType.RELAUNCH);

    await eventToPromise('cr-dialog-open', testRelaunchMixin);
    const relaunchConfirmationDialogElement =
        testRelaunchMixin.shadowRoot.querySelector(
            'relaunch-confirmation-dialog')!;

    assertTrue(relaunchConfirmationDialogElement.$.dialog.open);
    relaunchConfirmationDialogElement.$.confirm.click();
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('relaunch_withDialog_AndCancel', async function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    lifetimeBrowserProxy.setRelaunchConfirmationDialogDescription(
        'Relaunch dialog body.');
    testRelaunchMixin.performRestart(RestartType.RELAUNCH);

    await eventToPromise('cr-dialog-open', testRelaunchMixin);
    const relaunchConfirmationDialogElement =
        testRelaunchMixin.shadowRoot.querySelector(
            'relaunch-confirmation-dialog')!;

    assertTrue(relaunchConfirmationDialogElement.$.dialog.open);
    relaunchConfirmationDialogElement.$.cancel.click();
    await eventToPromise('close', testRelaunchMixin);
    await microtasksFinished();
    assertFalse(relaunchConfirmationDialogElement.$.dialog.open);

    // The dialog should be removed from the dom.
    assertFalse(!!testRelaunchMixin.shadowRoot.querySelector(
        'relaunch-confirmation-dialog'));
  });

  test('onBeforeRestart_WithoutDialog', async function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(false);
    let onBeforeRestartCount = 0;
    testRelaunchMixin.performRestart(
        RestartType.RESTART, /*alwaysShowDialog=*/ false, () => {
          // Runs before the restart.
          assertEquals(0, lifetimeBrowserProxy.getCallCount('restart'));
          onBeforeRestartCount++;
        });
    await lifetimeBrowserProxy.whenCalled('restart');
    assertEquals(1, onBeforeRestartCount);
  });

  test('onBeforeRestart_withDialog', async function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    lifetimeBrowserProxy.setRelaunchConfirmationDialogDescription(
        'Relaunch dialog body.');
    let onBeforeRestartCount = 0;
    testRelaunchMixin.performRestart(
        RestartType.RELAUNCH, /*alwaysShowDialog=*/ false, () => {
          // Runs before the relaunch.
          assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
          onBeforeRestartCount++;
        });

    await eventToPromise('cr-dialog-open', testRelaunchMixin);
    const dialog = testRelaunchMixin.shadowRoot.querySelector(
        'relaunch-confirmation-dialog');
    assertTrue(!!dialog);
    assertEquals(0, onBeforeRestartCount);

    dialog.$.confirm.click();
    await lifetimeBrowserProxy.whenCalled('relaunch');
    assertEquals(1, onBeforeRestartCount);
  });

  test('onBeforeRestart_withDialog_AndCancel', async function() {
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    lifetimeBrowserProxy.setRelaunchConfirmationDialogDescription(
        'Relaunch dialog body.');
    let onBeforeRestartCount = 0;
    testRelaunchMixin.performRestart(
        RestartType.RELAUNCH, /*alwaysShowDialog=*/ false,
        () => onBeforeRestartCount++);

    await eventToPromise('cr-dialog-open', testRelaunchMixin);
    const dialog = testRelaunchMixin.shadowRoot.querySelector(
        'relaunch-confirmation-dialog');
    assertTrue(!!dialog);
    dialog.$.cancel.click();
    await eventToPromise('close', testRelaunchMixin);
    await microtasksFinished();

    assertEquals(0, onBeforeRestartCount);
    assertEquals(0, lifetimeBrowserProxy.getCallCount('relaunch'));
  });
});
