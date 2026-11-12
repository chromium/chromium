// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {TrustedVaultErrorDialogElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl, TrustedVaultUnlockDialogAction, TrustedVaultUnlockDialogSource} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

function histogramName(source: TrustedVaultUnlockDialogSource): string {
  return `PasswordManager.TrustedVaultUnlockDialog.Action.${source}`;
}

suite('TrustedVaultErrorDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let dialog: TrustedVaultErrorDialogElement;
  let metrics: MetricsTracker;

  async function createDialog(source?: TrustedVaultUnlockDialogSource) {
    dialog = document.createElement('trusted-vault-error-dialog');
    if (source) {
      dialog.source = source;
    }
    document.body.appendChild(dialog);
    await microtasksFinished();
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    await createDialog();
  });

  test('dialog opens on connectedCallback', function() {
    assertTrue(dialog.$.dialog.open);
  });

  test('clicking cancel closes the dialog', function() {
    assertTrue(dialog.$.dialog.open);
    const cancelButton =
        dialog.shadowRoot.querySelector<HTMLElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    assertFalse(dialog.$.dialog.open);
  });

  test('clicking verify calls startTrustedVaultUnlock', async function() {
    assertTrue(dialog.$.dialog.open);
    const verifyButton =
        dialog.shadowRoot.querySelector<HTMLElement>('#verifyButton');
    assertTrue(!!verifyButton);
    verifyButton.click();
    await passwordManager.whenCalled('startTrustedVaultUnlock');
    assertFalse(dialog.$.dialog.open);
  });

  test('displays dialog texts', function() {
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogTitle'),
        dialog.shadowRoot.querySelector('.title-heading')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogDescription'),
        dialog.shadowRoot.querySelector('.body-text')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogContinue'),
        dialog.shadowRoot.querySelector('#verifyButton')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogNotNow'),
        dialog.shadowRoot.querySelector('#cancelButton')!.textContent.trim());
  });

  test('records shown with default source', function() {
    const name = histogramName(TrustedVaultUnlockDialogSource.AUTO_ON_LOAD);
    assertEquals(1, metrics.count(name));
    assertEquals(1, metrics.count(name, TrustedVaultUnlockDialogAction.SHOWN));
  });

  [TrustedVaultUnlockDialogSource.AUTO_ON_LOAD,
   TrustedVaultUnlockDialogSource.ADD_PASSWORD,
   TrustedVaultUnlockDialogSource.IMPORT,
  ].forEach(source => {
    test(`records shown for source ${source}`, async function() {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      metrics = fakeMetricsPrivate();
      await createDialog(source);

      assertEquals(1, metrics.count(histogramName(source)));
      assertEquals(
          1,
          metrics.count(
              histogramName(source), TrustedVaultUnlockDialogAction.SHOWN));
    });

    test(
        `records dismissed via Not now for source ${source}`, async function() {
          document.body.innerHTML = window.trustedTypes!.emptyHTML;
          metrics = fakeMetricsPrivate();
          await createDialog(source);

          const whenCancel = eventToPromise('cancel', dialog.$.dialog);
          dialog.shadowRoot.querySelector<HTMLElement>(
                               '#cancelButton')!.click();
          await whenCancel;

          assertEquals(2, metrics.count(histogramName(source)));
          assertEquals(
              1,
              metrics.count(
                  histogramName(source),
                  TrustedVaultUnlockDialogAction.DISMISSED));
          assertEquals(
              0,
              metrics.count(
                  histogramName(source),
                  TrustedVaultUnlockDialogAction.CONTINUE));
        });

    test(`records continue for source ${source}`, async function() {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      metrics = fakeMetricsPrivate();
      await createDialog(source);

      dialog.shadowRoot.querySelector<HTMLElement>('#verifyButton')!.click();
      await passwordManager.whenCalled('startTrustedVaultUnlock');
      await microtasksFinished();

      assertEquals(2, metrics.count(histogramName(source)));
      assertEquals(
          1,
          metrics.count(
              histogramName(source), TrustedVaultUnlockDialogAction.CONTINUE));
      assertEquals(
          0,
          metrics.count(
              histogramName(source), TrustedVaultUnlockDialogAction.DISMISSED));
    });
  });

  test('records dismissed when Esc is pressed', async function() {
    const name = histogramName(TrustedVaultUnlockDialogSource.AUTO_ON_LOAD);
    const whenCancel = eventToPromise('cancel', dialog.$.dialog);
    // Simulate the native 'cancel' event fired by <dialog> on Esc.
    dialog.$.dialog.getNative().dispatchEvent(
        new CustomEvent('cancel', {cancelable: true}));
    await whenCancel;
    await microtasksFinished();

    assertFalse(dialog.$.dialog.open);
    assertEquals(
        1, metrics.count(name, TrustedVaultUnlockDialogAction.DISMISSED));
    assertEquals(
        0, metrics.count(name, TrustedVaultUnlockDialogAction.CONTINUE));
  });
});
