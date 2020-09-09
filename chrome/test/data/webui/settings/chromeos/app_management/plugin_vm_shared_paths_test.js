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
  }

  /** override */
  getPluginVmSharedPathsDisplayText(paths) {
    this.methodCalled('getPluginVmSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** override */
  removePluginVmSharedPath(vmName, path) {
    this.methodCalled('removePluginVmSharedPath', [vmName, path]);
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
    assertEquals(2, page.shadowRoot.querySelectorAll('.list-item').length);
    assertFalse(page.$.pluginVmInstructionsRemove.hidden);
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
    assertEquals(1, page.shadowRoot.querySelectorAll('.list-item').length);
    assertFalse(page.$.pluginVmInstructionsRemove.hidden);

    // Remove remaining shared path, none left.
    pluginVmBrowserProxy.resetResolver('removePluginVmSharedPath');
    page.$$('.list-item cr-icon-button').click();
    {
      const [vmName, path] =
          await pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path2', path);
    }
    await setPrefs({'ignored': ['ignore']});
    assertEquals(0, page.shadowRoot.querySelectorAll('.list-item').length);
    // Verify remove instructions are hidden.
    assertTrue(page.$.pluginVmInstructionsRemove.hidden);
  });
});
