// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history-sync-optin/history_sync_optin_app.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {browserProxyFactory, ScreenMode} from 'chrome://history-sync-optin/history_sync_optin.mojom-webui.js';
import type {PageRemote} from 'chrome://history-sync-optin/history_sync_optin.mojom-webui.js';
import type {HistorySyncOptinAppElement} from 'chrome://history-sync-optin/history_sync_optin_app.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistorySyncOptInHandler} from './test_history_sync_optin_browser_proxy.js';

suite('HistorySyncOptinAppTest', function() {
  let app: HistorySyncOptinAppElement;
  let handler: TestHistorySyncOptInHandler;
  let pageRemote: PageRemote;

  setup(async function() {
    handler = new TestHistorySyncOptInHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    pageRemote = remote;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('history-sync-optin-app');
    document.body.appendChild(app);
    await handler.whenCalled('requestAccountInfo');
  });

  // Checks that the buttons become disabled after one of them is clicked.
  async function testButtonClick(actionButton: CrButtonElement) {
    const allButtons =
        Array.from(app.shadowRoot.querySelectorAll('cr-button'));

    allButtons.forEach(button => assertFalse(button.disabled));

    assertTrue(!!actionButton);
    actionButton.click();
    await microtasksFinished();

    allButtons.forEach(button => assertTrue(button.disabled));
  }

  // Tests that the buttons are hidden by default.
  test('LoadPage', function() {
    const rejectButton = app.$.rejectButton;
    assertTrue(!!rejectButton);
    assertFalse(rejectButton.hidden);
    assertTrue(rejectButton.classList.contains('visibility-hidden'));

    const acceptButton =
        app.$.acceptButton;
    assertTrue(!!acceptButton);
    assertFalse(acceptButton.hidden);
    assertTrue(acceptButton.classList.contains('visibility-hidden'));
  });

  test('Clicking accept button', async function() {
    await testButtonClick(app.$.acceptButton);
    await handler.whenCalled('accept');
  });

  test('Clicking reject button', async function() {
    await testButtonClick(app.$.rejectButton);
    await handler.whenCalled('reject');
  });

  // Tests that the buttons are updated when the screen mode changes to
  // unrestricted.
  test('ScreenModeChangedUnrestricted', async () => {
    const rejectButton = app.$.rejectButton;
    const acceptButton = app.$.acceptButton;

    assertTrue(rejectButton.classList.contains('visibility-hidden'));
    assertTrue(acceptButton.classList.contains('visibility-hidden'));

    // Simulate the screen mode changing to unrestricted.
    pageRemote.sendScreenMode(ScreenMode.kUnrestricted);
    await microtasksFinished();

    assertFalse(rejectButton.classList.contains('visibility-hidden'));
    assertFalse(acceptButton.classList.contains('visibility-hidden'));
    assertTrue(rejectButton.classList.contains('tonal-button'));
    assertTrue(acceptButton.classList.contains('action-button'));
  });

  // Tests that the buttons are updated when the screen mode changes to
  // restricted.
  test('ScreenModeChangedRestricted', async () => {
    const rejectButton = app.$.rejectButton;
    const acceptButton = app.$.acceptButton;

    assertTrue(rejectButton.classList.contains('visibility-hidden'));
    assertTrue(acceptButton.classList.contains('visibility-hidden'));

    // Simulate the screen mode changing to unrestricted.
    pageRemote.sendScreenMode(ScreenMode.kRestricted);
    await microtasksFinished();

    assertFalse(rejectButton.classList.contains('visibility-hidden'));
    assertFalse(acceptButton.classList.contains('visibility-hidden'));
    // The buttons have no special class, they are equally weighted.
    assertFalse(rejectButton.classList.contains('tonal-button'));
    assertFalse(acceptButton.classList.contains('action-button'));
  });
});
