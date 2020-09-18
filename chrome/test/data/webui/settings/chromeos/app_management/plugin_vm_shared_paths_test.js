// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.PluginVmBrowserProxy} */
class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluginVmSharedPathsDisplayText',
      'removePluginVmSharedPath',
    ]);
    this.removeSharedPathResult = true;
  }

  /** override */
  getPluginVmSharedPathsDisplayText(paths) {
    this.methodCalled('getPluginVmSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** override */
  removePluginVmSharedPath(vmName, path) {
    this.methodCalled('removePluginVmSharedPath', [vmName, path]);
    return Promise.resolve(this.removeSharedPathResult);
  }
}

suite('SharedPaths', function() {
  /** @type {?SettingsPluginVmSharedPathsElement} */
  let page = null;

  /** @type {?TestPluginVmBrowserProxy} */
  let pluginVmBrowserProxy = null;

  function setPrefs(sharedPaths) {
    pluginVmBrowserProxy.resetResolver('getPluginVmSharedPathsDisplayText');
    page.prefs = {
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      }
    };
    return pluginVmBrowserProxy.whenCalled('getPluginVmSharedPathsDisplayText')
        .then(() => {
          Polymer.dom.flush();
        });
  }

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-shared-paths');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Remove', async function() {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});
    assertEquals(3, page.shadowRoot.querySelectorAll('.settings-box').length);
    const rows = '.list-item:not([hidden])';
    assertEquals(2, page.shadowRoot.querySelectorAll(rows).length);

    assertFalse(page.$.pluginVmInstructionsRemove.hidden);
    assertFalse(page.$.pluginVmList.hidden);
    assertTrue(page.$.pluginVmListEmpty.hidden);
    assertTrue(!!page.$$('.list-item cr-icon-button'));

    // Remove first shared path, still one left.
    page.$$('.list-item cr-icon-button').click();
    {
      const [vmName, path] =
          await pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path1', path);
    }
    await setPrefs({'path2': ['PvmDefault']});
    assertEquals(1, page.shadowRoot.querySelectorAll(rows).length);
    assertFalse(page.$.pluginVmInstructionsRemove.hidden);

    // Remove remaining shared path, none left.
    pluginVmBrowserProxy.resetResolver('removePluginVmSharedPath');
    page.$$(`${rows} cr-icon-button`).click();
    {
      const [vmName, path] =
          await pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path2', path);
    }
    await setPrefs({'ignored': ['ignore']});
    assertTrue(page.$.pluginVmList.hidden);
    // Verify remove instructions are hidden, and empty list message is shown.
    assertTrue(page.$.pluginVmInstructionsRemove.hidden);
    assertTrue(page.$.pluginVmList.hidden);
    assertFalse(page.$.pluginVmListEmpty.hidden);
  });

  test('RemoveFailedRetry', async function() {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});

    // Remove shared path fails.
    pluginVmBrowserProxy.removeSharedPathResult = false;
    page.$$('.list-item cr-icon-button').click();

    await pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
    Polymer.dom.flush();
    assertTrue(page.$$('#removeSharedPathFailedDialog').open);

    // Click retry and make sure 'removePluginVmSharedPath' is called
    // and dialog is closed/removed.
    pluginVmBrowserProxy.removeSharedPathResult = true;
    page.$$('#removeSharedPathFailedDialog')
        .querySelector('.action-button')
        .click();
    await pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
    assertFalse(!!page.$$('#removeSharedPathFailedDialog'));
  });
});
