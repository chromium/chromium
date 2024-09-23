// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for app-management-uninstall-button. */
import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {AppManagementUninstallButtonElement} from 'chrome://os-settings/lazy_load.js';
import {App, InstallReason} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {createApp, setupFakeHandler} from '../../app_management/test_util.js';
import {clearBody} from '../../utils.js';

suite('AppManagementUninstallButtonTest', () => {
  let uninstallButton: AppManagementUninstallButtonElement;
  let fakeHandler: FakePageHandler;

  setup(async function() {
    clearBody();
    fakeHandler = setupFakeHandler();
  });

  async function setupUninstallButton(app: App) {
    uninstallButton = document.createElement('app-management-uninstall-button');
    uninstallButton.app = app;
    document.body.appendChild(uninstallButton);
    await flushTasks();
  }

  test('Click uninstall', async () => {
    const app: App = createApp('some test app id');
    app.installReason = InstallReason.kUser;
    setupUninstallButton(app);

    const clickable = uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton');
    assertTrue(!!clickable);
    clickable.click();

    assertEquals(await fakeHandler.whenCalled('uninstall'), 'some test app id');
  });

  test('Disabled by policy', async () => {
    const app: App = createApp('app');
    app.installReason = InstallReason.kPolicy;

    await setupUninstallButton(app);
    const clickable = uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton');
    assertTrue(!!clickable);
    clickable.click();

    // Disabled by policy, clicking should not remove app.
    assertEquals(fakeHandler.getCallCount('uninstall'), 0);
  });

  test('Disallow uninstall, button hidden', async () => {
    const app: App = createApp('app');
    app.allowUninstall = false;
    await setupUninstallButton(app);

    assertFalse(!!uninstallButton.shadowRoot!.querySelector<HTMLElement>(
        '#uninstallButton'));
  });

  test('User can delete app installed from command line argument', async () => {
    const app: App = createApp('test id for command line app');
    app.installReason = InstallReason.kCommandLine;
    await setupUninstallButton(app);

    uninstallButton.shadowRoot!.querySelector<HTMLElement>(
                                   '#uninstallButton')!.click();

    assertEquals(
        await fakeHandler.whenCalled('uninstall'),
        'test id for command line app');
  });
});
