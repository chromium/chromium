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

/** @type {?TestPluginVmBrowserProxy} */
let pluginVmBrowserProxy = null;

suite('Details', function() {
  /** @type {?SettingsPluginVmSubpageElement} */
  let page = null;

  setup(function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    settings.PluginVmBrowserProxyImpl.instance_ = pluginVmBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-plugin-vm-subpage');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Sanity', function() {
    assertTrue(!!page.$$('#plugin-vm-shared-paths'));
  });
});

suite('SharedPaths', function() {
  /** @type {?SettingsPluginVmSharedPathsElement} */
  let page = null;

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

  test('Remove', function() {
    return setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']})
        .then(() => {
          assertEquals(
              2, page.shadowRoot.querySelectorAll('.settings-box').length);
          assertEquals(
              2, page.shadowRoot.querySelectorAll('.list-item').length);
          assertFalse(page.$.pluginVmInstructionsRemove.hidden);
          assertTrue(!!page.$$('.list-item cr-icon-button'));

          // Remove first shared path, still one left.
          page.$$('.list-item cr-icon-button').click();
          return pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
        })
        .then(([vmName, path]) => {
          assertEquals('PvmDefault', vmName);
          assertEquals('path1', path);
          return setPrefs({'path2': ['PvmDefault']});
        })
        .then(() => {
          assertEquals(
              1, page.shadowRoot.querySelectorAll('.list-item').length);
          assertFalse(page.$.pluginVmInstructionsRemove.hidden);

          // Remove remaining shared path, none left.
          pluginVmBrowserProxy.resetResolver('removePluginVmSharedPath');
          page.$$('.list-item cr-icon-button').click();
          return pluginVmBrowserProxy.whenCalled('removePluginVmSharedPath');
        })
        .then(([vmName, path]) => {
          assertEquals('PvmDefault', vmName);
          assertEquals('path2', path);
          return setPrefs({'ignored': ['ignore']});
        })
        .then(() => {
          assertEquals(
              0, page.shadowRoot.querySelectorAll('.list-item').length);
          // Verify remove instructions are hidden.
          assertTrue(page.$.pluginVmInstructionsRemove.hidden);
        });
  });
});
