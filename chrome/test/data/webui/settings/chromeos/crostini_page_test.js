// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

function setCrostiniPrefs(enabled, opt_sharedPaths, opt_sharedUsbDevices) {
  crostiniPage.prefs = {
    crostini: {
      enabled: {value: enabled},
    },
    guest_os: {
      paths_shared_to_vms: {value: opt_sharedPaths || {}},
    }
  };
  crostiniBrowserProxy.sharedUsbDevices = opt_sharedUsbDevices || [];
  Polymer.dom.flush();
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      crostiniPage.async(resolve);
    });
  }

  suite('Main Page', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));
      assertFalse(button.disabled);

      button.click();
      Polymer.dom.flush();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestCrostiniInstallerView'));
      setCrostiniPrefs(true);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });

    test('ButtonDisabledDuringInstall', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      return flushAsync()
          .then(() => {
            assertFalse(button.disabled);
            cr.webUIListenerCallback('crostini-installer-status-changed', true);
            flushAsync();
          })
          .then(() => {
            assertTrue(button.disabled);
            cr.webUIListenerCallback(
                'crostini-installer-status-changed', false);
            flushAsync();
          })
          .then(() => {
            assertFalse(button.disabled);
          });
    });
  });

  suite('SubPageDetails', function() {
    let subpage;

    /**
     * Returns a new promise that resolves after a window 'popstate' event.
     * @return {!Promise}
     */
    function whenPopState() {
      return new Promise(function(resolve) {
        window.addEventListener('popstate', function callback() {
          window.removeEventListener('popstate', callback);
          resolve();
        });
      });
    }

    setup(function() {
      setCrostiniPrefs(true);
      loadTimeData.overrideValues({
        showCrostiniExportImport: true,
      });

      const eventPromise = new Promise((resolve) => {
                             const v = cr.addWebUIListener(
                                 'crostini-installer-status-changed', () => {
                                   resolve(v);
                                 });
                           }).then((v) => {
        assertTrue(cr.removeWebUIListener(v));
      });

      settings.navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();

      const pageLoadPromise = flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-subpage');
        assertTrue(!!subpage);
      });

      return Promise.all([pageLoadPromise, eventPromise]);
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths'));
      assertTrue(!!subpage.$$('#crostini-shared-usb-devices'));
      assertTrue(!!subpage.$$('#crostini-export-import'));
      assertTrue(!!subpage.$$('#remove'));
    });

    test('SharedPaths', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths'));
      subpage.$$('#crostini-shared-paths').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-shared-paths');
        assertTrue(!!subpage);
      });
    });

    test('Export', function() {
      assertTrue(!!subpage.$$('#crostini-export-import'));
      subpage.$$('#crostini-export-import').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-export-import');
        assertTrue(!!subpage.$$('#export cr-button'));
        subpage.$$('#export cr-button').click();
        assertEquals(
            1, crostiniBrowserProxy.getCallCount('exportCrostiniContainer'));
      });
    });

    test('Import', function() {
      assertTrue(!!subpage.$$('#crostini-export-import'));
      subpage.$$('#crostini-export-import').click();
      return flushAsync()
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-export-import');
            subpage.$$('#import cr-button').click();
            return flushAsync();
          })
          .then(() => {
            subpage =
                subpage.$$('settings-crostini-import-confirmation-dialog');
            subpage.$$('cr-dialog cr-button[id="continue"]').click();
            assertEquals(
                1,
                crostiniBrowserProxy.getCallCount('importCrostiniContainer'));
          });
    });

    test('ExportImportButtonsGetDisabledOnOperationStatus', function() {
      assertTrue(!!subpage.$$('#crostini-export-import'));
      subpage.$$('#crostini-export-import').click();
      return flushAsync()
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback(
                'crostini-export-import-operation-status-changed', true);
            return flushAsync();
          })
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertTrue(subpage.$$('#export cr-button').disabled);
            assertTrue(subpage.$$('#import cr-button').disabled);
            cr.webUIListenerCallback(
                'crostini-export-import-operation-status-changed', false);
            return flushAsync();
          })
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-export-import');
            assertFalse(subpage.$$('#export cr-button').disabled);
            assertFalse(subpage.$$('#import cr-button').disabled);
          });
    });

    test('Remove', function() {
      assertTrue(!!subpage.$$('#remove cr-button'));
      subpage.$$('#remove cr-button').click();
      assertEquals(
          1, crostiniBrowserProxy.getCallCount('requestRemoveCrostini'));
      setCrostiniPrefs(false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });
    });

    test('RemoveHidden', function() {
      // Elements are not destroyed when a dom-if stops being shown, but we can
      // check if their rendered width is non-zero. This should be resilient
      // against most formatting changes, since we're not relying on them having
      // any exact size, or on Polymer using any particular means of hiding
      // elements.
      assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
      cr.webUIListenerCallback('crostini-installer-status-changed', true);
      return flushAsync().then(() => {
        assertFalse(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        cr.webUIListenerCallback('crostini-installer-status-changed', false);
        return flushAsync().then(() => {
          assertTrue(!!subpage.shadowRoot.querySelector('#remove').clientWidth);
        });
      });
    });

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.CROSTINI_DETAILS);
      setCrostiniPrefs(false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
      });
    });
  });

  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(function() {
      setCrostiniPrefs(true, {'path1': ['termina'], 'path2': ['termina']});
      return flushAsync().then(() => {
        settings.navigateTo(settings.routes.CROSTINI_SHARED_PATHS);
        return flushAsync().then(() => {
          subpage = crostiniPage.$$('settings-crostini-shared-paths');
          assertTrue(!!subpage);
        });
      });
    });

    test('Sanity', function() {
      assertEquals(
          2, subpage.shadowRoot.querySelectorAll('.settings-box').length);
      assertEquals(2, subpage.shadowRoot.querySelectorAll('.list-item').length);
    });

    test('Remove', function() {
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);
      assertTrue(!!subpage.$$('.list-item cr-icon-button'));
      // Remove first shared path, still one left.
      subpage.$$('.list-item cr-icon-button').click();
      return crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath')
          .then(([vmName, path]) => {
            assertEquals('termina', vmName);
            assertEquals('path1', path);
            setCrostiniPrefs(true, {'path2': ['termina']});
            return flushAsync();
          })
          .then(() => {
            Polymer.dom.flush();
            assertEquals(
                1, subpage.shadowRoot.querySelectorAll('.list-item').length);
            assertFalse(subpage.$.crostiniInstructionsRemove.hidden);

            // Remove remaining shared path, none left.
            crostiniBrowserProxy.resetResolver('removeCrostiniSharedPath');
            subpage.$$('.list-item cr-icon-button').click();
            return crostiniBrowserProxy.whenCalled('removeCrostiniSharedPath');
          })
          .then(([vmName, path]) => {
            assertEquals('termina', vmName);
            assertEquals('path2', path);
            setCrostiniPrefs(true, {});
            return flushAsync();
          })
          .then(() => {
            Polymer.dom.flush();
            assertEquals(
                0, subpage.shadowRoot.querySelectorAll('.list-item').length);
            // Verify remove instructions are hidden.
            assertTrue(subpage.$.crostiniInstructionsRemove.hidden);
          });
    });
  });

  suite('SubPageSharedUsbDevices', function() {
    let subpage;

    setup(function() {
      setCrostiniPrefs(true, {}, [
        {'shared': true, 'guid': '0001', 'name': 'usb_dev1'},
        {'shared': false, 'guid': '0002', 'name': 'usb_dev2'},
        {'shared': true, 'guid': '0003', 'name': 'usb_dev3'}
      ]);

      return flushAsync()
          .then(() => {
            settings.navigateTo(settings.routes.CROSTINI_SHARED_USB_DEVICES);
            return flushAsync();
          })
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-shared-usb-devices');
            assertTrue(!!subpage);
          });
    });

    test('USB devices are shown', function() {
      assertEquals(3, subpage.shadowRoot.querySelectorAll('.toggle').length);
    });

    test('USB shared state is updated by toggling', function() {
      assertTrue(!!subpage.$$('.toggle'));
      subpage.$$('.toggle').click();
      return flushAsync()
          .then(() => {
            Polymer.dom.flush();
            return crostiniBrowserProxy.whenCalled(
                'setCrostiniUsbDeviceShared');
          })
          .then(args => {
            assertEquals('0001', args[0]);
            assertEquals(false, args[1]);

            // Simulate a change in the underlying model.
            cr.webUIListenerCallback('crostini-shared-usb-devices-changed', [
              {'shared': true, 'guid': '0001', 'name': 'usb_dev1'},
            ]);
            Polymer.dom.flush();
            assertEquals(
                1, subpage.shadowRoot.querySelectorAll('.toggle').length);
          });
    });
  });
});
