// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicLoginPermissionsPageElement} from 'chrome://settings/lazy_load.js';
import {GlicBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';

suite('GlicLoginPermissionsPage', function() {
  let page: SettingsGlicLoginPermissionsPageElement;
  let browserProxy: TestGlicBrowserProxy;

  const origin = 'example.com';
  const username = 'user';
  const displayName = 'Example';

  setup(async function() {
    browserProxy = new TestGlicBrowserProxy();
    GlicBrowserProxyImpl.setInstance(browserProxy);

    browserProxy.setActorLoginPermissions([{
      signonRealm: origin,
      username: username,
      displayName: displayName,
      faviconUrl: 'http://example.com/favicon.ico',
    }]);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-login-permissions-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  test('login permissions list is visible', () => {
    const loginPermissionsList =
        page.shadowRoot!.querySelector('#actorLoginPermissionsList');
    assertTrue(!!loginPermissionsList);
    assertEquals(
        1, page.shadowRoot!.querySelectorAll('.permission-item').length);
  });

  test('remove dialog is shown', async () => {
    // Check that the remove dialog is not shown.
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));

    // Click the remove button.
    const removeButton =
        page.shadowRoot!.querySelector<HTMLElement>('.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    // Check that the remove dialog is shown.
    const dialog =
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.bodyText.includes(displayName));

    // Close the dialog.
    const closePromise = eventToPromise('close', dialog);
    dialog.$.cancel.click();
    await closePromise;
    await flushTasks();

    // Check that the remove dialog is not shown.
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));
  });

  test('remove permission succeded', async () => {
    // Click the remove button.
    const removeButton =
        page.shadowRoot!.querySelector<HTMLElement>('.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    const dialog =
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);

    // Confirm dialog.
    const actionButton =
        dialog.shadowRoot!.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);
    const whenClose = eventToPromise('close', dialog);
    actionButton.click();

    const capturedSignonRealm =
        await browserProxy.whenCalled('revokeActorLoginPermission');
    assertEquals(origin, capturedSignonRealm);

    await whenClose;
    // Closing the dialog is done asynchronously, so still need to wait for the
    // dialog to close.
    await flushTasks();
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));

    const toast = page.shadowRoot!.querySelector<any>('#removeErrorToast');
    assertTrue(!!toast);
    assertFalse(toast.open);
  });

  test('remove permission failed', async () => {
    browserProxy.setRevokeActorLoginPermissionResponse(false);

    // Click the remove button.
    const removeButton =
        page.shadowRoot!.querySelector<HTMLElement>('.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    const dialog =
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog');
    assertTrue(!!dialog);
    // Confirm dialog.
    const actionButton =
        dialog.shadowRoot!.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);
    const whenClose = eventToPromise('close', dialog);
    actionButton.click();

    const capturedSignonRealm =
        await browserProxy.whenCalled('revokeActorLoginPermission');
    assertEquals(origin, capturedSignonRealm);

    await whenClose;
    // Handling the closed dialog is done asynchronously, so still need to wait.
    await flushTasks();
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));

    // Check that the error toast is shown.
    const toast = page.shadowRoot!.querySelector<any>('#removeErrorToast');
    assertTrue(!!toast);
    assertTrue(toast.open);
  });

  test('offline warning is shown', async () => {
    // Online by default.
    assertFalse(isVisible(page.shadowRoot!.querySelector('#offlineWarning')));

    const whenOffline = eventToPromise('offline', window);
    window.dispatchEvent(new Event('offline'));
    await whenOffline;

    assertTrue(isVisible(page.shadowRoot!.querySelector('#offlineWarning')));

    const whenOnline = eventToPromise('online', window);
    window.dispatchEvent(new Event('online'));
    await whenOnline;

    assertFalse(isVisible(page.shadowRoot!.querySelector('#offlineWarning')));
    const loginPermissionsList =
        page.shadowRoot!.querySelector('#actorLoginPermissionsList');
    assertTrue(!!loginPermissionsList);
    assertEquals(
        1, page.shadowRoot!.querySelectorAll('.permission-item').length);
  });
});
