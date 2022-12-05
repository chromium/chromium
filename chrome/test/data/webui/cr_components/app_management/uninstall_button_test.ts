// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-management-uninstall-button. */
import 'chrome://resources/cr_components/app_management/uninstall_button.js';

import {App, InstallReason} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManamentUninstallButtonElement} from 'chrome://resources/cr_components/app_management/uninstall_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createTestApp, TestAppManagementBrowserProxy} from './app_management_test_support.js';

suite('AppManagementUninstallButtonTest', () => {
  let uninstallButton: AppManamentUninstallButtonElement;
  let testProxy: TestAppManagementBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestAppManagementBrowserProxy();
    BrowserProxy.setInstance(testProxy);
  });

  async function setupUninstallButton(app: App) {
    uninstallButton = document.createElement('app-management-uninstall-button');
    uninstallButton.app = app;
    document.body.appendChild(uninstallButton);
    await flushTasks();
  }

  test('Click uninstall', async () => {
    const app: App = createTestApp();
    app.id = 'some test app id';
    app.installReason = InstallReason.kUser;
    setupUninstallButton(app);

    const clickable = uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton');
    assertTrue(!!clickable);
    clickable.click();

    assertEquals(
        await testProxy.handler.whenCalled('uninstall'), 'some test app id');
  });

  test('Disabled by policy', async () => {
    const app: App = createTestApp();
    app.installReason = InstallReason.kPolicy;

    await setupUninstallButton(app);
    const clickable = uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton');
    assertTrue(!!clickable);
    clickable.click();

    // Disabled by policy, clicking should not remove app.
    assertEquals(testProxy.handler.getCallCount('uninstall'), 0);
  });

  test('System app, button hidden', async () => {
    const app: App = createTestApp();
    app.installReason = InstallReason.kSystem;
    await setupUninstallButton(app);

    assertFalse(!!uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton'));
  });

  test('User can delete app installed from command line argument', async () => {
    const app: App = createTestApp();
    app.id = 'test id for command line app';
    app.installReason = InstallReason.kCommandLine;
    await setupUninstallButton(app);

    uninstallButton.shadowRoot!.querySelector<HTMLElement>(
                                   '#uninstallButton')!.click();

    assertEquals(
        await testProxy.handler.whenCalled('uninstall'),
        'test id for command line app');
  });
});
