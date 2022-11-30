// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import 'chrome://os-settings/chromeos/os_settings.js';

import {AppManagementStore} from 'chrome://os-settings/chromeos/os_settings.js';
import {setupFakeHandler, replaceStore, replaceBody} from './test_util.js';

suite('<app-management-uninstall-button', () => {
  let uninstallButton;
  let fakeHandler;
  let app;


  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();
  });

  async function setupUninstallButton(installReason) {
    // Create an ARC app options.
    const arcOptions = {
      type: appManagement.mojom.AppType.kArc,
      installReason: installReason,
    };

    app = await fakeHandler.addApp('app1_id', arcOptions);
    await fakeHandler.flushPipesForTesting();

    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);

    uninstallButton = document.createElement('app-management-uninstall-button');
    uninstallButton.app = app;
    replaceBody(uninstallButton);
    await fakeHandler.flushPipesForTesting();
  }

  test('Click uninstall', async () => {
    await setupUninstallButton(appManagement.mojom.InstallReason.kUser);

    uninstallButton.shadowRoot.querySelector('#uninstallButton').click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(!!AppManagementStore.getInstance().data.apps[app.id]);
  });

  test('Disabled by policy', async () => {
    await setupUninstallButton(appManagement.mojom.InstallReason.kPolicy);
    uninstallButton.shadowRoot.querySelector('#uninstallButton').click();
    await fakeHandler.flushPipesForTesting();
    // Disabled by policy, clicking should not remove app.
    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);
  });

  test('System app, button hidden', async () => {
    await setupUninstallButton(appManagement.mojom.InstallReason.kSystem);
    assertFalse(!!uninstallButton.shadowRoot.querySelector('#uninstallButton'));
    await fakeHandler.flushPipesForTesting();
    // Disabled by policy, clicking should not remove app.
    assertTrue(!!AppManagementStore.getInstance().data.apps[app.id]);
  });
});
