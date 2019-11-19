// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_reset_page', function() {
  /** @enum {string} */
  const TestNames = {
    PowerwashDialogAction: 'PowerwashDialogAction',
    PowerwashDialogOpenClose: 'PowerwashDialogOpenClose',
  };

  function registerDialogTests() {
    suite('DialogTests', function() {
      let resetPage = null;

      /** @type {!settings.ResetPageBrowserProxy} */
      let resetPageBrowserProxy = null;

      /** @type {!settings.LifetimeBrowserProxy} */
      let lifetimeBrowserProxy = null;

      setup(function() {
        lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
        settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

        resetPageBrowserProxy = new reset_page.TestOsResetBrowserProxy();
        settings.OsResetBrowserProxyImpl.instance_ = resetPageBrowserProxy;

        PolymerTest.clearBody();
        resetPage = document.createElement('os-settings-reset-page');
        document.body.appendChild(resetPage);
        Polymer.dom.flush();
      });

      teardown(function() {
        resetPage.remove();
      });

      /**
       * @param {function(SettingsPowerwashDialogElement):!Element}
       *     closeButtonFn A function that returns the button to be used for
       *     closing the dialog.
       * @return {!Promise}
       */
      function testOpenClosePowerwashDialog(closeButtonFn) {
        // Open powerwash dialog.
        assertTrue(!!resetPage);
        resetPage.$.powerwash.click();
        Polymer.dom.flush();
        const dialog = resetPage.$$('os-settings-powerwash-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        const onDialogClosed = new Promise(function(resolve, reject) {
          dialog.addEventListener('close', function() {
            assertFalse(dialog.$.dialog.open);
            resolve();
          });
        });

        closeButtonFn(dialog).click();
        return Promise.all([
          onDialogClosed,
          resetPageBrowserProxy.whenCalled('onPowerwashDialogShow'),
        ]);
      }

      // Tests that the powerwash dialog opens and closes correctly, and
      // that chrome.send calls are propagated as expected.
      test(TestNames.PowerwashDialogOpenClose, function() {
        // Test case where the 'cancel' button is clicked.
        return testOpenClosePowerwashDialog(function(dialog) {
          return dialog.$.cancel;
        });
      });

      // Tests that when powerwash is requested chrome.send calls are
      // propagated as expected.
      test(TestNames.PowerwashDialogAction, async () => {
        // Open powerwash dialog.
        resetPage.$.powerwash.click();
        Polymer.dom.flush();
        const dialog = resetPage.$$('os-settings-powerwash-dialog');
        assertTrue(!!dialog);
        dialog.$.powerwash.click();
        const requestTpmFirmwareUpdate =
            await lifetimeBrowserProxy.whenCalled('factoryReset');
        assertFalse(requestTpmFirmwareUpdate);
      });
    });
  }

  registerDialogTests();
});
