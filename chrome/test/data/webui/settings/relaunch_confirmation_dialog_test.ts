// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LifetimeBrowserProxyImpl, RelaunchMixin, RestartType} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// clang-format on

class TestRelaunchMixinElement extends RelaunchMixin
(PolymerElement) {
  static get is() {
    return 'test-relaunch-mixin-element';
  }

  static get template() {
    return html`
    <template is="dom-if" if="[[shouldShowRelaunchDialog]]" restamp>
      <relaunch-confirmation-dialog restart-type="[[restartTypeEnum.RELAUNCH]]"
          on-close="onRelaunchDialogClose">
      </relaunch-confirmation-dialog>
    </template>`;
  }
}

customElements.define(TestRelaunchMixinElement.is, TestRelaunchMixinElement);

declare global {
  interface HTMLElementTagNameMap {
    'test-relaunch-mixin-element': TestRelaunchMixinElement;
  }
}

suite('RelaunchConfirmationDialogTestSuite', function() {
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let testRelaunchMixin: TestRelaunchMixinElement;

  setup(function() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testRelaunchMixin = document.createElement('test-relaunch-mixin-element');
    document.body.appendChild(testRelaunchMixin);
    flush();

    assertEquals(
        null,
        testRelaunchMixin.shadowRoot!.querySelector(
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
        testRelaunchMixin.shadowRoot!.querySelector(
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
        testRelaunchMixin.shadowRoot!.querySelector(
            'relaunch-confirmation-dialog')!;

    assertTrue(relaunchConfirmationDialogElement.$.dialog.open);
    relaunchConfirmationDialogElement.$.cancel.click();
    await eventToPromise('close', testRelaunchMixin);
    assertFalse(relaunchConfirmationDialogElement.$.dialog.open);

    // The dialog should be removed from the dom.
    assertFalse(!!testRelaunchMixin.shadowRoot!.querySelector(
        'relaunch-confirmation-dialog'));
  });
});
